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
#include <time.h>
#include <pthread.h>

#include "relay.h"
#include "dispatcher.h"

static dispatcher **dispatchers;
static int dispatcherscnt;
static pthread_t collectorid;

/**
 * Collects metrics from dispatchers and servers and emits them.
 */
static void *
collector_runner(void *p)
{
	int i;
	size_t ticks = 0;
	size_t metrics = 0;
	size_t totticks;
	size_t totmetrics;
	time_t now;
	(void)p;  /* pacify compiler */

	while (keep_running) {
		sleep(60);
		now = time(NULL);
		totticks = 0;
		totmetrics = 0;
		for (i = 0; i <  dispatcherscnt; i++) {
			totticks += ticks = dispatch_get_ticks(dispatchers[i]);
			totmetrics += metrics = dispatch_get_metrics(dispatchers[i]);
			fprintf(stdout, "carbon.relays.%s.dispatcher%d.metricsReceived %zd %zd\n",
					relay_hostname, i + 1,  metrics, (size_t)now);
			fprintf(stdout, "carbon.relays.%s.dispatcher%d.wallTime_ms %zd %zd\n",
					relay_hostname, i + 1, ticks, (size_t)now);
		}
		fprintf(stdout, "carbon.relays.%s.metricsReceived %zd %zd\n",
				relay_hostname, totmetrics, (size_t)now);
		fprintf(stdout, "carbon.relays.%s.wallTime_ms %zd %zd\n",
				relay_hostname, totticks, (size_t)now);
	}

	return NULL;
}

/**
 * Initialises and starts the collector.
 */
void
collector_start(int dcnt, dispatcher **d)
{
	dispatcherscnt = dcnt;
	dispatchers = d;

	if (pthread_create(&collectorid, NULL, collector_runner, NULL) != 0)
		fprintf(stderr, "failed to start collector!\n");
}

/**
 * Shuts down the collector.
 */
void
collector_stop(void)
{
	pthread_join(collectorid, NULL);
}
