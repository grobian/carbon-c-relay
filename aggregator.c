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
#include <math.h>
#include <regex.h>
#include <pthread.h>
#include <assert.h>

#include "relay.h"
#include "dispatcher.h"
#include "server.h"
#include "router.h"
#include "aggregator.h"
#include "fnv1a.h"

static pthread_t aggregatorid;
static aggregator *aggregators = NULL;
static aggregator *lastaggr = NULL;
static size_t prevreceived = 0;
static size_t prevsent = 0;
static size_t prevdropped = 0;
static char keep_running = 1;


/**
 * Allocates a new aggregator setup to hold buckets matching interval
 * and expiry time.
 */
aggregator *
aggregator_new(
		unsigned int interval,
		unsigned int expire,
		enum _aggr_timestamp tswhen)
{
	aggregator *ret = malloc(sizeof(aggregator));

	if (ret == NULL)
		return ret;

	assert(interval != 0);
	assert(interval < expire);

	if (aggregators == NULL) {
		aggregators = lastaggr = ret;
	} else {
		lastaggr = lastaggr->next = ret;
	}

	ret->interval = interval;
	ret->expire = expire;
	ret->tswhen = tswhen;
	ret->bucketcnt = (expire + (interval - 1)) / interval + 1 + 1;
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
	char store = 0;
	int pctl = 0;

	if (strcmp(type, "sum") == 0) {
		act = SUM;
	} else if (strcmp(type, "count") == 0 || strcmp(type, "cnt") == 0) {
		act = CNT;
	} else if (strcmp(type, "max") == 0) {
		act = MAX;
	} else if (strcmp(type, "min") == 0) {
		act = MIN;
	} else if (strcmp(type, "average") == 0 || strcmp(type, "avg") == 0) {
		act = AVG;
	} else if (strcmp(type, "median") == 0) {
		act = MEDN;
		pctl = 50;
		store = 1;
	} else if (strncmp(type, "percentile", strlen("percentile")) == 0) {
		pctl = atoi(type + strlen("percentile"));
		if (pctl > 100 || pctl <= 0) {
			return -1;
		} else {
			act = PCTL;
			store = 1;
		}
	} else if (strcmp(type, "variance") == 0) {
		act = VAR;
		store = 1;
	} else if (strcmp(type, "stddev") == 0) {
		act = SDEV;
		store = 1;
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
	ac->percentile = (unsigned char)pctl;
	ac->metric = strdup(metric);
	memset(ac->invocations_ht, 0, sizeof(ac->invocations_ht));
	ac->entries_needed = store;
	ac->next = NULL;

	return 0;
}

void
aggregator_set_stub(
		aggregator *s,
		const char *stubname)
{
	struct _aggr_computes *ac;
	char newmetric[METRIC_BUFSIZ];

	for (ac = s->computes; ac != NULL; ac = ac->next) {
		snprintf(newmetric, sizeof(newmetric), "%s%s", stubname, ac->metric);
		free((void *)ac->metric);
		ac->metric = strdup(newmetric);
	}
}

/**
 * Adds a new metric to aggregator s.  The value from the metric is put
 * in the bucket matching the epoch contained in the metric.  In cases
 * where the contained epoch is too old or too new, the metric is
 * dropped.
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
	char newmetric[METRIC_BUFSIZ];
	char *newfirstspace = NULL;
	size_t len;
	const char *ometric;
	const char *omp;
	unsigned int omhash;
	unsigned int omhtbucket;
	struct _aggr_computes *compute;
	struct _aggr_invocations *invocation;
	struct _aggr_bucket *bucket;
	struct _aggr_bucket_entries *entries;

	/* get value */
	if ((v = strchr(firstspace + 1, ' ')) == NULL) {
		/* metric includes \n */
		if (mode == DEBUG || mode == DEBUGTEST)
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

		omhash = FNV1A_32_OFFSET;
		for (omp = ometric; *omp != '\0'; omp++)
			omhash = (omhash ^ (unsigned int)*omp) * FNV1A_32_PRIME;

		omhtbucket =
			((omhash >> AGGR_HT_POW_SIZE) ^ omhash) &
			(((unsigned int)1 << AGGR_HT_POW_SIZE) - 1);
		invocation = compute->invocations_ht[omhtbucket];
		for (; invocation != NULL; invocation = invocation->next)
			if (invocation->hash == omhash &&
					strcmp(ometric, invocation->metric) == 0)  /* match */
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
			invocation->hash = omhash;

			/* Start buckets in the past such that expiry time
			 * conditions are met.  Add a splay to the expiry time to
			 * avoid a thundering herd of expirations when the
			 * aggregator is spammed with metrics, e.g. right after
			 * startup when other relays flush their queues.  This
			 * approach shouldn't affect the timing of the buckets as
			 * requested in issue #72.
			 * For consistency with other tools/old carbon-aggregator
			 * align the buckets to interval boundaries such that it is
			 * predictable what intervals will be taken, issue #104. */
			time(&now);
			now = ((now - s->expire) / s->interval) * s->interval;
			invocation->expire = s->expire + (rand() % s->interval);

			/* allocate enough buckets to hold the past + future */
			invocation->buckets =
				malloc(sizeof(struct _aggr_bucket) * s->bucketcnt);
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

			invocation->next = compute->invocations_ht[omhtbucket];
			compute->invocations_ht[omhtbucket] = invocation;
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
			if (mode == DEBUG || mode == DEBUGTEST)
				logerr("aggregator: dropping metric too far in the "
						"future (%lld > %lld): %s from %s", epoch,
						invocation->buckets[s->bucketcnt - 1].start,
						ometric, metric);
			s->dropped++;
			continue;
		}

