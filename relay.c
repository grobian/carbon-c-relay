/*
 * Copyright 2013-2014 Fabian Groffen
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */


#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <signal.h>
#include <time.h>
#include <errno.h>
#if __STDC__VERSION__ >= 201112L && \
	((__GNUC__ > 4 || ( __GNU_C__ == 4 && __GNUC_MINOR__ != 8)) || __clang__)
#include <stdatomic.h>
#endif

#include "relay.h"
#include "consistent-hash.h"
#include "server.h"
#include "router.h"
#include "receptor.h"
#include "dispatcher.h"
#include "aggregator.h"
#include "collector.h"

int keep_running = 1;
char relay_hostname[256];
enum rmode mode = NORMAL;

FILE *outlog;
FILE *errlog;
char std_log_path[256];
char err_log_path[256];

static void exchange_logs(FILE **new_log, FILE **old_log) {
	FILE *tmp = NULL;
#if __STDC__VERSION__ >= 201112L && \
	((__GNUC__ > 4 || ( __GNU_C__ == 4 && __GNUC_MINOR__ != 8)) || __clang__)
	tmp = atomic_exchange(*new_log, *old_log);
#elif __GNUC__ && (__GNU_C__ > 4 || (__GNU_C__ == 4 && __GNUC_MINOR__ >= 8))
	tmp = __atomic_exchange_n(old_log, *new_log, __ATOMIC_SEQ_CST);
#elif __GNUC__
	tmp = __sync_lock_test_and_set(old_log, *new_log);
#else
#error "Unsupported compiler and std combination"
#endif
	*new_log = tmp;
}

static void
exit_handler(int sig)
{
	char *signal = "unknown signal";

	switch (sig) {
		case SIGTERM:
			signal = "SIGTERM";
			break;
		case SIGINT:
			signal = "SIGINT";
			break;
		case SIGQUIT:
			signal = "SIGQUIT";
			break;
	}
	if (keep_running) {
		fprintf(outlog, "caught %s, terminating...\n", signal);
		fflush(outlog);
	} else {
		fprintf(errlog, "caught %s while already shutting down, "
				"forcing exit...\n", signal);
		fflush(NULL);
		exit(1);
	}
	keep_running = 0;
}

static int
open_logs(FILE **new_outlog, FILE **new_errlog, char *std_log_path, char *err_log_path)
{
	fprintf(outlog, "Opening %s as new stdout log...\n", std_log_path);
	*new_outlog = fopen(std_log_path, "a+");
	if (!(*new_outlog)) {
		fprintf(errlog, "Can't reopen %s...\n", std_log_path);
		return -1;
	}

	fprintf(outlog, "Opening %s as new stderr log...\n", err_log_path);
	*new_errlog = fopen(err_log_path, "a+");
	if (!(*new_errlog)) {
		fprintf(errlog, "Can't reopen %s...\n", err_log_path);
		fclose(*new_outlog);
		return -2;
	}

	return 0;
}

static void
log_handler(int sig)
{
	FILE *new_outlog;
	FILE *new_errlog;
	char *signal = "unknown signal";

	switch (sig) {
		case SIGUSR2:
			signal = "SIGUSR2";
			break;
	}
	fprintf(outlog, "caught %s, reopening logs...\n", signal);

	if (open_logs(&new_outlog, &new_errlog, std_log_path, err_log_path) < 0) {
		return;
	}

	exchange_logs(&outlog, &new_outlog);
	exchange_logs(&errlog, &new_errlog);
	fflush(new_outlog);
	fclose(new_outlog);
	fflush(new_errlog);
	fclose(new_errlog);
}

char *
fmtnow(char nowbuf[24])
{
	time_t now;
	struct tm *tm_now;

	time(&now);
	tm_now = localtime(&now);
	strftime(nowbuf, 24, "%Y-%m-%d %H:%M:%S", tm_now);

	return nowbuf;
}

void
do_version(void)
{
	printf("carbon-c-relay v" VERSION " (" GIT_VERSION ")\n");

	exit(0);
}

