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
#include "aggregator.h"

static dispatcher **dispatchers;
static server **servers;
static char debug = 0;
static pthread_t collectorid;

/**
 * Collects metrics from dispatchers and servers and emits them.
 */
static void *
collector_runner(void *s)
{
	int i;
	size_t totticks;
	size_t totmetrics;
	size_t totqueued;
	size_t totdropped;
	size_t ticks;
	size_t metrics;
	size_t queued;
	size_t dropped;
	size_t dispatchers_idle;
	size_t dispatchers_busy;
	time_t now;
	time_t nextcycle;
	char ipbuf[32];
	char *p;
	size_t numaggregators = aggregator_numaggregators();
	server *submission = (server *)s;
	char metric[8096];
	char *m;
	size_t sizem = 0;

	/* prepare hostname for graphite metrics */
	snprintf(metric, sizeof(metric), "carbon.relays.%s", relay_hostname);
	for (m = metric + strlen("carbon.relays."); *m != '\0'; m++)
		if (*m == '.')
			*m = '_';
	*m++ = '.';
	*m = '\0';
	sizem = sizeof(metric) - (m - metric);

#define send(metric) \
	if (debug) \
		fprintf(stdout, "%s", metric); \
	else \
		server_send(submission, metric);

	nextcycle = time(NULL) + 60;
	while (keep_running) {
		sleep(1);
		now = time(NULL);
		if (nextcycle > now)
			continue;
		nextcycle += 60;
		totticks = 0;
		totmetrics = 0;
		dispatchers_idle = 0;
		dispatchers_busy = 0;
		for (i = 0; dispatchers[i] != NULL; i++) {
			if (dispatch_busy(dispatchers[i])) {
				dispatchers_busy++;
			} else {
				dispatchers_idle++;
			}
			totticks += ticks = dispatch_get_ticks(dispatchers[i]);
			totmetrics += metrics = dispatch_get_metrics(dispatchers[i]);
			snprintf(m, sizem, "dispatcher%d.metricsReceived %zd %zd\n",
					i + 1, metrics, (size_t)now);
			send(metric);
			snprintf(m, sizem, "dispatcher%d.wallTime_ns %zd %zd\n",
					i + 1, ticks, (size_t)now);
			send(metric);
		}
		snprintf(m, sizem, "metricsReceived %zd %zd\n",
				totmetrics, (size_t)now);
		send(metric);
		snprintf(m, sizem, "dispatch_wallTime_ns %zd %zd\n",
				totticks, (size_t)now);
		send(metric);
		snprintf(m, sizem, "dispatch_busy %zd %zd\n",
				dispatchers_busy, (size_t)now);
		send(metric);
		snprintf(m, sizem, "dispatch_idle %zd %zd\n",
				dispatchers_idle, (size_t)now);
		send(metric);

		totticks = 0;
		totmetrics = 0;
		totqueued = 0;
		totdropped = 0;
		for (i = 0; servers[i] != NULL; i++) {
			totticks += ticks = server_get_ticks(servers[i]);
			totmetrics += metrics = server_get_metrics(servers[i]);
			totqueued += queued = server_get_queue_len(servers[i]);
			totdropped += dropped = server_get_dropped(servers[i]);
			snprintf(ipbuf, sizeof(ipbuf), "%s", server_ip(servers[i]));
			for (p = ipbuf; *p != '\0'; p++)
				if (*p == '.')
					*p = '_';
			snprintf(m, sizem, "destinations.%s:%u.sent %zd %zd\n",
					ipbuf, server_port(servers[i]), metrics, (size_t)now);
			send(metric);
			snprintf(m, sizem, "destinations.%s:%u.queued %zd %zd\n",
					ipbuf, server_port(servers[i]), queued, (size_t)now);
			send(metric);
			snprintf(m, sizem, "destinations.%s:%u.dropped %zd %zd\n",
					ipbuf, server_port(servers[i]), dropped, (size_t)now);
			send(metric);
			snprintf(m, sizem, "destinations.%s:%u.wallTime_ns %zd %zd\n",
					ipbuf, server_port(servers[i]), ticks, (size_t)now);
			send(metric);
		}
		snprintf(m, sizem, "destinations.internal.sent %zd %zd\n",
				server_get_metrics(submission), (size_t)now);
		send(metric);
		snprintf(m, sizem, "destinations.internal.queued %zd %zd\n",
				server_get_queue_len(submission), (size_t)now);
		send(metric);
		snprintf(m, sizem, "destinations.internal.dropped %zd %zd\n",
				server_get_dropped(submission), (size_t)now);
		send(metric);
		snprintf(m, sizem, "destinations.internal.wallTime_ns %zd %zd\n",
				server_get_ticks(submission), (size_t)now);
		send(metric);

		snprintf(m, sizem, "metricsSent %zd %zd\n",
				totmetrics, (size_t)now);
		send(metric);
		snprintf(m, sizem, "metricsQueued %zd %zd\n",
				totqueued, (size_t)now);
		send(metric);
		snprintf(m, sizem, "metricsDropped %zd %zd\n",
				totdropped, (size_t)now);
		send(metric);
		snprintf(m, sizem, "server_wallTime_ns %zd %zd\n",
				totticks, (size_t)now);
		send(metric);
		snprintf(m, sizem, "connections %zd %zd\n",
				dispatch_get_connections(), (size_t)now);
		send(metric);

		if (numaggregators > 0) {
			snprintf(m, sizem, "aggregators.metricsReceived %zd %zd\n",
					aggregator_get_received(), (size_t)now);
			send(metric);
			snprintf(m, sizem, "aggregators.metricsSent %zd %zd\n",
					aggregator_get_sent(), (size_t)now);
			send(metric);
			snprintf(m, sizem, "aggregators.metricsDropped %zd %zd\n",
					aggregator_get_dropped(), (size_t)now);
			send(metric);
		}

		i = 0;

		if (debug)
			fflush(stdout);
	}

	return NULL;
}

