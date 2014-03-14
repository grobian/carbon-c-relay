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

#ifndef CONSISTENT_HASH_H
#define CONSISTENT_HASH_H 1

#include "server.h"

#ifndef CH_RING
#define CH_RING void
#endif
typedef CH_RING ch_ring;
typedef enum { CARBON, FNV1a } ch_type;

ch_ring *ch_new(ch_type type);
ch_ring *ch_addnode(ch_ring *ring, server *s);
void ch_get_nodes(
		server *ret[],
		ch_ring *ring,
		const char replcnt,
		const char *metric);

#endif
