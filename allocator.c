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

#include <stdlib.h>
#include <string.h>
#include <assert.h>

#include "allocator.h"

struct _cr_allocator {
	void *memory_region;
	void *nextp;
	size_t sz;
	struct _cr_allocator *next;
};

/**
 * Free the resources associated to this allocator.
 */
void
ra_free(allocator *ra)
{
	allocator *ra_next;

	for ( ; ra != NULL; ra = ra_next) {
		free(ra->memory_region);
		ra_next = ra->next;
		free(ra);
		ra = NULL;
	}
}

#define ra_alloc(RA, SZ) { \
	assert(SZ >= 0); \
	nsz = 256 * 1024; \
	if (SZ > nsz) \
		nsz = ((SZ / 1024) + 1) * 1024; \
	RA = malloc(sizeof(allocator)); \
	if (RA == NULL) \
		return NULL; \
	RA->memory_region = malloc(sizeof(char) * nsz); \
	if (RA->memory_region == NULL) { \
		free(RA); \
		RA = NULL; \
		return NULL; \
	} \
	RA->nextp = RA->memory_region; \
	RA->sz = nsz; \
	RA->next = NULL; \
}

/**
 * Allocate a new allocator.
 */
allocator *
ra_new(void)
{
	allocator *ret;
	size_t nsz;

	ra_alloc(ret, 0);

	return ret;
}

/**
 * malloc() in one of the regions for this allocators.  If insufficient
 * memory is available in the region, a new one is allocated.  If
 * that fails, NULL is returned, else a pointer that can be written to
 * up to sz bytes.
 */
void *
ra_malloc(allocator *ra, size_t sz)
{
	void *retp = NULL;
	size_t nsz;

	for (; ra != NULL; ra = ra->next) {
		if (ra->sz - (ra->nextp - ra->memory_region) >= sz) {
			retp = ra->nextp;
			ra->nextp += sz;
			return retp;
		}
		if (ra->next == NULL)
			ra_alloc(ra->next, sz);
	}

	/* this should be unreachable code */
	return NULL;
}

/**
 * strdup using ra_malloc, e.g. get memory from the region associated
 * to the given allocated.
 */
char *
ra_strdup(allocator *ra, const char *s)
{
	size_t sz = strlen(s) + 1;
	char *m = ra_malloc(ra, sz);
	if (m == NULL)
		return m;
	memcpy(m, s, sz);
	return m;
}

