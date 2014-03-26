/*
 *  This file is part of carbon-c-relay.
 *
 *  carbon-c-relay is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  carbon-c-relay is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with carbon-c-relay.  If not, see <http://www.gnu.org/licenses/>.
 */


#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <signal.h>
#include <time.h>
#include <errno.h>

#include "relay.h"
#include "consistent-hash.h"
#include "server.h"
#include "router.h"
#include "receptor.h"
#include "dispatcher.h"
#include "aggregator.h"
#include "collector.h"

int keep_running = 1;
char relay_hostname[128];

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
		fprintf(stdout, "caught %s, terminating...\n", signal);
		fflush(stdout);
	} else {
		fprintf(stderr, "caught %s while already shutting down, "
				"forcing exit...\n", signal);
		fflush(NULL);
		exit(1);
	}
	keep_running = 0;
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
	printf("Usage: relay [-vdst] -f <config> [-p <port>] [-w <workers>]\n");
	printf("\n");
	printf("Options:\n");
	printf("  -v  print version and exit\n");
	printf("  -f  read <config> for clusters and routes\n");
	printf("  -p  listen on <port> for connections, defaults to 2003\n");
	printf("  -w  user <workers> worker threads, defaults to 16\n");
	printf("  -d  debug mode: currently writes statistics to stdout\n");
	printf("  -s  submission mode: write info about errors to stdout\n");
	printf("  -t  config test mode: prints rule matches from input on stdin\n");

	exit(exitcode);
}

