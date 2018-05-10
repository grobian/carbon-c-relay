/*
 * Copyright 2013-2018 Fabian Groffen
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
#include <pthread.h>
#include <errno.h>
#include <assert.h>

#include "relay.h"
#include "dispatcher.h"
#include "server.h"
#include "router.h"
#include "fnv1a.h"
#include "aggregator.h"
#include "posixregex.h"

static pthread_t aggregatorid;
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
	int intconn[2];

	if (ret == NULL)
		return ret;

	assert(interval != 0);
	assert(interval < expire);

	if (pipe(intconn) < 0) {
		logerr("failed to create pipe for aggregator: %s\n",
				strerror(errno));
		free(ret);
		return NULL;
	}
	ret->disp_conn = dispatch_addconnection_aggr(intconn[0]);
	ret->fd = intconn[1];

	ret->interval = interval;
	ret->expire = expire;
	ret->tswhen = tswhen;
	ret->bucketcnt = (expire + (interval - 1)) / interval + 1 + 1;
	ret->received = 0;
	ret->sent = 0;
	ret->dropped = 0;
	ret->computes = NULL;
	ret->next = NULL;

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
		enum _aggr_compute_type act,
		unsigned char pctl)
{
	struct _aggr_computes *ac = s->computes;
	char store = 0;

	if (act == MEDN) {
		pctl = 50;
		store = 1;
	} else if (act == PCTL || act == VAR || act == SDEV) {
		store = 1;
	}

	if (ac == NULL) {
		ac = s->computes = malloc(sizeof(*ac));
	} else {
		while (ac->next != NULL)
			ac = ac->next;
		ac = ac->next = malloc(sizeof(*ac));
	}
	if (ac == NULL)
		return -1;

	ac->type = act;
	ac->percentile = pctl;
	ac->metric = strdup(metric);
	if (ac->metric == NULL)
		return -1;
	memset(ac->invocations_ht, 0, sizeof(ac->invocations_ht));
	ac->entries_needed = store;
	pthread_rwlock_init(&ac->invlock, NULL);
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

	/* do not accept new values when shutting down, issue #200 */
	if (__sync_bool_compare_and_swap(&keep_running, 0, 0))
		return;

	/* get timestamp */
	if ((v = strchr(firstspace + 1, ' ')) == NULL) {
		/* metric includes \n */
		if (mode & MODE_DEBUG)
			logerr("aggregator: dropping incorrect metric: %s",
					metric);
		return;
	}

	__sync_add_and_fetch(&s->received, 1);

	val = atof(firstspace + 1);
	epoch = atoll(v + 1);

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

		/* first "cheap" search for an invocation, using a read-lock */
		pthread_rwlock_rdlock(&compute->invlock);
		invocation = compute->invocations_ht[omhtbucket];
		for (; invocation != NULL; invocation = invocation->next)
			if (invocation->hash == omhash &&
					strcmp(ometric, invocation->metric) == 0)  /* match */
				break;

		if (invocation == NULL) {  /* no match, add */
			long long int i;
			time_t now;

			/* swap the lock for an expensive exclusive lock to
			 * add/update the invocation */
			pthread_rwlock_unlock(&compute->invlock);
			pthread_rwlock_wrlock(&compute->invlock);

			/* recheck if invocation is still NULL */
			invocation = compute->invocations_ht[omhtbucket];
			for (; invocation != NULL; invocation = invocation->next)
				if (invocation->hash == omhash &&
						strcmp(ometric, invocation->metric) == 0)  /* match */
					break;

			if (invocation == NULL) {
				if ((invocation = malloc(sizeof(*invocation))) == NULL) {
					logerr("aggregator: out of memory creating %s from %s",
							ometric, metric);
					pthread_rwlock_unlock(&compute->invlock);
					continue;
				}
				if ((invocation->metric = strdup(ometric)) == NULL) {
					logerr("aggregator: out of memory creating %s from %s",
							ometric, metric);
					free(invocation);
					pthread_rwlock_unlock(&compute->invlock);
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
					pthread_rwlock_unlock(&compute->invlock);
					continue;
				}
				for (i = 0; i < s->bucketcnt; i++) {
					bucket = &invocation->buckets[i];
					pthread_mutex_init(&bucket->bcklock, NULL);
					bucket->state = A_OPEN;
					bucket->start = now + (i * (long long int)s->interval);
					bucket->cnt = 0;
					bucket->entries.size = 0;
					bucket->entries.values = NULL;
				}

				invocation->next = compute->invocations_ht[omhtbucket];
				compute->invocations_ht[omhtbucket] = invocation;
			}
		}

		/* finally, try to do the maths */
		itime = epoch - invocation->buckets[0].start;
		if (itime < 0) {
			/* drop too old metric */
			__sync_add_and_fetch(&s->dropped, 1);
			pthread_rwlock_unlock(&compute->invlock);
			continue;
		}

		if (itime >= (s->bucketcnt * s->interval)) {
			if (mode & MODE_DEBUG)
				logerr("aggregator: dropping metric too far in the "
						"future (%lld > %lld): %s from %s", epoch,
						invocation->buckets[s->bucketcnt - 1].start,
						ometric, metric);
			__sync_add_and_fetch(&s->dropped, 1);
			pthread_rwlock_unlock(&compute->invlock);
			continue;
		}

		bucket = &invocation->buckets[itime / s->interval];
		if (__sync_bool_compare_and_swap(&bucket->state, A_EXPIRE, A_EXPIRE)) {
			/* drop too old metric */
			__sync_add_and_fetch(&s->dropped, 1);
			pthread_rwlock_unlock(&compute->invlock);
			continue;
		}

		/* Since we have for most cases just a read-lock on invocation
		 * here, run the bucket modification in a critical section of
		 * its own. */
		pthread_mutex_lock(&bucket->bcklock);
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
				double *new = realloc(entries->values,
						sizeof(double) * (entries->size + E_I_SZ));
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
		pthread_mutex_unlock(&bucket->bcklock);
		pthread_rwlock_unlock(&compute->invlock);
	}

	return;
}

