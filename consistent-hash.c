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


#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#include "fnv1a.h"
#include "md5.h"
#include "server.h"

#define CH_RING struct _ch_ring
#include "consistent-hash.h"


/* This value is hardwired in the carbon sources, and necessary to get
 * fair (re)balancing of metrics in the hash ring.  Because the value
 * seems reasonable, we use the same value for carbon and fnv1a hash
 * implementations. */
#define HASH_REPLICAS  100
#define MAX_RING_ENTRIES 65536

typedef struct _ring_entry {
	unsigned short pos;
	unsigned char malloced:1;
	server *server;
	struct _ring_entry *next;
} ch_ring_entry;

struct _ch_ring {
	ch_type type;
	unsigned char hash_replicas;
	ch_ring_entry *entries;
	ch_ring_entry **entrylist;  /* only used with jump hash */
	ch_ring_entry* entryarray[MAX_RING_ENTRIES];  /* used for binary search */
	int entrycnt;
};


/**
 * Computes the hash position for key in a 16-bit unsigned integer
 * space.  Returns a number between 0 and 65535 based on the highest 2
 * bytes of the MD5 sum of key.
 */
static unsigned short
carbon_hashpos(const char *key, const char *end)
{
	unsigned char md5[MD5_DIGEST_LENGTH];

	MD5((unsigned char *)key, end - key, md5);

	return ((md5[0] << 8) + md5[1]);
}

/**
 * Computes the hash position for key in a 16-bit unsigned integer
 * space.  Returns a number between 0 and 65535 based on the FNV1a hash
 * algorithm.
 */
static unsigned short
fnv1a_hashpos(const char *key, const char *end)
{
	unsigned int hash;

	fnv1a_32(hash, key, key, end);

	return (unsigned short)((hash >> 16) ^ (hash & (unsigned int)0xFFFF));
}

/**
 * Computes the bucket number for key in the range [0, bckcnt).  The
 * algorithm used is the jump consistent hash by Lamping and Veach.
 */
static unsigned int
jump_bucketpos(unsigned long long int key, int bckcnt)
{
	long long int b = -1, j = 0;

	while (j < bckcnt) {
		b = j;
		key = key * 2862933555777941757ULL + 1;
		j = (long long int)((double)(b + 1) *
				((double)(1LL << 31) / (double)((key >> 33) + 1))
			);
	}

	/* b cannot exceed the range of bckcnt, see while condition */
	return (unsigned int)b;
}

/**
 * Sort comparator for ch_ring_entry structs on pos, ip and instance.
 */
static int
entrycmp_carbon(const void *l, const void *r)
{
	ch_ring_entry *ch_l = (ch_ring_entry *)l;
	ch_ring_entry *ch_r = (ch_ring_entry *)r;

	if (ch_l->pos != ch_r->pos)
		return ch_l->pos - ch_r->pos;

#ifndef CH_CMP_V40_BEHAVIOUR
	{
		int d = strcmp(server_ip(ch_l->server), server_ip(ch_r->server));
		char *i_l, *i_r;
		if (d != 0)
			return d;
		i_l = server_instance(ch_l->server);
		i_r = server_instance(ch_r->server);
		if (i_l == NULL && i_r == NULL)
			return 0;
		if (i_l == NULL)
			return 1;
		if (i_r == NULL)
			return -1;
		return strcmp(i_l, i_r);
	}
#endif

	return 0;
}

/**
 * Sort comparator for ch_ring_entry structs on pos, ip and port.
 */
static int
entrycmp_fnv1a(const void *l, const void *r)
{
	ch_ring_entry *ch_l = (ch_ring_entry *)l;
	ch_ring_entry *ch_r = (ch_ring_entry *)r;

	if (ch_l->pos != ch_r->pos)
		return ch_l->pos - ch_r->pos;

#ifndef CH_CMP_V40_BEHAVIOUR
	{
		int d = strcmp(server_ip(ch_l->server), server_ip(ch_r->server));
		if (d != 0)
			return d;
		return server_port(ch_l->server) - server_port(ch_r->server);
	}
#endif

	return 0;
}

/**
 * Sort comparator for ch_ring_entry structs on instance only.
 */
static int
entrycmp_jump_fnv1a(const void *l, const void *r)
{
	char *si_l = server_instance(((ch_ring_entry *)l)->server);
	char *si_r = server_instance(((ch_ring_entry *)r)->server);

	return strcmp(si_l ? si_l : "", si_r ? si_r : "");
}

ch_ring *
ch_new(ch_type type)
{
	ch_ring *ret = malloc(sizeof(ch_ring));

	if (ret == NULL)
		return NULL;
	ret->type = type;
	switch (ret->type) {
		case CARBON:
		case FNV1a:
			ret->hash_replicas = HASH_REPLICAS;
			break;
		default:
			ret->hash_replicas = 1;
			break;
	}
	ret->entries = NULL;
	ret->entrylist = NULL;
	ret->entrycnt = 0;

	return ret;
}