		bucket = &invocation->buckets[slot];
		if (bucket->cnt == 0) {
			bucket->sum = val;
			bucket->max = val;
			bucket->min = val;
		} else {
			bucket->sum += val;
			if (bucket->max < val)
				bucket->max = val;
			if (bucket->min > val)
				bucket->min = val;
		}
		entries = &bucket->entries;
		if (compute->entries_needed) {
			if (bucket->cnt == entries->size) {
#define E_I_SZ 64
				double *new = realloc(entries->values, entries->size + E_I_SZ);
				if (new == NULL) {
					logerr("aggregator: out of memory creating entry bucket "
							"(%s from %s)", ometric, metric);
				} else {
					entries->values = new;
					entries->size += E_I_SZ;
				}
			}
			if (bucket->cnt < entries->size)
				entries->values[bucket->cnt] = val;
		}
		bucket->cnt++;
	}
	pthread_mutex_unlock(&s->bucketlock);

	return;
}

static inline int
cmp_entry(const void *l, const void *r)
{
	return *(const double *)l - *(const double *)r;
}

/**
 * Checks if the oldest bucket should be expired, if so, sends out
 * computed aggregate metrics and moves the bucket to the end of the
 * list.  When no buckets are in use for an invocation, it is removed to
 * cleanup resources.
 */
