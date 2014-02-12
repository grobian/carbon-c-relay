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
#include <time.h>
#include <pthread.h>

#include "relay.h"
#include "dispatcher.h"
#include "server.h"
#include "aggregator.h"

static pthread_t aggregatorid;
static aggregator *aggregators = NULL;
static aggregator *lastaggr = NULL;


/**
 * Allocates a new aggregator setup to hold buckets matching interval
 * and expiry time.
 */
aggregator *
aggregator_new(unsigned int interval, unsigned int expire)
{
	int i;
	time_t now;
	aggregator *ret = malloc(sizeof(aggregator));

	if (ret == NULL)
		return ret;

	if (aggregators == NULL) {
		aggregators = lastaggr = ret;
	} else {
		lastaggr = lastaggr->next = ret;
	}

	ret->interval = interval;
	ret->expire = expire;
	ret->received = 0;
	ret->sent = 0;
	ret->dropped = 0;
	ret->next = NULL;

	pthread_mutex_init(&ret->bucketlock, NULL);

	/* start buckets in the past with a splay, but before expiry
	 * the splay is necessary to avoid a thundering herd of expirations
	 * when the config lists many aggregations which then all get the
	 * same start time */
	time(&now);
	now -= expire + 1 + (rand() % interval);

	/* allocate enough buckets to hold the past + future */
	ret->bucketcnt = (expire / interval) * 2 + 1 ;
	ret->buckets = malloc(sizeof(struct _bucket *) * ret->bucketcnt);
	for (i = 0; i < ret->bucketcnt; i++) {
		ret->buckets[i] = malloc(sizeof(struct _bucket));
		ret->buckets[i]->start = now + (i * interval);
		ret->buckets[i]->cnt = 0;
	}

	return ret;
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
		const char *metric)
{
	char *v, *t;
	double val;
	long long int epoch;
	int slot;
	struct _bucket *bucket;

	/* get value */
	if ((v = strchr(metric, ' ')) == NULL || (t = strchr(v + 1, ' ')) == NULL) {
		/* metric includes \n */
		fprintf(stderr, "aggregator: dropping incorrect metric: %s", metric);
		return;
	}

	s->received++;

	val = atof(v + 1);
	epoch = atoll(t + 1);

	epoch -= s->buckets[0]->start;
	if (epoch < 0) {
		/* drop too old metric */
		s->dropped++;
		return;
	}

	slot = epoch / s->interval;
	if (slot >= s->bucketcnt) {
		fprintf(stderr, "aggregator: dropping metric too far in the future: %s",
				metric);
		s->dropped++;
		return;
	}

	pthread_mutex_lock(&s->bucketlock);
	bucket = s->buckets[slot];
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
	int work;
	server *submission = (server *)sub;
	char metric[8096];

	while (keep_running) {
		work = 0;

		for (s = aggregators; s != NULL; s = s->next) {
			now = time(NULL);
			while (s->buckets[0]->start + s->interval < now - s->expire) {
				/* yay, let's produce something cool */
				b = s->buckets[0];
				if (b->cnt > 0) {  /* avoid emitting empty/unitialised data */
					for (c = s->computes; c != NULL; c = c->next) {
						switch (c->type) {
							case SUM:
								snprintf(metric, sizeof(metric), "%s %f %lld\n",
										c->metric, b->sum,
										b->start + s->interval);
								break;
							case CNT:
								snprintf(metric, sizeof(metric), "%s %zd %lld\n",
										c->metric, b->cnt,
										b->start + s->interval);
								break;
							case MAX:
								snprintf(metric, sizeof(metric), "%s %f %lld\n",
										c->metric, b->max,
										b->start + s->interval);
								break;
							case MIN:
								snprintf(metric, sizeof(metric), "%s %f %lld\n",
										c->metric, b->min,
										b->start + s->interval);
								break;
							case AVG:
								snprintf(metric, sizeof(metric), "%s %f %lld\n",
										c->metric, b->sum / (double)b->cnt,
										b->start + s->interval);
								break;
						}
						server_send(submission, metric);
					}
				}
				pthread_mutex_lock(&s->bucketlock);
				if (b->cnt > 0)
					s->sent++;
				/* move the bucket to the end, to make room for new ones */
				memmove(&s->buckets[0], &s->buckets[1],
						sizeof(b) * (s->bucketcnt - 1));
				b->cnt = 0;
				b->start = s->buckets[s->bucketcnt - 2]->start + s->interval;
				s->buckets[s->bucketcnt - 1] = b;
				pthread_mutex_unlock(&s->bucketlock);

				work++;
			}
		}

		if (work == 0)  /* nothing done, avoid spinlocking */
			usleep(250 * 1000);  /* 250ms */
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
	if (pthread_create(&aggregatorid, NULL, aggregator_expire, submission) != 0) {
		fprintf(stderr, "failed to start aggregator!\n");
		return 0;
	}

	return 1;
}

/**
 * Shuts down the aggregator.
 */
void
aggregator_stop(void)
{
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