void
do_usage(int exitcode)
{
	printf("Usage: relay [-vdst] -f <config> [-p <port>] [-w <workers>] [-b <size>] [-q <size>]\n");
	printf("\n");
	printf("Options:\n");
	printf("  -v  print version and exit\n");
	printf("  -f  read <config> for clusters and routes\n");
	printf("  -p  listen on <port> for connections, defaults to 2003\n");
	printf("  -i  listen on <interface> for connections, defaults to all\n");
	printf("  -w  use <workers> worker threads, defaults to 16\n");
	printf("  -b  server send batch size, defaults to 2500\n");
	printf("  -q  server queue size, defaults to 25000\n");
	printf("  -S  statistics sending interval in seconds, defaults to 60\n");
	printf("  -d  debug mode: currently writes statistics to outlog\n");
	printf("  -s  submission mode: write info about errors to outlog\n");
	printf("  -t  config test mode: prints rule matches from input on stdin\n");
	printf("  -H  hostname: override hostname (used in statistics)\n");
	printf("  -l  logfile: path of logfile (default: /dev/stdout)\n");
	printf("  -e  error logfile: path of error logfile (default: /dev/stderr)\n");

	exit(exitcode);
}

int
main(int argc, char * const argv[])
{
	int stream_sock[] = {0, 0, 0};  /* tcp4, tcp6, UNIX */
	int stream_socklen = sizeof(stream_sock) / sizeof(stream_sock[0]);
	int dgram_sock[] = {0, 0};  /* udp4, udp6 */
	int dgram_socklen = sizeof(dgram_sock) / sizeof(dgram_sock[0]);
	char id;
	server **servers;
	dispatcher **workers;
	char workercnt = 0;
	char *routes = NULL;
	unsigned short listenport = 2003;
	int batchsize = 2500;
	int queuesize = 25000;
	int ch;
	char nowbuf[24];
	size_t numaggregators;
	size_t numcomputes;
	server *internal_submission;
	char *listeninterface = NULL;
	outlog = stdout;
	errlog = stderr;

	if (gethostname(relay_hostname, sizeof(relay_hostname)) < 0)
		snprintf(relay_hostname, sizeof(relay_hostname), "127.0.0.1");

	while ((ch = getopt(argc, argv, ":hvdstf:i:p:w:b:q:S:H:l:e:")) != -1) {
		switch (ch) {
			case 'v':
				do_version();
				break;
			case 'd':
				mode = DEBUG;
				break;
			case 's':
				mode = SUBMISSION;
				break;
			case 't':
				mode = TEST;
				break;
			case 'f':
				routes = optarg;
				break;
			case 'i':
				listeninterface = optarg;
				break;
			case 'p':
				listenport = (unsigned short)atoi(optarg);
				if (listenport == 0) {
					fprintf(errlog, "error: port needs to be a number >0\n");
					do_usage(1);
				}
				break;
			case 'w':
				workercnt = (char)atoi(optarg);
				if (workercnt <= 0) {
					fprintf(errlog, "error: workers needs to be a number >0\n");
					do_usage(1);
				}
				break;
			case 'b':
				batchsize = atoi(optarg);
				if (batchsize <= 0) {
					fprintf(errlog, "error: batch size needs to be a number >0\n");
					do_usage(1);
				}
				break;
			case 'q':
				queuesize = atoi(optarg);
				if (queuesize <= 0) {
					fprintf(errlog, "error: queue size needs to be a number >0\n");
					do_usage(1);
				}
				break;
			case 'S':
				collector_interval = atoi(optarg);
				if (collector_interval <= 0) {
					fprintf(errlog, "error: sending interval needs to be "
							"a number >0\n");
					do_usage(1);
				}
				break;
			case 'H':
				snprintf(relay_hostname, sizeof(relay_hostname), "%s", optarg);
				break;
			case 'l':
				snprintf(std_log_path, sizeof(std_log_path), "%s", optarg);
				break;
			case 'e':
				snprintf(err_log_path, sizeof(err_log_path), "%s", optarg);
				break;
			case '?':
			case ':':
				do_usage(1);
				break;
			case 'h':
			default:
				do_usage(0);
				break;
		}
	}
	if (optind == 1 || routes == NULL)
		do_usage(1);

	if (strlen(std_log_path) == 0) {
		strncpy(std_log_path, "/dev/stdout\0", 256);
	}

	if (strlen(err_log_path) == 0) {
		strncpy(err_log_path, "/dev/stderr\0", 256);
	}

	open_logs(&outlog, &errlog, std_log_path, err_log_path);

	/* seed randomiser for dispatcher and aggregator "splay" */
	srand(time(NULL));

	if (workercnt == 0)
		workercnt = mode == SUBMISSION ? 2 : 16;

	/* any_of failover maths need batchsize to be smaller than queuesize */
	if (batchsize > queuesize) {
		fprintf(errlog, "error: batchsize must be smaller than queuesize\n");
		exit(-1);
	}

	fprintf(outlog, "[%s] starting carbon-c-relay v%s (%s)\n",
		fmtnow(nowbuf), VERSION, GIT_VERSION);
	fprintf(outlog, "configuration:\n");
	fprintf(outlog, "    relay hostname = %s\n", relay_hostname);
	fprintf(outlog, "    listen port = %u\n", listenport);
	if (listeninterface != NULL)
		fprintf(outlog, "    listen interface = %s\n", listeninterface);
	fprintf(outlog, "    workers = %d\n", workercnt);
	fprintf(outlog, "    send batch size = %d\n", batchsize);
	fprintf(outlog, "    server queue size = %d\n", queuesize);
	fprintf(outlog, "    statistics submission interval = %ds\n",
			collector_interval);
	if (mode == DEBUG)
		fprintf(outlog, "    debug = true\n");
	else if (mode == SUBMISSION)
		fprintf(outlog, "    submission = true\n");
	fprintf(outlog, "    routes configuration = %s\n", routes);
	fprintf(outlog, "\n");
	if (router_readconfig(routes, queuesize, batchsize) == 0) {
		fprintf(errlog, "failed to read configuration '%s'\n", routes);
		return 1;
	}
	router_optimise();
	numaggregators = aggregator_numaggregators();
	numcomputes = aggregator_numcomputes();
	if (numaggregators > 10) {
		fprintf(outlog, "parsed configuration follows:\n"
				"(%zd aggregations with %zd computations omitted "
				"for brevity)\n", numaggregators, numcomputes);
		router_printconfig(outlog, 0);
	} else {
		fprintf(outlog, "parsed configuration follows:\n");
		router_printconfig(outlog, 1);
	}
	fprintf(outlog, "\n");

	/* shortcut for rule testing mode */
	if (mode == TEST) {
		char metricbuf[METRIC_BUFSIZ];
		char *p;

		fflush(outlog);
		while (fgets(metricbuf, sizeof(metricbuf), stdin) != NULL) {
			if ((p = strchr(metricbuf, '\n')) != NULL)
				*p = '\0';
			router_test(metricbuf);
		}

		exit(0);
	}

	if (signal(SIGINT, exit_handler) == SIG_ERR) {
		fprintf(errlog, "failed to create SIGINT handler: %s\n",
				strerror(errno));
		return 1;
	}
	if (signal(SIGTERM, exit_handler) == SIG_ERR) {
		fprintf(errlog, "failed to create SIGTERM handler: %s\n",
				strerror(errno));
		return 1;
	}
	if (signal(SIGQUIT, exit_handler) == SIG_ERR) {
		fprintf(errlog, "failed to create SIGQUIT handler: %s\n",
				strerror(errno));
		return 1;
	}
	if (signal(SIGPIPE, SIG_IGN) == SIG_ERR) {
		fprintf(errlog, "failed to ignore SIGPIPE: %s\n",
				strerror(errno));
		return 1;
	}
	if (signal(SIGUSR2, log_handler) == SIG_ERR) {
		fprintf(errlog, "failed to create SIGUSR2 handler: %s\n",
				strerror(errno));
        }

	workers = malloc(sizeof(dispatcher *) * (1 + workercnt + 1));
	if (workers == NULL) {
		fprintf(errlog, "failed to allocate memory for workers\n");
		return 1;
	}

	if (bindlisten(stream_sock, &stream_socklen,
				dgram_sock, &dgram_socklen,
				listeninterface, listenport) < 0) {
		fprintf(errlog, "failed to bind on port %s:%d: %s\n",
				listeninterface == NULL ? "" : listeninterface,
				listenport, strerror(errno));
		return -1;
	}
	for (ch = 0; ch < stream_socklen; ch++) {
		if (dispatch_addlistener(stream_sock[ch]) != 0) {
			fprintf(errlog, "failed to add listener\n");
			return -1;
		}
	}
	for (ch = 0; ch < dgram_socklen; ch++) {
		if (dispatch_addlistener_udp(dgram_sock[ch]) != 0) {
			fprintf(errlog, "failed to listen to datagram socket\n");
			return -1;
		}
	}
	if ((workers[0] = dispatch_new_listener()) == NULL)
		fprintf(errlog, "failed to add listener\n");

	fprintf(outlog, "starting %d workers\n", workercnt);
	for (id = 1; id < 1 + workercnt; id++) {
		workers[id + 0] = dispatch_new_connection();
		if (workers[id + 0] == NULL) {
			fprintf(errlog, "failed to add worker %d\n", id);
			break;
		}
	}
	workers[id + 0] = NULL;
	if (id < 1 + workercnt) {
		fprintf(errlog, "shutting down due to errors\n");
		keep_running = 0;
	}

	servers = server_get_servers();

	/* server used for delivering metrics produced inside the relay,
	 * that is collector (statistics) and aggregator (aggregations) */
	if ((internal_submission = server_new("internal", listenport, CON_PIPE,
					NULL, 3000 + (numcomputes * 3), batchsize)) == NULL)
	{
		fprintf(errlog, "failed to create internal submission queue, shutting down\n");
		keep_running = 0;
	}

	if (numaggregators > 0) {
		fprintf(outlog, "starting aggregator\n");
		if (!aggregator_start(internal_submission)) {
			fprintf(errlog, "shutting down due to failure to start aggregator\n");
			keep_running = 0;
		}
	}

	fprintf(outlog, "starting statistics collector\n");
	collector_start((void **)&workers[1], (void **)servers,
			internal_submission);

	fflush(outlog);  /* ensure all info stuff up here is out of the door */

	/* workers do the work, just wait */
	while (keep_running)
		sleep(1);

	fprintf(outlog, "[%s] shutting down...\n", fmtnow(nowbuf));
	fflush(outlog);
	/* make sure we don't accept anything new anymore */
	for (ch = 0; ch < stream_socklen; ch++)
		dispatch_removelistener(stream_sock[ch]);
	destroy_usock(listenport);
	fprintf(outlog, "[%s] listener for port %u closed\n",
			fmtnow(nowbuf), listenport);
	fflush(outlog);
	/* since workers will be freed, stop querying the structures */
	collector_stop();
	fprintf(outlog, "[%s] collector stopped\n", fmtnow(nowbuf));
	fflush(outlog);
	if (numaggregators > 0) {
		aggregator_stop();
		fprintf(outlog, "[%s] aggregator stopped\n", fmtnow(nowbuf));
		fflush(outlog);
	}
	server_shutdown(internal_submission);
	/* give a little time for whatever the collector/aggregator wrote,
	 * to be delivered by the dispatchers */
	usleep(500 * 1000);  /* 500ms */
	/* make sure we don't write to our servers any more */
	fprintf(outlog, "[%s] stopped worker", fmtnow(nowbuf));
	fflush(outlog);
	for (id = 0; id < 1 + workercnt; id++)
		dispatch_stop(workers[id + 0]);
	for (id = 0; id < 1 + workercnt; id++) {
		dispatch_shutdown(workers[id + 0]);
		fprintf(outlog, " %d", id + 1);
		fflush(outlog);
	}
	fprintf(outlog, " (%s)\n", fmtnow(nowbuf));
	fflush(outlog);
	router_shutdown();
	server_shutdown_all();
	fprintf(outlog, "[%s] routing stopped\n", fmtnow(nowbuf));
	fflush(outlog);

	fflush(errlog);  /* ensure all of our termination messages are out */

	free(workers);
	free(servers);
	return 0;
}
