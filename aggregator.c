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
#include <unistd.h>
#include <string.h>
#include <time.h>
#include <regex.h>
#include <pthread.h>

#include "relay.h"
#include "dispatcher.h"
#include "server.h"
#include "router.h"
#include "aggregator.h"

static pthread_t aggregatorid;
static aggregator *aggregators = NULL;
static aggregator *lastaggr = NULL;
static char keep_running = 1;


/**
 * Allocates a new aggregator setup to hold buckets matching interval
 * and expiry time.
 */
aggregator *
aggregator_new(unsigned int interval, unsigned int expire)
{
	aggregator *ret = malloc(sizeof(aggregator));

	if (ret == NULL)
		return ret;
	if (interval == 0 || expire == 0) {
		free(ret);
		return NULL;
	}

	if (aggregators == NULL) {
		aggregators = lastaggr = ret;
	} else {
		lastaggr = lastaggr->next = ret;
	}

	ret->interval = interval;
	ret->expire = expire;
	ret->bucketcnt = (expire / interval) * 2 + 1 ;
	ret->received = 0;
	ret->sent = 0;
	ret->dropped = 0;
	ret->computes = NULL;
	ret->next = NULL;

	pthread_mutex_init(&ret->bucketlock, NULL);

	return ret;
}

/**
 * Adds a new compute part to this aggregator.  Returns -1 if type is
 * not a recognised aggregation type.
 */
char
aggregator_add_compute(
		aggregator *s,
		const char *metric,
		const char *type)
{
	struct _aggr_computes *ac = s->computes;
	enum _aggr_compute_type act;

	if (strcmp(type, "sum") == 0) {
		act = SUM;
	} else if (strcmp(type, "count") == 0) {
		act = CNT;
	} else if (strcmp(type, "max") == 0) {
		act = MAX;
	} else if (strcmp(type, "min") == 0) {
		act = MIN;
	} else if (strcmp(type, "average") == 0) {
		act = AVG;
	} else {
		return -1;
	}

	if (ac == NULL) {
		ac = s->computes = malloc(sizeof(*ac));
	} else {
		while (ac->next != NULL)
			ac = ac->next;
		ac = ac->next = malloc(sizeof(*ac));
	}

	ac->type = act;
	ac->metric = strdup(metric);
	ac->invocations = NULL;
	ac->next = NULL;

	return 0;
}

/**
 * Adds a new metric to aggregator s.  The value from the metric is put
 * in the bucket matching the epoch contained in the metric.  In cases
 * where the contained epoch is too old or too new, the metric is
 * rejected.
 */
void
aggregator_putmetric(
		aggregator *s,
		const char *metric,
		const char *firstspace,
		size_t nmatch,
		regmatch_t *pmatch)
{
	char *v;
	double val;
	long long int epoch;
	long long int itime;
	int slot;
	struct _bucket *bucket;
	char newmetric[METRIC_BUFSIZ];
	char *newfirstspace = NULL;
	size_t len;
	const char *ometric;
	struct _aggr_computes *compute;
	struct _aggr_invocations *invocation;

	/* get value */
	if ((v = strchr(firstspace + 1, ' ')) == NULL) {
		/* metric includes \n */
		if (mode == DEBUG)
			logerr("aggregator: dropping incorrect metric: %s",
					metric);
		return;
	}

	s->received++;

	val = atof(firstspace + 1);
	epoch = atoll(v + 1);

	pthread_mutex_lock(&s->bucketlock);
	for (compute = s->computes; compute != NULL; compute = compute->next) {
		if (nmatch == 0) {
			ometric = compute->metric;
		} else if ((len = router_rewrite_metric(
						&newmetric, &newfirstspace,
						metric, firstspace,
						compute->metric,
						nmatch, pmatch)) == 0)
		{
			/* fail, skip */
			continue;
		} else {
			*newfirstspace = '\0';
			ometric = newmetric;
		}

		for (invocation = compute->invocations; invocation != NULL; invocation = invocation->next)
			if (strcmp(ometric, invocation->metric) == 0)  /* match */
				break;
		if (invocation == NULL) {  /* no match, add */
			int i;
			time_t now;

			if ((invocation = malloc(sizeof(*invocation))) == NULL) {
				logerr("aggregator: out of memory creating %s from %s",
						ometric, metric);
				continue;
			}
			if ((invocation->metric = strdup(ometric)) == NULL) {
				logerr("aggregator: out of memory creating %s from %s",
						ometric, metric);
				free(invocation);
				continue;
			}

			/* Start buckets in the past with a splay, but before expiry
			 * the splay is necessary to avoid a thundering herd of
			 * expirations when the aggregator is spammed with metrics,
			 * e.g. right after startup when other relays flush their
			 * queues. */
			time(&now);
			now -= s->expire + 1 + (rand() % s->interval);

			/* allocate enough buckets to hold the past + future */
			invocation->buckets =
				malloc(sizeof(struct _bucket) * s->bucketcnt);
			if (invocation->buckets == NULL) {
				logerr("aggregator: out of memory creating %s from %s",
						ometric, metric);
				free(invocation->metric);
				free(invocation);
				continue;
			}
			for (i = 0; i < s->bucketcnt; i++) {
				invocation->buckets[i].start = now + (i * s->interval);
				invocation->buckets[i].cnt = 0;
			}

			invocation->next = compute->invocations;
			compute->invocations = invocation;
		}

		/* finally, try to do the maths */

		itime = epoch - invocation->buckets[0].start;
		if (itime < 0) {
			/* drop too old metric */
			s->dropped++;
			continue;
		}

		slot = itime / s->interval;
		if (slot >= s->bucketcnt) {
			if (mode == DEBUG)
				logerr("aggregator: dropping metric too far in the "
						"future (%lld > %lld): %s from %s", epoch,
						invocation->buckets[s->bucketcnt - 1].start,
						ometric, metric);
			s->dropped++;
			continue;
		}

		bucket = &invocation->buckets[slot];
		if (bucket->cnt == 0) {
			bucket->cnt = 1;
			bucket->sum = val;
			bucket->max = val;
			bucket->min = val;
		} else {
			bucket->cnt++;
			bucket->sum += val;
			if (bucket->max < val)
				bucket->max = val;
			if (bucket->min > val)
				bucket->min = val;
		}
	}
	pthread_mutex_unlock(&s->bucketlock);

	return;
}