static void *
aggregator_expire(void *sub)
{
	time_t now;
	aggregator *s;
	struct _aggr_bucket *b;
	struct _aggr_computes *c;
	struct _aggr_invocations *inv;
	struct _aggr_invocations *lastinv;
	double *values;
	int i;
	unsigned char j;
	int work;
	server *submission = (server *)sub;
	char metric[METRIC_BUFSIZ];
	char isempty;
	long long int ts = 0;

	while (1) {
		work = 0;

		for (s = aggregators; s != NULL; s = s->next) {
			/* send metrics for buckets that are completely past the
			 * expiry time, unless we are shutting down, then send
			 * metrics for all buckets that have completed */
			now = time(NULL) + (keep_running ? 0 : s->expire - s->interval);
			for (c = s->computes; c != NULL; c = c->next) {
				for (i = 0; i < (1 << AGGR_HT_POW_SIZE); i++) {
					lastinv = NULL;
					isempty = 0;
					for (inv = c->invocations_ht[i]; inv != NULL; ) {
						while (inv->buckets[0].start +
								(keep_running ? inv->expire : s->expire) < now)
						{
							/* yay, let's produce something cool */
							b = &inv->buckets[0];
							/* avoid emitting empty/unitialised data */
							isempty = b->cnt == 0;
							if (!isempty) {
								switch (s->tswhen) {
									case TS_START:
										ts = b->start;
										break;
									case TS_MIDDLE:
										ts = b->start + (s->interval / 2);
										break;
									case TS_END:
										ts = b->start + s->interval;
										break;
									default:
										assert(0);
								}
								switch (c->type) {
									case SUM:
										snprintf(metric, sizeof(metric),
												"%s %f %lld\n",
												inv->metric, b->sum, ts);
										break;
									case CNT:
										snprintf(metric, sizeof(metric),
												"%s %zd %lld\n",
												inv->metric, b->cnt, ts);
										break;
									case MAX:
										snprintf(metric, sizeof(metric),
												"%s %f %lld\n",
												inv->metric, b->max, ts);
										break;
									case MIN:
										snprintf(metric, sizeof(metric),
												"%s %f %lld\n",
												inv->metric, b->min, ts);
										break;
									case AVG:
										snprintf(metric, sizeof(metric),
												"%s %f %lld\n",
												inv->metric,
												b->sum / (double)b->cnt, ts);
										break;
									case MEDN:
										/* median == 50th percentile */
									case PCTL: {
										/* nearest rank method */
										size_t n =
											(int)(((double)c->percentile/100.0 *
														(double)b->cnt) + 0.9);
										values = b->entries.values;
										/* TODO: lazy approach, in case
										 * of 1 (first/last) or 2 buckets
										 * distance we could do a
										 * forward run picking the max
										 * entries and returning that
										 * iso sorting the full array */
										qsort(values, b->cnt,
												sizeof(double), cmp_entry);
										snprintf(metric, sizeof(metric),
												"%s %f %lld\n",
												inv->metric,
												values[n - 1],
												b->start + s->interval);
									}	break;
									case VAR:
									case SDEV: {
										double avg = b->sum / (double)b->cnt;
										double ksum = 0;
										values = b->entries.values;
										for (i = 0; i < b->cnt; i++)
											ksum += pow(values[i] - avg, 2);
										ksum /= (double)b->cnt;
										snprintf(metric, sizeof(metric),
												"%s %f %lld\n",
												inv->metric,
												c->type == VAR ? ksum :
													sqrt(ksum),
												b->start + s->interval);
									}	break;
								}
								server_send(submission, strdup(metric), 1);
								s->sent++;
							}

							/* move the bucket to the end, to make room for
							 * new ones */
							pthread_mutex_lock(&s->bucketlock);
							memmove(&inv->buckets[0], &inv->buckets[1],
									sizeof(*b) * (s->bucketcnt - 1));
							b = &inv->buckets[s->bucketcnt - 1];
							b->cnt = 0;
							b->start =
								inv->buckets[s->bucketcnt - 2].start +
								s->interval;
							pthread_mutex_unlock(&s->bucketlock);

							work++;
						}

						if (isempty) {
							/* see if the remaining buckets are empty too */
							pthread_mutex_lock(&s->bucketlock);
							for (j = 0; j < s->bucketcnt; j++) {
								if (inv->buckets[j].cnt != 0) {
									isempty = 0;
									pthread_mutex_unlock(&s->bucketlock);
									break;
								}
							}
						}
						if (isempty) {
							/* free and unlink */
							if (c->entries_needed)
								for (j = 0; j < s->bucketcnt; j++)
									if (inv->buckets[j].entries.values)
										free(inv->buckets[j].entries.values);
							free(inv->metric);
							free(inv->buckets);
							if (lastinv != NULL) {
								lastinv->next = inv->next;
								free(inv);
								inv = lastinv->next;
							} else {
								c->invocations_ht[i] = inv->next;
								free(inv);
								inv = c->invocations_ht[i];
							}
							pthread_mutex_unlock(&s->bucketlock);
						} else {
							lastinv = inv;
							inv = inv->next;
						}
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
 * Returns an approximate number of metrics received by all aggregators
 * since the last call to this function.
 */
inline size_t
aggregator_get_received_sub()
{
	size_t d = aggregator_get_received();
	size_t r = d - prevreceived;
	prevreceived += d;
	return r;
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
 * Returns an approximate number of metrics sent by all aggregators
 * since the last call to this function.
 */
inline size_t
aggregator_get_sent_sub()
{
	size_t d = aggregator_get_sent();
	size_t r = d - prevsent;
	prevsent += d;
	return r;
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

/**
 * Returns an approximate number of metrics dropped by all aggregators
 * since the last call to this function.
 */
inline size_t
aggregator_get_dropped_sub()
{
	size_t d = aggregator_get_dropped();
	size_t r = d - prevdropped;
	prevdropped += d;
	return r;
}
