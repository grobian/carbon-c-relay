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
#include <unistd.h>
#include <string.h>
#include <time.h>
#include <pthread.h>

#include "relay.h"
#include "dispatcher.h"
#include "server.h"

static dispatcher **dispatchers;
static server **servers;
static char debug;
static int metricsock;
static pthread_t collectorid;

/**
 * Collects metrics from dispatchers and servers and emits them.
 */
static void *
collector_runner(void *unused)
{
	int i;
	size_t totticks;
	size_t totmetrics;
	size_t totdropped;
	size_t ticks;
	size_t metrics;
	size_t dropped;
	time_t now;
	char *hostname = strdup(relay_hostname);
	char ipbuf[32];
	char *p;
	FILE *dest = stdout;
	(void)unused;  /* pacify compiler */

	/* prepare hostname for graphite metrics */
	for (p = hostname; *p != '\0'; p++)
		if (*p == '.')
			*p = '_';

	/* create a stream (unless we are in debug mode) */
	if (debug == 0) {
		if ((dest = fdopen(metricsock, "w")) == NULL) {
			fprintf(stderr, "failed to open pipe socket for writing, no statistics will be sent\n");
			dest = stdout;
		}
	}

	i = 0;
	while (keep_running) {
		sleep(1);
		i++;
		if (i < 60)
			continue;
		now = time(NULL);
		totticks = 0;
		totmetrics = 0;
		for (i = 0; dispatchers[i] != NULL; i++) {
			totticks += ticks = dispatch_get_ticks(dispatchers[i]);
			totmetrics += metrics = dispatch_get_metrics(dispatchers[i]);
			fprintf(dest, "carbon.relays.%s.dispatcher%d.metricsReceived %zd %zd\n",
					hostname, i + 1, metrics, (size_t)now);
			fprintf(dest, "carbon.relays.%s.dispatcher%d.wallTime_ns %zd %zd\n",
					hostname, i + 1, ticks, (size_t)now);
		}
		fprintf(dest, "carbon.relays.%s.metricsReceived %zd %zd\n",
				hostname, totmetrics, (size_t)now);
		fprintf(dest, "carbon.relays.%s.dispatch_wallTime_ns %zd %zd\n",
				hostname, totticks, (size_t)now);

		totticks = 0;
		totmetrics = 0;
		totdropped = 0;
		for (i = 0; servers[i] != NULL; i++) {
			totticks += ticks = server_get_ticks(servers[i]);
			totmetrics += metrics = server_get_metrics(servers[i]);
			totdropped += dropped = server_get_dropped(servers[i]);
			snprintf(ipbuf, sizeof(ipbuf), "%s", server_ip(servers[i]));
			for (p = ipbuf; *p != '\0'; p++)
				if (*p == '.')
					*p = '_';
			fprintf(dest, "carbon.relays.%s.destinations.%s:%u.sent %zd %zd\n",
					hostname, ipbuf, server_port(servers[i]), metrics, (size_t)now);
			fprintf(dest, "carbon.relays.%s.destinations.%s:%u.dropped %zd %zd\n",
					hostname, ipbuf, server_port(servers[i]), dropped, (size_t)now);
			fprintf(dest, "carbon.relays.%s.destinations.%s:%u.wallTime_ns %zd %zd\n",
					hostname, ipbuf, server_port(servers[i]), ticks, (size_t)now);
		}
		fprintf(dest, "carbon.relays.%s.metricsSent %zd %zd\n",
				hostname, totmetrics, (size_t)now);
		fprintf(dest, "carbon.relays.%s.metricsDropped %zd %zd\n",
				hostname, totdropped, (size_t)now);
		fprintf(dest, "carbon.relays.%s.server_wallTime_ns %zd %zd\n",
				hostname, totticks, (size_t)now);

		i = 0;
	}

	free(hostname);
	return NULL;
}

/**
 * Initialises and starts the collector.
 */
void
collector_start(dispatcher **d, server **s, char dbg)
{
	dispatchers = d;
	servers = s;
	debug = dbg;
	metricsock = -1;

	if (debug == 0) {
		int pipefds[2];

		/* create pipe to relay metrics over */
		if (pipe(pipefds) < 0) {
			fprintf(stderr, "failed to create pipe, statistics will not be sent\n");
			debug = 1;
		} else {
			dispatch_addconnection(pipefds[0]);
			metricsock = pipefds[1];
		}
	}

	if (pthread_create(&collectorid, NULL, collector_runner, NULL) != 0)
		fprintf(stderr, "failed to start collector!\n");
}

/**
 * Shuts down the collector.
 */
void
collector_stop(void)
{
	if (debug == 0)
		close(metricsock);
	pthread_join(collectorid, NULL);
}
