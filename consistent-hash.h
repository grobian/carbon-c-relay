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


#ifndef CONSISTENT_HASH_H
#define CONSISTENT_HASH_H 1

#include "server.h"
#include "router.h"

#ifndef CH_RING
#define CH_RING void
#endif
typedef CH_RING ch_ring;
typedef enum { CARBON, FNV1a } ch_type;

ch_ring *ch_new(ch_type type);
ch_ring *ch_addnode(ch_ring *ring, server *s);
void ch_get_nodes(
		destination ret[],
		ch_ring *ring,
		const char replcnt,
		const char *metric,
		const char *firstspace);
void ch_free(ch_ring *ring);

#endif
