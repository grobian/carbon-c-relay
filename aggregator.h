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


#ifndef AGGREGATOR_H
#define AGGREGATOR_H 1

#include <pthread.h>

#include "server.h"

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
int aggregator_start(server *submission);
void aggregator_stop(void);
size_t aggregator_numaggregators(void);
size_t aggregator_numcomputes(void);
size_t aggregator_get_received(void);
size_t aggregator_get_sent(void);
size_t aggregator_get_dropped(void);

#endif
