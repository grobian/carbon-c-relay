/*
 * Copyright 2013-2015 Fabian Groffen
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
#include <stdarg.h>
#include <unistd.h>
#include <string.h>
#include <signal.h>
#include <time.h>
#include <errno.h>
#include <assert.h>

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

static char *config = NULL;
static int batchsize = 2500;
static int queuesize = 25000;
static dispatcher **workers = NULL;
static char workercnt = 0;
static cluster *clusters = NULL;
static route *routes = NULL;
static char *relay_logfile = NULL;
static FILE *relay_stdout = NULL;
static FILE *relay_stderr = NULL;
static char relay_can_log = 0;


/**
 * Writes to the setup output stream, prefixed with a timestamp, and if
 * the stream is not going to stdout or stderr, prefixed with MSG or
 * ERR.
 */
int
relaylog(enum logdst dest, const char *fmt, ...)
{
	va_list ap;
	char prefix[64];
	size_t len;
	time_t now;
	struct tm *tm_now;
	FILE *dst = NULL;
	char console = 0;
	int ret;

	switch (dest) {
		case LOGOUT:
			dst = relay_stdout;
			if (dst == stdout)
				console = 1;
			break;
		case LOGERR:
			dst = relay_stderr;
			if (dst == stderr)
				console = 1;
			break;
	}
	assert(dst != NULL);

	/* briefly stall if we're swapping fds */
	while (!relay_can_log)
		usleep((100 + (rand() % 200)) * 1000);  /* 100ms - 300ms */

	time(&now);
	tm_now = localtime(&now);
	len = strftime(prefix, sizeof(prefix), "[%Y-%m-%d %H:%M:%S]", tm_now);

	if (!console)
		len += snprintf(prefix + len, sizeof(prefix) - len, " (%s)", dest == LOGOUT ? "MSG" : "ERR");

	fprintf(dst, "%s ", prefix);

	va_start(ap, fmt);

	ret = vfprintf(dst, fmt, ap);
	fflush(dst);

	va_end(ap);

	return ret;
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
		logout("caught %s, terminating...\n", signal);
	} else {
		logerr("caught %s while already shutting down, "
				"forcing exit...\n", signal);
		exit(1);
	}
	keep_running = 0;
}