/**
 * Checks if the oldest bucket should be expired, if so, sends out
 * computed aggregate metrics and moves the bucket to the end of the
 * list.
 */
static void *
aggregator_expire(void *sub)
{
	time_t now;
	aggregator *s;
	struct _bucket *b;
	struct _aggr_computes *c;
	struct _aggr_invocations *i;
	int work;
	server *submission = (server *)sub;
	char metric[METRIC_BUFSIZ];

	while (1) {
		work = 0;

		for (s = aggregators; s != NULL; s = s->next) {
			/* send metrics for buckets that are completely past the
			 * expiry time, unless we are shutting down, then send
			 * metrics for all buckets that have completed */
			now = time(NULL) + (keep_running ? 0 : s->expire);
			for (c = s->computes; c != NULL; c = c->next) {
				for (i = c->invocations; i != NULL; i = i->next) {
					while (i->buckets[0].start + s->interval < now - s->expire) {
						/* yay, let's produce something cool */
						b = &i->buckets[0];
						if (b->cnt > 0) {  /* avoid emitting empty/unitialised data */
							switch (c->type) {
								case SUM:
									snprintf(metric, sizeof(metric),
											"%s %f %lld\n",
											i->metric, b->sum,
											b->start + s->interval);
									break;
								case CNT:
									snprintf(metric, sizeof(metric),
											"%s %zd %lld\n",
											i->metric, b->cnt,
											b->start + s->interval);
									break;
								case MAX:
									snprintf(metric, sizeof(metric),
											"%s %f %lld\n",
											i->metric, b->max,
											b->start + s->interval);
									break;
								case MIN:
									snprintf(metric, sizeof(metric),
											"%s %f %lld\n",
											i->metric, b->min,
											b->start + s->interval);
									break;
								case AVG:
									snprintf(metric, sizeof(metric),
											"%s %f %lld\n",
											i->metric,
											b->sum / (double)b->cnt,
											b->start + s->interval);
									break;
							}
							server_send(submission, strdup(metric), 1);
						}
						pthread_mutex_lock(&s->bucketlock);
						if (b->cnt > 0)
							s->sent++;
						/* move the bucket to the end, to make room for new ones */
						memmove(&i->buckets[0], &i->buckets[1],
								sizeof(*b) * (s->bucketcnt - 1));
						b = &i->buckets[s->bucketcnt - 1];
						b->cnt = 0;
						b->start = i->buckets[s->bucketcnt - 2].start + s->interval;
						pthread_mutex_unlock(&s->bucketlock);

						work++;
					}
				}
			}
		}

		if (work == 0) {
			if (!keep_running)
				break;
			/* nothing done, avoid spinlocking */
			usleep(250 * 1000);  /* 250ms */
		}
	}

	return NULL;
}

/**
 * Returns the number of aggregators defined.
 */
size_t
aggregator_numaggregators(void)
{
	size_t totaggregators = 0;
	aggregator *a;

	for (a = aggregators; a != NULL; a = a->next)
		totaggregators++;

	return totaggregators;
}

/**
 * Returns the total number of computations defined.
 */
size_t
aggregator_numcomputes(void)
{
	size_t totcomputes = 0;
	aggregator *a;
	struct _aggr_computes *c;

	for (a = aggregators; a != NULL; a = a->next)
		for (c = a->computes; c != NULL; c = c->next)
			totcomputes++;

	return totcomputes;
}

/**
 * Initialises and starts the aggregator.  Returns false when starting
 * failed, true otherwise.
 */
int
aggregator_start(server *submission)
{
	if (pthread_create(&aggregatorid, NULL, aggregator_expire, submission) != 0)
		return 0;

	return 1;
}

/**
 * Shuts down the aggregator.
 */
void
aggregator_stop(void)
{
	keep_running = 0;
	pthread_join(aggregatorid, NULL);
}

/**
 * Returns an approximate number of received metrics by all aggregators.
 */
size_t
aggregator_get_received(void)
{
	size_t totreceived = 0;
	aggregator *a;

	for (a = aggregators; a != NULL; a = a->next)
		totreceived += a->received;

	return totreceived;
}

/**
 * Returns an approximate number of metrics sent by all aggregators.
 */
size_t
aggregator_get_sent(void)
{
	size_t totsent = 0;
	aggregator *a;

	for (a = aggregators; a != NULL; a = a->next)
		totsent += a->sent;

	return totsent;
}

/**
 * Returns an approximate number of dropped metrics by all aggregators.
 * Metrics are dropped if they are too much in the past (past expiry
 * time) or if they are too much in the future.
 */
size_t
aggregator_get_dropped(void)
{
	size_t totdropped = 0;
	aggregator *a;

	for (a = aggregators; a != NULL; a = a->next)
		totdropped += a->dropped;

	return totdropped;
}
