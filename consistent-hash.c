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
#include <string.h>
#include <assert.h>

#include "md5.h"
#include "fnv1a.h"
#include "server.h"

#define CH_RING struct _ch_ring
#include "consistent-hash.h"


/* This value is hardwired in the carbon sources, and necessary to get
 * fair (re)balancing of metrics in the hash ring.  Because the value
 * seems reasonable, we use the same value for all hash implementations. */
#define HASH_REPLICAS  100

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

ch_ring *
ch_new(ch_type type)
{
	ch_ring *ret = malloc(sizeof(ch_ring));

	if (ret == NULL)
		return NULL;
	ret->type = type;
	ret->hash_replicas = HASH_REPLICAS;
	ret->entries = NULL;

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
				entries[i].pos = fnv1a_hash16(buf, buf + strlen(buf));
				entries[i].server = s;
				entries[i].next = NULL;
				entries[i].malloced = 0;
			}
			cmp = *entrycmp_fnv1a;
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
			if (cmp(&w->pos, &entries[i].pos) <= 0) {
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
			pos = fnv1a_hash16(metric, firstspace);
			break;
	}

	assert(ring->entries);

	/* implement behaviour of Python's bisect_left on the ring (used in
	 * carbon hash source), one day we might want to implement it as
	 * real binary search iso forward pointer chasing */
	for (w = ring->entries, i = 0; w != NULL; i++, w = w->next)
		if (w->pos >= pos)
			break;
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
}

unsigned short
ch_gethashpos(ch_ring *ring, const char *key, const char *end)
{
	switch (ring->type) {
		case CARBON:
			return carbon_hashpos(key, end);
		case FNV1a:
			return fnv1a_hash16(key, end);
		default:
			assert(0);  /* this shouldn't happen */
	}

	return 0;  /* pacify compiler */
}

/**
 * Frees the ring structure and its added nodes.
 */
void
ch_free(ch_ring *ring)
{
	ch_ring_entry *deletes = NULL;
	ch_ring_entry *w = NULL;

	for (; ring->entries != NULL; ring->entries = ring->entries->next) {
		server_shutdown(ring->entries->server);

		if (ring->entries->malloced) {
			if (deletes == NULL) {
				w = deletes = ring->entries;
			} else {
				w = w->next = ring->entries;
			}
		}
	}

	assert(w != NULL);
	w->next = NULL;
	while (deletes != NULL) {
		w = deletes->next;
		free(deletes);
		deletes = w;
	}

	free(ring);
}