static void
hup_handler(int sig)
{
	route *newroutes;
	cluster *newclusters;
	int id;
	FILE *newfd;

	logout("caught SIGHUP...\n");
	if (relay_stderr != stderr) {
		/* try to re-open the file first, so we can still try and say
		 * something if that fails */
		if ((newfd = fopen(relay_logfile, "a")) == NULL) {
			logerr("not reopening logfiles: can't open '%s': %s\n",
					relay_logfile, strerror(errno));
		} else {
			logout("closing logfile\n");
			relay_can_log = 0;
			fclose(relay_stderr);
			relay_stdout = newfd;
			relay_stderr = newfd;
			relay_can_log = 1;
			logout("reopening logfile\n");
		}
	}
	logout("reloading config from '%s'...\n", config);

	if (router_readconfig(&newclusters, &newroutes,
				config, queuesize, batchsize) == 0)
	{
		logerr("failed to read configuration '%s', aborting reload\n",
				config);
		return;
	}
	router_optimise(&newroutes);

	logout("reloading worker");
	for (id = 1; id < 1 + workercnt; id++)
		dispatch_schedulereload(workers[id + 0], newroutes);
	for (id = 1; id < 1 + workercnt; id++) {
		while (!dispatch_reloadcomplete(workers[id + 0]))
			usleep((100 + (rand() % 200)) * 1000);  /* 100ms - 300ms */
		fprintf(relay_stdout, " %d", id + 1);
		fflush(relay_stdout);
	}
	fprintf(relay_stdout, "\n");

	logout("reloading collector\n");
	collector_schedulereload(newclusters);
	while (!collector_reloadcomplete())
		usleep((100 + (rand() % 200)) * 1000);  /* 100ms - 300ms */

	router_free(clusters, routes);

	routes = newroutes;
	clusters = newclusters;

	logout("SIGHUP handler complete\n");
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
	printf("  -l  write output to <file>, defaults to stdout/stderr\n");
	printf("  -w  use <workers> worker threads, defaults to 16\n");
	printf("  -b  server send batch size, defaults to 2500\n");
	printf("  -q  server queue size, defaults to 25000\n");
	printf("  -S  statistics sending interval in seconds, defaults to 60\n");
	printf("  -d  debug mode: currently writes statistics to log\n");
	printf("  -s  submission mode: write info about errors to log\n");
	printf("  -t  config test mode: prints rule matches from input on stdin\n");
	printf("  -H  hostname: override hostname (used in statistics)\n");

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
	unsigned short listenport = 2003;
	int ch;
	size_t numaggregators;
	size_t numcomputes;
	server *internal_submission;
	char *listeninterface = NULL;
	server **servers;
	int i;

	if (gethostname(relay_hostname, sizeof(relay_hostname)) < 0)
		snprintf(relay_hostname, sizeof(relay_hostname), "127.0.0.1");

	while ((ch = getopt(argc, argv, ":hvdstf:i:l:p:w:b:q:S:H:")) != -1) {
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
				config = optarg;
				break;
			case 'i':
				listeninterface = optarg;
				break;
			case 'l':
				relay_logfile = optarg;
				break;
			case 'p':
				listenport = (unsigned short)atoi(optarg);
				if (listenport == 0) {
					fprintf(stderr, "error: port needs to be a number >0\n");
					do_usage(1);
				}
				break;
			case 'w':
				workercnt = (char)atoi(optarg);
				if (workercnt <= 0) {
					fprintf(stderr, "error: workers needs to be a number >0\n");
					do_usage(1);
				}
				break;
			case 'b':
				batchsize = atoi(optarg);
				if (batchsize <= 0) {
					fprintf(stderr, "error: batch size needs to be a number >0\n");
					do_usage(1);
				}
				break;
			case 'q':
				queuesize = atoi(optarg);
				if (queuesize <= 0) {
					fprintf(stderr, "error: queue size needs to be a number >0\n");
					do_usage(1);
				}
				break;
			case 'S':
				collector_interval = atoi(optarg);
				if (collector_interval <= 0) {
					fprintf(stderr, "error: sending interval needs to be "
							"a number >0\n");
					do_usage(1);
				}
				break;
			case 'H':
				snprintf(relay_hostname, sizeof(relay_hostname), "%s", optarg);
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
	if (optind == 1 || config == NULL)
		do_usage(1);


	/* seed randomiser for dispatcher and aggregator "splay" */
	srand(time(NULL));

	if (workercnt == 0)
		workercnt = mode == SUBMISSION ? 2 : 16;

	/* any_of failover maths need batchsize to be smaller than queuesize */
	if (batchsize > queuesize) {
		fprintf(stderr, "error: batchsize must be smaller than queuesize\n");
		exit(-1);
	}

	if (relay_logfile != NULL && mode != TEST) {
		FILE *f = fopen(relay_logfile, "a");
		if (f == NULL) {
			fprintf(stderr, "error: failed to open logfile '%s': %s\n",
					relay_logfile, strerror(errno));
			exit(-1);
		}
		relay_stdout = f;
		relay_stderr = f;
	} else {
		relay_stdout = stdout;
		relay_stderr = stderr;
	}
	relay_can_log = 1;

	logout("starting carbon-c-relay v%s (%s), pid=%d\n",
			VERSION, GIT_VERSION, getpid());
	fprintf(relay_stdout, "configuration:\n");
	fprintf(relay_stdout, "    relay hostname = %s\n", relay_hostname);
	fprintf(relay_stdout, "    listen port = %u\n", listenport);
	if (listeninterface != NULL)
		fprintf(relay_stdout, "    listen interface = %s\n", listeninterface);
	fprintf(relay_stdout, "    workers = %d\n", workercnt);
	fprintf(relay_stdout, "    send batch size = %d\n", batchsize);
	fprintf(relay_stdout, "    server queue size = %d\n", queuesize);
	fprintf(relay_stdout, "    statistics submission interval = %ds\n",
			collector_interval);
	if (mode == DEBUG)
		fprintf(relay_stdout, "    debug = true\n");
	else if (mode == SUBMISSION)
		fprintf(relay_stdout, "    submission = true\n");
	fprintf(relay_stdout, "    routes configuration = %s\n", config);
	fprintf(relay_stdout, "\n");

	if (router_readconfig(&clusters, &routes,
				config, queuesize, batchsize) == 0)
	{
		logerr("failed to read configuration '%s'\n", config);
		return 1;
	}
	router_optimise(&routes);

	numaggregators = aggregator_numaggregators();
	numcomputes = aggregator_numcomputes();
	if (numaggregators > 10) {
		fprintf(relay_stdout, "parsed configuration follows:\n"
				"(%zd aggregations with %zd computations omitted "
				"for brevity)\n", numaggregators, numcomputes);
		router_printconfig(relay_stdout, 0, clusters, routes);
	} else {
		fprintf(relay_stdout, "parsed configuration follows:\n");
		router_printconfig(relay_stdout, 1, clusters, routes);
	}
	fprintf(relay_stdout, "\n");

	/* shortcut for rule testing mode */
	if (mode == TEST) {
		char metricbuf[METRIC_BUFSIZ];
		char *p;

		fflush(relay_stdout);
		while (fgets(metricbuf, sizeof(metricbuf), stdin) != NULL) {
			if ((p = strchr(metricbuf, '\n')) != NULL)
				*p = '\0';
			router_test(metricbuf, routes);
		}

		exit(0);
	}

	if (signal(SIGINT, exit_handler) == SIG_ERR) {
		logerr("failed to create SIGINT handler: %s\n", strerror(errno));
		return 1;
	}
	if (signal(SIGTERM, exit_handler) == SIG_ERR) {
		logerr("failed to create SIGTERM handler: %s\n", strerror(errno));
		return 1;
	}
	if (signal(SIGQUIT, exit_handler) == SIG_ERR) {
		logerr("failed to create SIGQUIT handler: %s\n", strerror(errno));
		return 1;
	}
	if (signal(SIGHUP, hup_handler) == SIG_ERR) {
		logerr("failed to create SIGHUP handler: %s\n", strerror(errno));
		return 1;
	}
	if (signal(SIGPIPE, SIG_IGN) == SIG_ERR) {
		logerr("failed to ignore SIGPIPE: %s\n", strerror(errno));
		return 1;
	}

	workers = malloc(sizeof(dispatcher *) * (1 + workercnt + 1));
	if (workers == NULL) {
		logerr("failed to allocate memory for workers\n");
		return 1;
	}

	if (bindlisten(stream_sock, &stream_socklen,
				dgram_sock, &dgram_socklen,
				listeninterface, listenport) < 0) {
		logerr("failed to bind on port %s:%d: %s\n",
				listeninterface == NULL ? "" : listeninterface,
				listenport, strerror(errno));
		return -1;
	}
	for (ch = 0; ch < stream_socklen; ch++) {
		if (dispatch_addlistener(stream_sock[ch]) != 0) {
			logerr("failed to add listener\n");
			return -1;
		}
	}
	for (ch = 0; ch < dgram_socklen; ch++) {
		if (dispatch_addlistener_udp(dgram_sock[ch]) != 0) {
			logerr("failed to listen to datagram socket\n");
			return -1;
		}
	}
	if ((workers[0] = dispatch_new_listener()) == NULL)
		logerr("failed to add listener\n");

	logout("starting %d workers\n", workercnt);
	for (id = 1; id < 1 + workercnt; id++) {
		workers[id + 0] = dispatch_new_connection(routes);
		if (workers[id + 0] == NULL) {
			logerr("failed to add worker %d\n", id);
			break;
		}
	}
	workers[id + 0] = NULL;
	if (id < 1 + workercnt) {
		logerr("shutting down due to errors\n");
		keep_running = 0;
	}

	/* server used for delivering metrics produced inside the relay,
	 * that is collector (statistics) and aggregator (aggregations) */
	if ((internal_submission = server_new("internal", listenport, CON_PIPE,
					NULL, 3000 + (numcomputes * 3), batchsize)) == NULL)
	{
		logerr("failed to create internal submission queue, shutting down\n");
		keep_running = 0;
	}

	if (numaggregators > 0) {
		logout("starting aggregator\n");
		if (!aggregator_start(internal_submission)) {
			logerr("shutting down due to failure to start aggregator\n");
			keep_running = 0;
		}
	}

	logout("starting statistics collector\n");
	collector_start(&workers[1], clusters, internal_submission);

	logout("startup sequence complete\n");

	/* workers do the work, just wait */
	while (keep_running)
		sleep(1);

	logout("shutting down...\n");
	/* make sure we don't accept anything new anymore */
	for (ch = 0; ch < stream_socklen; ch++)
		dispatch_removelistener(stream_sock[ch]);
	destroy_usock(listenport);
	logout("listeners for port %u closed\n", listenport);
	/* since workers will be freed, stop querying the structures */
	collector_stop();
	logout("collector stopped\n");
	if (numaggregators > 0) {
		aggregator_stop();
		logout("aggregator stopped\n");
	}
	server_shutdown(internal_submission);
	/* give a little time for whatever the collector/aggregator wrote,
	 * to be delivered by the dispatchers */
	usleep(500 * 1000);  /* 500ms */
	/* make sure we don't write to our servers any more */
	logout("stopped worker");
	for (id = 0; id < 1 + workercnt; id++)
		dispatch_stop(workers[id + 0]);
	for (id = 0; id < 1 + workercnt; id++) {
		dispatch_shutdown(workers[id + 0]);
		fprintf(relay_stdout, " %d", id + 1);
		fflush(relay_stdout);
	}
	fprintf(relay_stdout, "\n");
	router_shutdown();
	servers = router_getservers(clusters);
	logout("stopped server");
	for (i = 0; servers[i] != NULL; i++)
		server_stop(servers[i]);
	for (i = 0; servers[i] != NULL; i++) {
		server_shutdown(servers[i]);
		fprintf(relay_stdout, " %d", i + 1);
		fflush(relay_stdout);
	}
	fprintf(relay_stdout, "\n");
	logout("routing stopped\n");

	router_free(clusters, routes);
	free(workers);
	return 0;
}