/**
 * Writes messages about dropped events or high queue sizes.
 */
static size_t lastdropped = 0;
static size_t lastaggrdropped = 0;
static void *
collector_writer(void *unused)
{
	int i = 0;
	size_t queued;
	size_t totdropped;
	char nowbuf[24];
	size_t numaggregators = aggregator_numaggregators();

	while (keep_running) {
		sleep(1);
		i++;
		if (i < 60)
			continue;
		totdropped = 0;
		for (i = 0; servers[i] != NULL; i++) {
			queued = server_get_queue_len(servers[i]);
			totdropped += server_get_dropped(servers[i]);

			if (queued > 150) {
				fprintf(stdout, "[%s] warning: metrics queuing up "
						"for %s:%u: %zd metrics\n",
						fmtnow(nowbuf),
						server_ip(servers[i]), server_port(servers[i]), queued);
				fflush(stdout);
			}
		}
		if (totdropped - lastdropped > 0) {
			fprintf(stdout, "[%s] warning: dropped %zd metrics\n",
					fmtnow(nowbuf), totdropped - lastdropped);
			fflush(stdout);
		}
		lastdropped = totdropped;
		if (numaggregators > 0) {
			totdropped = aggregator_get_dropped();
			if (totdropped - lastaggrdropped > 0) {
				fprintf(stdout, "[%s] warning: aggregator dropped %zd metrics\n",
						fmtnow(nowbuf), totdropped - lastaggrdropped);
				fflush(stdout);
			}
			lastaggrdropped = totdropped;
		}

		i = 0;
	}
	return NULL;
}

/**
 * Initialises and starts the collector.
 */
void
collector_start(dispatcher **d, server **s, enum rmode mode, server *submission)
{
	dispatchers = d;
	servers = s;

	if (mode == NORMAL) {
	}

	if (mode == DEBUG)
		debug = 1;

	if (mode != SUBMISSION) {
		if (pthread_create(&collectorid, NULL, collector_runner, submission) != 0)
			fprintf(stderr, "failed to start collector!\n");
	} else {
		if (pthread_create(&collectorid, NULL, collector_writer, NULL) != 0)
			fprintf(stderr, "failed to start collector!\n");
	}
}

/**
 * Shuts down the collector.
 */
void
collector_stop(void)
{
	pthread_join(collectorid, NULL);
}
