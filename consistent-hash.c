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
#include <string.h>
#include <openssl/md5.h>

#include "server.h"

#define CH_RING struct _ch_ring
#include "consistent-hash.h"


/* This value is hardwired in the carbon sources, and necessary to get
 * fair (re)balancing of metrics in the hash ring.  Because the value
 * seems reasonable, we use the same value for all hash implementations. */
#define HASH_REPLICAS  100

typedef struct _ring_entry {
	unsigned short pos;
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
carbon_hashpos(const char *key)
{
	unsigned char md5[MD5_DIGEST_LENGTH];

	MD5((unsigned char *)key, strlen(key), md5);

	return ((md5[0] << 8) + md5[1]);
}

/**
 * Computes the hash position for key in a 16-bit unsigned integer
 * space.  Returns a number between 0 and 65535 based on the FNV1a hash
 * algorithm.
 */
static unsigned short
fnv1a_hashpos(const char *key)
{
	// TODO: lookup short start hash value and multiplication value */
	size_t hash = 2166136261UL;  /* FNV1a */

	for (; *key != '\0'; key++)
		hash = (hash ^ (size_t)*key) * 16777619;

	return (unsigned short)hash;
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
 * carbon hash calculation.  Returns an updated ring.
 */
ch_ring *
ch_addnode(ch_ring *ring, server *s)
{
	int i;
	char buf[256];
	ch_ring_entry *entries =
		(ch_ring_entry *)malloc(sizeof(ch_ring_entry) * ring->hash_replicas);

	if (entries == NULL)
		return NULL;
	if (ring == NULL) {
		free(entries);
		return NULL;
	}

	switch (ring->type) {
		case CARBON:
			for (i = 0; i < ring->hash_replicas; i++) {
				/* this format is actually Python's tuple format that is
				 * used in serialised form as input for the hash */
				snprintf(buf, sizeof(buf), "('%s', None):%d", server_ip(s), i);
				/* TODO:
				 * https://github.com/graphite-project/carbon/commit/024f9e67ca47619438951c59154c0dec0b0518c7
				 * Question is how harmful the collision is -- it will probably
				 * change the location of some metrics */
				entries[i].pos = carbon_hashpos(buf);
				entries[i].server = s;
				entries[i].next = NULL;
			}
			break;
		case FNV1a:
			for (i = 0; i < ring->hash_replicas; i++) {
				/* take all server info into account, such that
				 * different port numbers for the same hosts will work
				 * (unlike CARBON) */
				snprintf(buf, sizeof(buf), "%d-%s:%u",
						i, server_ip(s), server_port(s));
				entries[i].pos = fnv1a_hashpos(buf);
				entries[i].server = s;
				entries[i].next = NULL;
			}
			break;
	}

	/* sort to allow merge joins later down the road */
	qsort(entries, ring->hash_replicas, sizeof(ch_ring_entry), *entrycmp);

	if (ring->entries == NULL) {
		for (i = 1; i < ring->hash_replicas; i++)
			entries[i - 1].next = &entries[i];
		ring->entries = entries;
	} else {
		/* merge-join the two rings */
		ch_ring_entry *w, *last;
		i = 0;
		last = NULL;
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
		server *ret[],
		ch_ring *ring,
		const char replcnt,
		const char *metric)
{
	ch_ring_entry *w;
	unsigned short pos = 0;
	int i, j;

	switch (ring->type) {
		case CARBON:
			pos = carbon_hashpos(metric);
			break;
		case FNV1a:
			pos = fnv1a_hashpos(metric);
			break;
	}

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
			if (ret[j] == w->server) {
				j = i;
				break;
			}
		}
		if (j == i) {
			i--;
			continue;
		}
		ret[i] = w->server;
	}
}
