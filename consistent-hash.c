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
#include <openssl/md5.h>
#include <assert.h>

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
 * Computes the hash position for key in a 16-bit unsigned integer
 * space.  Returns a number between 0 and 65535 based on the FNV1a hash
 * algorithm.
 */
static unsigned short
fnv1a_hashpos(const char *key, const char *end)
{
	unsigned int hash = 2166136261UL;  /* FNV1a */

	for (; key < end; key++)
		hash = (hash ^ (unsigned int)*key) * 16777619;

	return (unsigned short)((hash >> 16) ^ (hash & (unsigned int)0xFFFF));
}

/**
 * Qsort comparator for ch_ring_entry structs on pos.
 */
static int
entrycmp(const void *l, const void *r)
{
	return ((ch_ring_entry *)l)->pos - ((ch_ring_entry *)r)->pos;
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

	if (ring == NULL)
		return NULL;

	entries =
		(ch_ring_entry *)malloc(sizeof(ch_ring_entry) * ring->hash_replicas);
	if (entries == NULL)
		return NULL;

	switch (ring->type) {
		case CARBON:
			for (i = 0; i < ring->hash_replicas; i++) {
				char *instance = server_instance(s);
				/* this format is actually Python's tuple format that is
				 * used in serialised form as input for the hash */
				snprintf(buf, sizeof(buf), "('%s', %s%s%s):%d",
						server_ip(s),
						instance == NULL ? "" : "'",
						instance == NULL ? "None" : instance,
						instance == NULL ? "" : "'",
						i);
				/* TODO:
				 * https://github.com/graphite-project/carbon/commit/024f9e67ca47619438951c59154c0dec0b0518c7
				 * Question is how harmful the collision is -- it will probably
				 * change the location of some metrics */
				entries[i].pos = carbon_hashpos(buf, buf + strlen(buf));
				entries[i].server = s;
				entries[i].next = NULL;
				entries[i].malloced = 0;
			}
			break;
		case FNV1a:
			for (i = 0; i < ring->hash_replicas; i++) {
				/* take all server info into account, such that
				 * different port numbers for the same hosts will work
				 * (unlike CARBON) */
				snprintf(buf, sizeof(buf), "%d-%s:%u",
						i, server_ip(s), server_port(s));
				entries[i].pos = fnv1a_hashpos(buf, buf + strlen(buf));
				entries[i].server = s;
				entries[i].next = NULL;
				entries[i].malloced = 0;
			}
			break;
	}

	/* sort to allow merge joins later down the road */
	qsort(entries, ring->hash_replicas, sizeof(ch_ring_entry), *entrycmp);
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
			if (w->pos < entries[i].pos) {
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
			pos = fnv1a_hashpos(metric, firstspace);
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