/**
 * Computes the hash positions for the server name given.  This is based
 * on the hashpos function.  The server name usually is the IPv4
 * address.  The port component is just stored and not used in the
 * carbon hash calculation in case of carbon_ch.  The instance component
 * is used in the hash calculation of carbon_ch, it is ignored for
 * fnv1a_ch.  Returns an updated ring.
 */
ch_ring *
ch_addnode(ch_ring *ring, server *s)
{
	int i;
	char buf[256];
	ch_ring_entry *entries;
	char *instance = server_instance(s);
	int (*cmp)(const void *, const void *) = NULL;

	if (ring == NULL)
		return NULL;

	entries =
		(ch_ring_entry *)malloc(sizeof(ch_ring_entry) * ring->hash_replicas);
	if (entries == NULL)
		return NULL;

	switch (ring->type) {
		case CARBON:
			for (i = 0; i < ring->hash_replicas; i++) {
				/* this format is actually Python's tuple format that is
				 * used in serialised form as input for the hash */
				snprintf(buf, sizeof(buf), "('%s', %s%s%s):%d",
						server_ip(s),
						instance == NULL ? "" : "'",
						instance == NULL ? "None" : instance,
						instance == NULL ? "" : "'",
						i);
				/* carbon upstream committed:
				 * https://github.com/graphite-project/carbon/commit/024f9e67ca47619438951c59154c0dec0b0518c7
				 * this makes sure no collissions exist on pos, however,
				 * at the expense of being agnostic to the input order,
				 * therefore that change isn't implemented here, see
				 * https://github.com/grobian/carbon-c-relay/issues/84 */
				entries[i].pos = carbon_hashpos(buf, buf + strlen(buf));
				entries[i].server = s;
				entries[i].next = NULL;
				entries[i].malloced = 0;
			}
			cmp = *entrycmp_carbon;
			break;
		case FNV1a:
			for (i = 0; i < ring->hash_replicas; i++) {
				/* take all server info into account, such that
				 * different port numbers for the same hosts will work
				 * (unlike CARBON), unless we got a full overrride */
				if (instance == NULL) {
					snprintf(buf, sizeof(buf), "%d-%s:%u",
							i, server_ip(s), server_port(s));
				} else {
					snprintf(buf, sizeof(buf), "%d-%s", i, instance);
				}
				entries[i].pos = fnv1a_hashpos(buf, buf + strlen(buf));
				entries[i].server = s;
				entries[i].next = NULL;
				entries[i].malloced = 0;
			}
			cmp = *entrycmp_fnv1a;
			break;
		case JUMP_FNV1a:
			entries[0].pos = 0;
			entries[0].server = s;
			entries[0].next = NULL;
			entries[0].malloced = 0;
			cmp = *entrycmp_jump_fnv1a;
			break;
	}

	/* sort to allow merge joins later down the road */
	qsort(entries, ring->hash_replicas, sizeof(ch_ring_entry), cmp);
	entries[0].malloced = 1;

	if (ring->entries == NULL) {
		for (i = 1; i < ring->hash_replicas; i++)
			entries[i - 1].next = &entries[i];
		ring->entries = entries;
	} else {
		/* merge-join the two rings */
		ch_ring_entry *w, *last;
		i = 0;
		last = NULL;
		assert(ring->hash_replicas > 0);
		for (w = ring->entries; w != NULL && i < ring->hash_replicas; ) {
			if (cmp(w, &entries[i]) <= 0) {
				last = w;
				w = w->next;
			} else {
				entries[i].next = w;
				if (last == NULL) {
					ring->entries = &entries[i];
				} else {
					last->next = &entries[i];
				}
				last = &entries[i];
				i++;
			}
		}
		if (w != NULL) {
			last->next = w;
		} else {
			last->next = &entries[i];
			for (i = i + 1; i < ring->hash_replicas; i++)
				entries[i - 1].next = &entries[i];
		}
	}

	if (ring->type == JUMP_FNV1a) {
		ch_ring_entry *w;

		/* count the ring, pos is purely cosmetic, it isn't used */
		for (w = ring->entries, i = 0; w != NULL; w = w->next, i++)
			w->pos = i;
		ring->entrycnt = i;
		/* this is really wasteful, but optimising this isn't worth it
		 * since it's called only a few times during config parsing */
		if (ring->entrylist != NULL)
			free(ring->entrylist);
		ring->entrylist = malloc(sizeof(ch_ring_entry *) * ring->entrycnt);
		for (w = ring->entries, i = 0; w != NULL; w = w->next, i++)
			ring->entrylist[i] = w;

		if (i == CONN_DESTS_SIZE) {
			logerr("ch_addnode: nodes in use exceeds CONN_DESTS_SIZE, "
					"increase CONN_DESTS_SIZE in router.h\n");
			return NULL;
		}
	} else {
                // Generate ring->entryarray[pos]=entry in order to speed up ch_get_nodes()
		ch_ring_entry *w;

		memset(ring->entryarray, 0, sizeof(ring->entryarray));

		for (w = ring->entries; w != NULL; w = w->next)
			// Collision avoidance.  First entry wins.
			if (ring->entryarray[w->pos] == 0)
				ring->entryarray[w->pos] = w;

		for (i=MAX_RING_ENTRIES-2; i>=0; i--) {
			if (ring->entryarray[i] == 0)
				ring->entryarray[i] = ring->entryarray[i+1];
		}
		// Fill in tail of array;
		for (i=MAX_RING_ENTRIES-1; ring->entryarray[i]==0; i--) {
			ring->entryarray[i] = ring->entryarray[0];
		}
	}

	return ring;
}

