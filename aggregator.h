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

#ifndef AGGREGATOR_H
#define AGGREGATOR_H 1

#include <pthread.h>

typedef struct _aggregator {
	unsigned short interval;  /* when to perform the aggregation */
	unsigned short expire;    /* when incoming metrics are no longer valid */
	unsigned char bucketcnt;
	struct _bucket {
		long long int start;
		size_t cnt;
		double sum;
		double max;
		double min;
	} **buckets;
	size_t received;
	size_t sent;
	size_t dropped;
	struct _aggr_computes {
		enum { SUM, CNT, MAX, MIN, AVG } type;
		char *metric;         /* name of metric to produce */
		struct _aggr_computes *next;
	} *computes;
	pthread_mutex_t bucketlock;
	struct _aggregator *next;
} aggregator;

aggregator *aggregator_new(unsigned int interval, unsigned int expire);
void aggregator_putmetric(aggregator *s, const char *metric);
int aggregator_start(void);
void aggregator_stop(void);
char aggregator_hasaggregators(void);
size_t aggregator_get_received(void);
size_t aggregator_get_sent(void);
size_t aggregator_get_dropped(void);

#endif