static inline int
cmp_entry(const void *l, const void *r)
{
	return *(const double *)l - *(const double *)r;
}

static inline void
write_metric(
		aggregator *s,
		struct _aggr_bucket *b,
		struct _aggr_computes *c,
		char *invmetric)
{
	char metric[METRIC_BUFSIZ];
	double *values;
	size_t len = 0;
	long long int ts = 0;
	int k;

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

	pthread_mutex_lock(&b->bcklock);
	switch (c->type) {
		case SUM:
			len = snprintf(metric, sizeof(metric),
					"%s %f %lld\n", invmetric, b->sum, ts);
			break;
		case CNT:
			len = snprintf(metric, sizeof(metric),
					"%s %zu %lld\n", invmetric, b->cnt, ts);
			break;
		case MAX:
			len = snprintf(metric, sizeof(metric),
					"%s %f %lld\n", invmetric, b->max, ts);
			break;
		case MIN:
			len = snprintf(metric, sizeof(metric),
					"%s %f %lld\n", invmetric, b->min, ts);
			break;
		case AVG:
			len = snprintf(metric, sizeof(metric),
					"%s %f %lld\n", invmetric, b->sum / (double)b->cnt, ts);
			break;
		case MEDN:
			/* median == 50th percentile */
		case PCTL:
			/* nearest rank method */
			k = (int)(((double)c->percentile/100.0 * (double)b->cnt) + 0.9);
			values = b->entries.values;
			/* TODO: lazy approach, in case of 1 (first/last) or 2
			 * buckets distance we could do a forward run picking the
			 * max entries and returning that iso sorting the full array */
			qsort(values, b->cnt, sizeof(double), cmp_entry);
			len = snprintf(metric, sizeof(metric),
					"%s %f %lld\n", invmetric, values[k - 1], ts);
			break;
		case VAR:
		case SDEV: {
			double avg = b->sum / (double)b->cnt;
			double ksum = 0;
			values = b->entries.values;
			for (k = 0; k < b->cnt; k++)
			   ksum += pow(values[k] - avg, 2);
			ksum /= (double)b->cnt;
			len = snprintf(metric, sizeof(metric), "%s %f %lld\n",
				   invmetric, c->type == VAR ? ksum : sqrt(ksum), ts);
		}	break;
		default:
			assert(0);  /* for compiler (len) */
	}
	pthread_mutex_unlock(&b->bcklock);

	/* perform the write outside of any lock */
	pthread_rwlock_unlock(&c->invlock);

	ts = write(s->fd, metric, len);
	if (ts < 0) {
		logerr("aggregator: failed to write to "
				"pipe (fd=%d): %s\n",
				s->fd, strerror(errno));
		__sync_add_and_fetch(&s->dropped, 1);
	} else if (ts < len) {
		logerr("aggregator: uncomplete write on "
				"pipe (fd=%d)\n", s->fd);
		__sync_add_and_fetch(&s->dropped, 1);
	} else {
		__sync_add_and_fetch(&s->sent, 1);
	}
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
	aggregator *aggrs = (aggregator *)sub;
	time_t now;
	aggregator *s;
	struct _aggr_bucket *b;
	struct _aggr_computes *c;
	struct _aggr_invocations *inv;
	struct _aggr_invocations *lastinv;
	double *values;
	size_t len = 0;
	int i;
	unsigned char j;
	int work;
	char isempty;

	while (1) {
		work = 0;

		for (s = aggrs; s != NULL; s = s->next) {
			/* send metrics for buckets that are completely past the
			 * expiry time, unless we are shutting down, then send
			 * metrics for all buckets that have completed */
			now = time(NULL) + (__sync_bool_compare_and_swap(
						&keep_running, 1, 1) ? 0 : s->expire - s->interval);
			for (c = s->computes; c != NULL; c = c->next) {
				pthread_rwlock_rdlock(&c->invlock);
				for (i = 0; i < (1 << AGGR_HT_POW_SIZE); i++) {
					isempty = 0;
					for (inv = c->invocations_ht[i];
							inv != NULL;
							inv = inv->next)
					{
						for (b = &inv->buckets[0];
							b->start + (__sync_bool_compare_and_swap(
										&keep_running, 1, 1) ?
								 		inv->expire : s->expire) < now; b++)
						{
							if (!__sync_bool_compare_and_swap(
										&b->state, A_OPEN, A_EXPIRE))
								continue;

							/* avoid emitting empty/unitialised data */
							if (b->cnt > 0) {
								/* write_metric unlocks before doing the
								 * write to avoid any blocks */
								write_metric(s, b, c, inv->metric);
								pthread_rwlock_rdlock(&c->invlock);
							}

							work++;
						}
					}
				}
				pthread_rwlock_unlock(&c->invlock);
			}

			/* now do some cleanup for A_EXPIREd buckets in an exclusive
			 * lock */
			for (c = s->computes; c != NULL; c = c->next) {
				pthread_rwlock_wrlock(&c->invlock);
				for (i = 0; i < (1 << AGGR_HT_POW_SIZE); i++) {
					lastinv = NULL;
					isempty = 0;
					for (inv = c->invocations_ht[i]; inv != NULL; ) {
						/* move expired buckets to the end */
						b = &inv->buckets[0];
						while (b->state == A_EXPIRE) {
							len = b->entries.size;
							values = b->entries.values;
							memmove(b, &inv->buckets[1],
									sizeof(*b) * (s->bucketcnt - 1));
							b = &inv->buckets[s->bucketcnt - 1];
							b->cnt = 0;
							b->start =
								inv->buckets[s->bucketcnt - 2].start +
								s->interval;
							b->entries.size = len;
							b->entries.values = values;
							b->state = A_OPEN;

							b = &inv->buckets[0];
						}
						/* see if the entire invocation is empty */
						isempty = 1;
						for (j = 0; j < s->bucketcnt; j++) {
							if (inv->buckets[j].cnt != 0) {
								isempty = 0;
								break;
							}
						}
						if (isempty) {
							/* free and unlink */
							if (c->entries_needed)
								for (j = 0; j < s->bucketcnt; j++)
									if (inv->buckets[j].entries.values)
										free(inv->buckets[j].entries.values);
							free(inv->metric);
							for (j = 0; j < s->bucketcnt; j++)
								pthread_mutex_destroy(&inv->buckets[j].bcklock);
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
						} else {
							lastinv = inv;
							inv = inv->next;
						}
					}
				}
				pthread_rwlock_unlock(&c->invlock);
			}
		}

		if (work == 0) {
			if (__sync_bool_compare_and_swap(&keep_running, 0, 0))
				break;
			/* nothing done, avoid spinlocking */
			usleep(250 * 1000);  /* 250ms */
		}
	}

	/* free up value buckets */
	while ((s = aggrs) != NULL) {
		while (s->computes != NULL) {
			c = s->computes;

			free((void *)c->metric);
			for (i = 0; i < 1 << AGGR_HT_POW_SIZE; i++) {
				inv = c->invocations_ht[i];

				while (inv != NULL) {
					struct _aggr_invocations *invocation = inv;

					free(inv->metric);
					if (c->entries_needed)
						for (j = 0; j < s->bucketcnt; j++)
							if (inv->buckets[j].entries.values)
								free(inv->buckets[j].entries.values);
					for (j = 0; j < s->bucketcnt; j++)
						pthread_mutex_destroy(&inv->buckets[j].bcklock);
					free(inv->buckets);

					inv = invocation->next;
					free(invocation);
				}
			}

			pthread_rwlock_destroy(&c->invlock);

			s->computes = c->next;
			free(c);
		}
		aggrs = aggrs->next;
		free(s);
	}

	return NULL;
}

