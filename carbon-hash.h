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


/* this is hardwired in the carbon sources, and necessary to get
 * fair (re)balancing of metrics in the hash ring. */
#define CARBON_REPLICAS  100

typedef struct _ring_entry {
	unsigned short pos;
	const char *server;
	unsigned short port;
	struct _ring_entry *next;
} carbon_ring;

carbon_ring *carbon_addnode(
		carbon_ring *ring,
		const char *host,
		const unsigned short port);
void carbon_get_nodes(
		carbon_ring *ret[],
		carbon_ring *ring,
		const char replcnt,
		const char *metric);
