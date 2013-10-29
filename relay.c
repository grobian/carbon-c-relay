/*
 *  This file is part of carbon-c-relay.
 *
 *  dnspq is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  dnspq is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with dnspq.  If not, see <http://www.gnu.org/licenses/>.
 */


#include <stdio.h>

#include "carbon-hash.h"


int main() {
	carbon_ring *ring;

	printf("%s: %d\n", "('ip1', None):1", carbon_hashpos("('ip1', None):1"));
	ring = carbon_addnode(NULL, "ip1", 2003);
	ring = carbon_addnode(ring, "ip2", 2003);

	for (; ring != NULL; ring = ring->next)
		printf("(%d, %s:%d)\n", ring->pos, ring->server, ring->port);
	return 0;
}