/**
 * Retrieve the nodes responsible for storing the given metric.  The
 * replcnt argument specifies how many hosts should be retrieved.
 * Results are stored in ret, an array of ch_ring pointers.  The
 * caller is responsible for ensuring that ret is large enough to store
 * replcnt pointers.
 */
void
ch_get_nodes(
		destination ret[],
		ch_ring *ring,
		const char replcnt,
		const char *metric,
		const char *firstspace)
{
	ch_ring_entry *w;
	unsigned short pos = 0;
	int i, j;

	switch (ring->type) {
		case CARBON:
			pos = carbon_hashpos(metric, firstspace);
			break;
		case FNV1a:
			pos = fnv1a_hashpos(metric, firstspace);
			break;
		case JUMP_FNV1a: {
			/* this is really a short route, since the jump hash gives
			 * us a bucket immediately */
			unsigned long long int hash;
			ch_ring_entry *bcklst[CONN_DESTS_SIZE];
			const char *p;

			i = ring->entrycnt;
			pos = replcnt;

			memcpy(bcklst, ring->entrylist, sizeof(bcklst[0]) * i);
			fnv1a_64(hash, p, metric, firstspace);

			while (i > 0) {
				j = jump_bucketpos(hash, i);

				(*ret).dest = bcklst[j]->server;
				(*ret).metric = strdup(metric);
				ret++;

				if (--pos == 0)
					break;

				/* use xorshift to generate a different hash for input
				 * in the hump hash again */
				hash ^= hash >> 12;
				hash ^= hash << 25;
				hash ^= hash >> 27;
				hash *= 2685821657736338717ULL;

				/* remove the server we just selected, such that we can
				 * be sure the next iteration will fetch another server */
				bcklst[j] = bcklst[--i];
			}
		}	return;
	}

	assert(ring->entries);

	/* Instead of Python's bisect_left or doing forward pointer chaising
	 * the ring's entryarray contains all possible pos values pointing
	 * to the appropriate ring->entries values */
	w = ring->entryarray[pos];

	/* now fetch enough unique servers to match the requested count */
	for (i = 0; i < replcnt; i++, w = w->next) {
		if (w == NULL)
			w = ring->entries;
		for (j = i - 1; j >= 0; j--) {
			if (ret[j].dest == w->server) {
				j = i;
				break;
			}
		}
		if (j == i) {
			i--;
			continue;
		}
		ret[i].dest = w->server;
		ret[i].metric = strdup(metric);
	}
}

void
ch_printhashring(ch_ring *ring, FILE *f)
{
	ch_ring_entry *w;
	char column = 0;
	char srvbuf[21];

	for (w = ring->entries; w != NULL; w = w->next) {
		snprintf(srvbuf, sizeof(srvbuf), "%s:%d%s%s",
				server_ip(w->server),
				server_port(w->server),
				server_instance(w->server) ? "=" : "",
				server_instance(w->server) ? server_instance(w->server) : "");
		fprintf(f, "%5d@%-20s", w->pos, srvbuf);
		if (column < 2) {
			fprintf(f, " ");
			column++;
		} else {
			fprintf(f, "\n");
			column = 0;
		}
	}
	if (column != 0)
		fprintf(f, "\n");
}

unsigned short
ch_gethashpos(ch_ring *ring, const char *key, const char *end)
{
	switch (ring->type) {
		case CARBON:
			return carbon_hashpos(key, end);
		case FNV1a:
			return fnv1a_hashpos(key, end);
		case JUMP_FNV1a: {
			unsigned long long int hash;
			fnv1a_64(hash, key, key, end);
			return jump_bucketpos(hash, ring->entrycnt);
		}
		default:
			assert(0);  /* this shouldn't happen */
	}

	return 0;  /* pacify compiler */
}

/**
 * Frees the ring structure and its added nodes, leaves the referenced
 * servers untouched.
 */
void
ch_free(ch_ring *ring)
{
	ch_ring_entry *deletes = NULL;
	ch_ring_entry *w = NULL;

	for (; ring->entries != NULL; ring->entries = ring->entries->next) {
		if (ring->entries->malloced) {
			if (deletes == NULL) {
				w = deletes = ring->entries;
			} else {
				w = w->next = ring->entries;
			}
		}
	}

	if (w != NULL) {
		w->next = NULL;
		while (deletes != NULL) {
			w = deletes->next;
			free(deletes);
			deletes = w;
		}
	}

	if (ring->entrylist != NULL)
		free(ring->entrylist);

	free(ring);
}