/**
 * Returns the number of aggregators defined.
 */
size_t
aggregator_numaggregators(aggregator *aggrs)
{
	size_t totaggregators = 0;
	aggregator *a;

	for (a = aggrs; a != NULL; a = a->next)
		totaggregators++;

	return totaggregators;
}

/**
 * Returns the total number of computations defined.
 */
size_t
aggregator_numcomputes(aggregator *aggrs)
{
	size_t totcomputes = 0;
	aggregator *a;
	struct _aggr_computes *c;

	for (a = aggrs; a != NULL; a = a->next)
		for (c = a->computes; c != NULL; c = c->next)
			totcomputes++;

	return totcomputes;
}

/**
 * Initialises and starts the aggregator.  Returns false when starting
 * failed, true otherwise.
 */
int
aggregator_start(aggregator *aggrs)
{
	keep_running = 1;
	if (pthread_create(&aggregatorid, NULL, aggregator_expire, aggrs) != 0)
		return 0;

	return 1;
}

/**
 * Shuts down the aggregator.
 */
void
aggregator_stop(void)
{
	__sync_bool_compare_and_swap(&keep_running, 1, 0);
	pthread_join(aggregatorid, NULL);
}

/**
 * Returns an approximate number of received metrics by all aggregators.
 */
size_t
aggregator_get_received(aggregator *a)
{
	size_t totreceived = 0;

	for ( ; a != NULL; a = a->next)
		totreceived += __sync_add_and_fetch(&a->received, 0);

	return totreceived;
}

