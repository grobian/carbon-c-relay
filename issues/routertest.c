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

/* Simple example of how to use the routing functionality of the relay
 * https://github.com/grobian/carbon-c-relay/issues/192
 *
 * compile with something like:
 * gcc -o routertest routertest.c router.c server.c consistent-hash.c \
 *        md5.c queue.c dispatcher.c aggregator.c -lnsl -lsocket -lm
 */


#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "relay.h"
#include "server.h"
#include "router.h"

unsigned char mode = 0;

relaylog(enum logdst dest, const char *fmt, ...)
{
	return 0;
}

#define DESTS_SIZE 32

int main() {
	size_t len = 0;
	size_t i;
	destination dests[DESTS_SIZE];
	char *metric;
	char *firstspace;
	server *s;
	router *rtr;

	if ((rtr = router_readconfig(NULL, "test.conf", 100, 10, 1, 100)) == NULL)
		exit(1);

	metric = strdup("bla.bla.bla");
	firstspace = metric + strlen(metric);
	(void)router_route(rtr, dests, &len, DESTS_SIZE, "127.0.0.1", metric, firstspace);

	for (i = 0; i < len; i++) {
		free((char *)dests[i].metric);
		s = dests[i].dest;
		printf("%zd: %s:%d=%s\n", i,
				server_ip(s), server_port(s), server_instance(s));
	}

	return 0;
}