int
main(int argc, char * const argv[])
{
	int sock[] = {0, 0, 0};  /* IPv4, IPv6, UNIX */
	int socklen = sizeof(sock) / sizeof(sock[0]);
	char id;
	server **servers;
	dispatcher **workers;
	char workercnt = 0;
	char *routes = NULL;
	unsigned short listenport = 2003;
	enum rmode mode = NORMAL;
	int ch;
	char nowbuf[24];
	size_t numaggregators;
	size_t numcomputes;
	server *internal_submission;

	while ((ch = getopt(argc, argv, ":hvdstf:p:w:")) != -1) {
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


	if (gethostname(relay_hostname, sizeof(relay_hostname)) < 0)
		snprintf(relay_hostname, sizeof(relay_hostname), "127.0.0.1");

	/* seed randomiser for dispatcher and aggregator "splay" */
	srand(time(NULL));

	if (workercnt == 0)
		workercnt = mode == SUBMISSION ? 2 : 16;

	fprintf(stdout, "[%s] starting carbon-c-relay v%s (%s)\n",
		fmtnow(nowbuf), VERSION, GIT_VERSION);
	fprintf(stdout, "configuration:\n");
	fprintf(stdout, "    relay hostname = %s\n", relay_hostname);
	fprintf(stdout, "    listen port = %u\n", listenport);
	fprintf(stdout, "    workers = %d\n", workercnt);
	if (mode == DEBUG)
		fprintf(stdout, "    debug = true\n");
	else if (mode == SUBMISSION)
		fprintf(stdout, "    submission = true\n");
	fprintf(stdout, "    routes configuration = %s\n", routes);
	fprintf(stdout, "\n");
	if (router_readconfig(routes) == 0) {
		fprintf(stderr, "failed to read configuration '%s'\n", routes);
		return 1;
	}
	router_optimise();
	numaggregators = aggregator_numaggregators();
	numcomputes = aggregator_numcomputes();
	if (numaggregators > 10) {
		fprintf(stdout, "parsed configuration follows:\n"
				"(%zd aggregations with %zd computations omitted "
				"for brevity)\n", numaggregators, numcomputes);
		router_printconfig(stdout, 0);
	} else {
		fprintf(stdout, "parsed configuration follows:\n");
		router_printconfig(stdout, 1);
	}
	fprintf(stdout, "\n");

	/* shortcut for rule testing mode */
	if (mode == TEST) {
		char metricbuf[8096];
		char *p;

		fflush(stdout);
		while (fgets(metricbuf, sizeof(metricbuf), stdin) != NULL) {
			if ((p = strchr(metricbuf, '\n')) != NULL)
				*p = '\0';
			router_test(metricbuf);
		}

		exit(0);
	}

	if (signal(SIGINT, exit_handler) == SIG_ERR) {
		fprintf(stderr, "failed to create SIGINT handler: %s\n",
				strerror(errno));
		return 1;
	}
	if (signal(SIGTERM, exit_handler) == SIG_ERR) {
		fprintf(stderr, "failed to create SIGTERM handler: %s\n",
				strerror(errno));
		return 1;
	}
	if (signal(SIGQUIT, exit_handler) == SIG_ERR) {
		fprintf(stderr, "failed to create SIGQUIT handler: %s\n",
				strerror(errno));
		return 1;
	}
	if (signal(SIGPIPE, SIG_IGN) == SIG_ERR) {
		fprintf(stderr, "failed to ignore SIGPIPE: %s\n",
				strerror(errno));
		return 1;
	}

	workers = malloc(sizeof(dispatcher *) * (1 + workercnt + 1));
	if (workers == NULL) {
		fprintf(stderr, "failed to allocate memory for workers\n");
		return 1;
	}

	if (bindlisten(sock, &socklen, listenport) < 0) {
		fprintf(stderr, "failed to bind on port %d: %s\n",
				listenport, strerror(errno));
		return -1;
	}
	for (ch = 0; ch < socklen; ch++) {
		if (dispatch_addlistener(sock[ch]) != 0) {
			fprintf(stderr, "failed to add listener\n");
			return -1;
		}
	}
	fprintf(stdout, "listening on port %u\n", listenport);
	if ((workers[0] = dispatch_new_listener()) == NULL)
		fprintf(stderr, "failed to add listener\n");

	fprintf(stdout, "starting %d workers\n", workercnt);
	for (id = 1; id < 1 + workercnt; id++) {
		workers[id + 0] = dispatch_new_connection();
		if (workers[id + 0] == NULL) {
			fprintf(stderr, "failed to add worker %d\n", id);
			break;
		}
	}
	workers[id + 0] = NULL;
	if (id < 1 + workercnt) {
		fprintf(stderr, "shutting down due to errors\n");
		keep_running = 0;
	}

	servers = server_get_servers();

	/* server used for delivering metrics produced inside the relay,
	 * that is collector (statistics) and aggregator (aggregations) */
	if ((internal_submission = server_new_qsize("internal", listenport,
					3000 + (numcomputes * 3))) == NULL)
	{
		fprintf(stderr, "failed to create internal submission queue, shutting down\n");
		keep_running = 0;
	}

	if (numaggregators > 0) {
		fprintf(stdout, "starting aggregator\n");
		if (!aggregator_start(internal_submission)) {
			fprintf(stderr, "shutting down due to failure to start aggregator\n");
			keep_running = 0;
		}
	}

	fprintf(stdout, "starting statistics collector\n");
	collector_start((void **)&workers[1], (void **)servers, mode,
			internal_submission);

	fflush(stdout);  /* ensure all info stuff up here is out of the door */

	/* workers do the work, just wait */
	while (keep_running)
		sleep(1);

	fprintf(stdout, "[%s] shutting down...\n", fmtnow(nowbuf));
	fflush(stdout);
	/* make sure we don't accept anything new anymore */
	for (ch = 0; ch < socklen; ch++)
		dispatch_removelistener(sock[ch]);
	destroy_usock(listenport);
	fprintf(stdout, "[%s] listener for port %u closed\n",
			fmtnow(nowbuf), listenport);
	fflush(stdout);
	/* since workers will be freed, stop querying the structures */
	collector_stop();
	fprintf(stdout, "[%s] collector stopped\n", fmtnow(nowbuf));
	fflush(stdout);
	if (numaggregators > 0) {
		aggregator_stop();
		fprintf(stdout, "[%s] aggregator stopped\n", fmtnow(nowbuf));
		fflush(stdout);
	}
	server_shutdown(internal_submission);
	/* give a little time for whatever the collector/aggregator wrote,
	 * to be delivered by the dispatchers */
	usleep(500 * 1000);  /* 500ms */
	/* make sure we don't write to our servers any more */
	fprintf(stdout, "[%s] stopped worker", fmtnow(nowbuf));
	fflush(stdout);
	for (id = 0; id < 1 + workercnt; id++)
		dispatch_stop(workers[id + 0]);
	for (id = 0; id < 1 + workercnt; id++) {
		dispatch_shutdown(workers[id + 0]);
		fprintf(stdout, " %d", id + 1);
		fflush(stdout);
	}
	fprintf(stdout, " (%s)\n", fmtnow(nowbuf));
	fflush(stdout);
	router_shutdown();
	server_shutdown_all();
	fprintf(stdout, "[%s] routing stopped\n", fmtnow(nowbuf));
	fflush(stdout);

	fflush(stderr);  /* ensure all of our termination messages are out */

	free(workers);
	free(servers);
	return 0;
}
