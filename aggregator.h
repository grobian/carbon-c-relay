/*
 * Copyright 2013-2017 Fabian Groffen
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

#include <regex.h>
#include <pthread.h>

#include "server.h"

#define AGGR_HT_POW_SIZE  12  /* 4096: too big? issue #60 */
typedef struct _aggregator {
	unsigned short interval;  /* when to perform the aggregation */
	unsigned short expire;    /* when incoming metrics are no longer valid */
	enum _aggr_timestamp { TS_START, TS_MIDDLE, TS_END } tswhen;
	unsigned char bucketcnt;
	int disp_conn;
	int fd;
	size_t received;
	size_t sent;
	size_t dropped;
	struct _aggr_computes {
		enum _aggr_compute_type { SUM, CNT, MAX, MIN, AVG,
		                          MEDN, PCTL, VAR, SDEV } type;
		const char *metric;   /* name template of metric to produce */
		struct _aggr_invocations {
			char *metric;       /* actual name to emit */
			unsigned int hash;  /* to speed up matching */
			unsigned short expire;  /* expire + splay */
			struct _aggr_bucket {
				long long int start;
				size_t cnt;
				double sum;
				double max;
				double min;
				struct _aggr_bucket_entries {
					size_t size;
					double *values;
				} entries;
			} *buckets;
			struct _aggr_invocations *next;
		} *invocations_ht[1 << AGGR_HT_POW_SIZE];
		unsigned char entries_needed:1;
		unsigned char percentile:7;
		pthread_rwlock_t invlock;
		struct _aggr_computes *next;
	} *computes;
	struct _aggregator *next;
} aggregator;

aggregator *aggregator_new(unsigned int interval, unsigned int expire, enum _aggr_timestamp tswhen);
char aggregator_add_compute(aggregator *s, const char *metric, const char *type);
void aggregator_set_stub(aggregator *s, const char *stubname);
void aggregator_putmetric(aggregator *s, const char *metric, const char *firstspace, size_t nmatch, regmatch_t *pmatch);
int aggregator_start(aggregator *aggrs);
void aggregator_stop(void);
size_t aggregator_numaggregators(aggregator *agrs);
size_t aggregator_numcomputes(aggregator *aggrs);
size_t aggregator_get_received(aggregator *aggrs);
size_t aggregator_get_sent(aggregator *aggrs);
size_t aggregator_get_dropped(aggregator *aggrs);
size_t aggregator_get_received_sub(aggregator *aggrs);
size_t aggregator_get_sent_sub(aggregator *aggrs);
size_t aggregator_get_dropped_sub(aggregator *aggrs);

#endif
