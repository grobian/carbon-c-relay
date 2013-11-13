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
#include <string.h>
#include <pthread.h>
#include <errno.h>

#include "carbon-hash.h"
#include "server.h"
#include "router.h"
#include "receptor.h"
#include "dispatcher.h"

int keep_running = 1;

int main() {
	int sock;
	char id;
	pthread_t w;

	router_readconfig("testconf");
	router_printconfig(stdout);

	sock = bindlisten(2003);
	if (sock < 0) {
		fprintf(stderr, "failed to bind on port %d: %s\n",
				2003, strerror(errno));
		return -1;
	}
	if (dispatch_addlistener(sock) != 0) {
		close(sock);
		fprintf(stderr, "failed to add listener\n");
		return -1;
	}

	id = 1;
	pthread_create(&w, NULL, &dispatcher, &id);

	sleep(10);
	fprintf(stdout, "shutting down...\n");
	keep_running = 0;
	router_shutdown();
	pthread_join(w, NULL);

	return 0;
}
