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
#include <string.h>
#include <assert.h>

#include "fnv1a.h"
#include "md5.h"
#include "server.h"
#include "allocator.h"

#define CH_RING struct _ch_ring
#include "consistent-hash.h"


/* This value is hardwired in the carbon sources, and necessary to get
 * fair (re)balancing of metrics in the hash ring.  Because the value
 * seems reasonable, we use the same value for carbon and fnv1a hash
 * implementations. */
#define HASH_REPLICAS  100

typedef struct _ring_entry {
	unsigned short pos;
	server *server;
} ch_ring_entry;

struct _ch_ring {
	ch_type type;
	unsigned char hash_replicas;
	ch_ring_entry *entrylist;
	int entrysize;
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

	assert(bckcnt > 0);  /* help static code analysis */
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
	ch_ring_entry *ch_l = (ch_ring_entry *)l;
	ch_ring_entry *ch_r = (ch_ring_entry *)r;

	char *si_l = server_instance(ch_l->server);
	char *si_r = server_instance(ch_r->server);
	char *endp;
	long val_l;
	long val_r;

	/* order such that servers with explicit instance come first
	 * (sorted), all servers without instance follow in order of their
	 * definition in the config file (as per documentation) */
	if (si_l == NULL && si_r == NULL)
		return ch_l->pos - ch_r->pos;
	if (si_l == NULL)
		return 1;
	if (si_r == NULL)
		return -1;
	
	/* at this point we have two instances, try and see if both are
	 * numbers, if so, compare as such, else fall back to string
	 * comparison */
	val_l = strtol(si_l, &endp, 10);
	if (*endp == '\0') {
		val_r = strtol(si_r, &endp, 10);
		if (*endp == '\0')
			return (int)(val_l - val_r);
	}
	return strcmp(si_l, si_r);
}

ch_ring *
ch_new(allocator *a, ch_type type, int servercnt)
{
	ch_ring *ret;

	if (servercnt > CONN_DESTS_SIZE) {
		logerr("ch_addnode: nodes in use exceeds CONN_DESTS_SIZE, "
				"increase CONN_DESTS_SIZE in router.h\n");
		return NULL;
	}

	ret = ra_malloc(a, sizeof(ch_ring));
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
	ret->entrycnt = 0;
	ret->entrysize = ret->hash_replicas * servercnt;
	ret->entrylist = ra_malloc(a, sizeof(ch_ring_entry) * ret->entrysize);
	if (ret->entrylist == NULL) {
		free(ret);
		return NULL;
	}

	return ret;
}

/**
 * Computes the hash positions for the server name given.  This is based
 * on the hashpos function.  The server name usually is the IPv4
 * address.  The port component is just stored and not used in the
 * carbon hash calculation in case of carbon_ch.  The instance component
 * is used in addition in the hash calculation of carbon_ch.  For
 * fnv1a_ch it is used solely when defined, e.g. disregarding host and
 * port.  Returns an updated ring once the last server is added.
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
	entries = &ring->entrylist[ring->entrycnt];

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
			}
			cmp = *entrycmp_fnv1a;
			break;
		case JUMP_FNV1a:
			entries[0].pos = ring->entrycnt;
			entries[0].server = s;
			cmp = *entrycmp_jump_fnv1a;
			break;
	}

	ring->entrycnt += ring->hash_replicas;
	if (ring->entrycnt == ring->entrysize) {
		qsort(&ring->entrylist[0], ring->entrycnt, sizeof(ch_ring_entry), cmp);

		/* overwrite successive duplicates with the first entry to ensure
		 * our binary search hits the "first" matching entry */
		for (i = 0; i < ring->entrycnt - 1; i++) {
			if (ring->entrylist[i].pos == ring->entrylist[i + 1].pos)
				ring->entrylist[i + 1].server = ring->entrylist[i].server;
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
	unsigned short pos = 0;
	int i, j, t;

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
			server *bcklst[CONN_DESTS_SIZE];
			const char *p;

			pos = replcnt;

			/* we know this fits, since ch_new checks i <= CONN_DESTS_SIZE */
			for (i = 0; i < ring->entrycnt; i++)
				bcklst[i] = ring->entrylist[i].server;
			fnv1a_64(hash, p, metric, firstspace);

			while (i > 0) {
				j = jump_bucketpos(hash, i);

				(*ret).dest = bcklst[j];
				ret++;

				if (--pos == 0)
					break;

				/* use xorshift to generate a different hash for input
				 * in the jump hash again */
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

	assert(ring->entrylist);

	/* binary search for first entry.pos >= pos */
	i = 0;
	j = ring->entrycnt;
	t = 0;
	/* 1 3 4 6 6 8 8 9 */
	while (1) {
		t = (i + j) / 2;
		if (ring->entrylist[t].pos == pos)
			break;  /* exact match */
		if (ring->entrylist[t].pos < pos) {
			i = t;  /* left is smaller, search right */
			if (i == j - 1) {
				if (ring->entrylist[j].pos < pos) {
					t = j + 1;
					break;  /* right pos is smaller than the value */
				} else {
					t = j;
					break;  /* right pos is the first bigger value */
				}
			}
		} else {
			j = t;  /* right is bigger, search left */
			if (i == j - 1) {
				if (ring->entrylist[i].pos >= pos) {
					t = i;
					break;  /* left pos is the first bigger than value */
				} else {
					t = j;
					break;  /* right pos is the first bigger value */
				}
			}
		}
	}

	/* now fetch enough unique servers to match the requested count */
	for (i = 0; i < replcnt; i++, t++) {
		if (t >= ring->entrycnt)
			t = 0;
		for (j = i - 1; j >= 0; j--) {
			if (ret[j].dest == ring->entrylist[t].server) {
				j = i;
				break;
			}
		}
		if (j == i) {
			i--;
			continue;
		}
		ret[i].dest = ring->entrylist[t].server;
	}
}

void
ch_printhashring(ch_ring *ring, FILE *f)
{
	ch_ring_entry *w;
	char column = 0;
	char srvbuf[21];
	int i;

	for (i = 0; i < ring->entrycnt; i++) {
		w = &ring->entrylist[i];
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
