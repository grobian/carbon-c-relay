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
#include <unistd.h>
#include <string.h>
#include <time.h>
#include <pthread.h>
#include <assert.h>

#include "relay.h"
#include "dispatcher.h"
#include "server.h"
#include "aggregator.h"
#include "collector.h"

static dispatcher **dispatchers;
static char debug = 0;
static pthread_t collectorid;
static char keep_running = 1;
int collector_interval = 60;
static char cluster_refresh_pending = 0;
static cluster *pending_clusters = NULL;

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
	size_t totstalls;
	size_t totdropped;
	size_t ticks;
	size_t metrics;
	size_t queued;
	size_t stalls;
	size_t dropped;
	size_t dispatchers_idle;
	size_t dispatchers_busy;
	time_t now;
	time_t nextcycle;
	char ipbuf[32];
	char *p;
	size_t numaggregators = aggregator_numaggregators();
	server *submission = (server *)s;
	server **srvs = NULL;
	char metric[METRIC_BUFSIZ];
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
		logout("%s", metric); \
	else \
		server_send(submission, strdup(metric), 1);

	nextcycle = time(NULL) + collector_interval;
	while (keep_running) {
		if (cluster_refresh_pending) {
			server **newservers = router_getservers(pending_clusters);
			if (srvs != NULL)
				free(srvs);
			srvs = newservers;
			cluster_refresh_pending = 0;
		}
		assert(srvs != NULL);
		sleep(1);
		now = time(NULL);
		if (nextcycle > now)
			continue;
		nextcycle += collector_interval;
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
			snprintf(m, sizem, "dispatcher%d.wallTime_us %zd %zd\n",
					i + 1, ticks, (size_t)now);
			send(metric);
		}
		snprintf(m, sizem, "metricsReceived %zd %zd\n",
				totmetrics, (size_t)now);
		send(metric);
		snprintf(m, sizem, "dispatch_wallTime_us %zd %zd\n",
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
		totstalls = 0;
		totdropped = 0;
		for (i = 0; srvs[i] != NULL; i++) {
			if (server_ctype(srvs[i]) == CON_PIPE) {
				strncpy(ipbuf, "internal", sizeof(ipbuf));

				ticks = server_get_ticks(srvs[i]);
				metrics = server_get_metrics(srvs[i]);
				queued = server_get_queue_len(srvs[i]);
				stalls = server_get_stalls(srvs[i]);
				dropped = server_get_dropped(srvs[i]);
			} else {
				snprintf(ipbuf, sizeof(ipbuf), "%s:%u",
						server_ip(srvs[i]), server_port(srvs[i]));
				for (p = ipbuf; *p != '\0'; p++)
					if (*p == '.')
						*p = '_';

				totticks += ticks = server_get_ticks(srvs[i]);
				totmetrics += metrics = server_get_metrics(srvs[i]);
				totqueued += queued = server_get_queue_len(srvs[i]);
				totstalls += stalls = server_get_stalls(srvs[i]);
				totdropped += dropped = server_get_dropped(srvs[i]);
			}
			snprintf(m, sizem, "destinations.%s.sent %zd %zd\n",
					ipbuf, metrics, (size_t)now);
			send(metric);
			snprintf(m, sizem, "destinations.%s.queued %zd %zd\n",
					ipbuf, queued, (size_t)now);
			send(metric);
			snprintf(m, sizem, "destinations.%s.stalls %zd %zd\n",
					ipbuf, stalls, (size_t)now);
			send(metric);
			snprintf(m, sizem, "destinations.%s.dropped %zd %zd\n",
					ipbuf, dropped, (size_t)now);
			send(metric);
			snprintf(m, sizem, "destinations.%s.wallTime_us %zd %zd\n",
					ipbuf, ticks, (size_t)now);
			send(metric);
		}

		snprintf(m, sizem, "metricsSent %zd %zd\n",
				totmetrics, (size_t)now);
		send(metric);
		snprintf(m, sizem, "metricsQueued %zd %zd\n",
				totqueued, (size_t)now);
		send(metric);
		snprintf(m, sizem, "metricStalls %zd %zd\n",
				totstalls, (size_t)now);
		send(metric);
		snprintf(m, sizem, "metricsDropped %zd %zd\n",
				totdropped, (size_t)now);
		send(metric);
		snprintf(m, sizem, "server_wallTime_us %zd %zd\n",
				totticks, (size_t)now);
		send(metric);
		snprintf(m, sizem, "connections %zd %zd\n",
				dispatch_get_accepted_connections(), (size_t)now);
		send(metric);
		snprintf(m, sizem, "disconnects %zd %zd\n",
				dispatch_get_closed_connections(), (size_t)now);
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
	size_t numaggregators = aggregator_numaggregators();
	server **srvs = NULL;

	while (keep_running) {
		if (cluster_refresh_pending) {
			server **newservers = router_getservers(pending_clusters);
			if (srvs != NULL)
				free(srvs);
			srvs = newservers;
			cluster_refresh_pending = 0;
		}
		assert(srvs != NULL);
		sleep(1);
		i++;
		if (i < collector_interval)
			continue;
		totdropped = 0;
		for (i = 0; srvs[i] != NULL; i++) {
			queued = server_get_queue_len(srvs[i]);
			totdropped += server_get_dropped(srvs[i]);

			if (queued > 150)
				logout("warning: metrics queuing up "
						"for %s:%u: %zd metrics\n",
						server_ip(srvs[i]), server_port(srvs[i]), queued);
		}
		if (totdropped - lastdropped > 0)
			logout("warning: dropped %zd metrics\n", totdropped - lastdropped);
		lastdropped = totdropped;
		if (numaggregators > 0) {
			totdropped = aggregator_get_dropped();
			if (totdropped - lastaggrdropped > 0)
				logout("warning: aggregator dropped %zd metrics\n",
						totdropped - lastaggrdropped);
			lastaggrdropped = totdropped;
		}

		i = 0;
	}
	return NULL;
}

/**
 * Schedules routes r to be put in place for the current routes.  The
 * replacement is performed at the next cycle of the collector.
 */
inline void
collector_schedulereload(cluster *c)
{
	pending_clusters = c;
	cluster_refresh_pending = 1;
}

/**
 * Returns true if the routes scheduled to be reloaded by a call to
 * collector_schedulereload() have been activated.
 */
inline char
collector_reloadcomplete(void)
{
	return cluster_refresh_pending == 0;
}

/**
 * Initialises and starts the collector.
 */
void
collector_start(dispatcher **d, cluster *c, server *submission)
{
	dispatchers = d;
	collector_schedulereload(c);

	if (mode == DEBUG)
		debug = 1;

	if (mode != SUBMISSION) {
		if (pthread_create(&collectorid, NULL, collector_runner, submission) != 0)
			logerr("failed to start collector!\n");
	} else {
		if (pthread_create(&collectorid, NULL, collector_writer, NULL) != 0)
			logerr("failed to start collector!\n");
	}
}

/**
 * Shuts down the collector.
 */
void
collector_stop(void)
{
	keep_running = 0;
	pthread_join(collectorid, NULL);
}