/**
 * Returns an approximate number of metrics received by all aggregators
 * since the last call to this function.
 */
inline size_t
aggregator_get_received_sub(aggregator *aggrs)
{
	size_t r = aggregator_get_received(aggrs) - prevreceived;
	prevreceived += r;
	return r;
}

/**
 * Returns an approximate number of metrics sent by all aggregators.
 */
size_t
aggregator_get_sent(aggregator *a)
{
	size_t totsent = 0;

	for ( ; a != NULL; a = a->next)
		totsent += __sync_add_and_fetch(&a->sent, 0);

	return totsent;
}

/**
 * Returns an approximate number of metrics sent by all aggregators
 * since the last call to this function.
 */
inline size_t
aggregator_get_sent_sub(aggregator *aggrs)
{
	size_t r = aggregator_get_sent(aggrs) - prevsent;
	prevsent += r;
	return r;
}

/**
 * Returns an approximate number of dropped metrics by all aggregators.
 * Metrics are dropped if they are too much in the past (past expiry
 * time) or if they are too much in the future.
 */
size_t
aggregator_get_dropped(aggregator *a)
{
	size_t totdropped = 0;

	for ( ; a != NULL; a = a->next)
		totdropped += __sync_add_and_fetch(&a->dropped, 0);

	return totdropped;
}

/**
 * Returns an approximate number of metrics dropped by all aggregators
 * since the last call to this function.
 */
inline size_t
aggregator_get_dropped_sub(aggregator *aggrs)
{
	size_t r = aggregator_get_dropped(aggrs) - prevdropped;
	prevdropped += r;
	return r;
}
