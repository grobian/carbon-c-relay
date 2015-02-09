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
#include <unistd.h>
#include <string.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <netinet/in.h>
#include <errno.h>

#include "relay.h"

#ifndef TMPDIR
# define TMPDIR "/tmp"
#endif

#define SOCKFILE ".s.carbon-c-relay"


/**
 * Opens up listener sockets.  Returns the socket fds in ret, and
 * updates retlen.  If opening sockets failed, -1 is returned.  The
 * caller should ensure retlen is at least 1, and ret should be an array
 * large enough to hold it.
 */
int
bindlisten(
		int ret_stream[], int *retlen_stream,
		int ret_dgram[], int *retlen_dgram,
		const char *interface, unsigned short port)
{
	int sock;
	int optval;
	struct timeval tv;
	struct addrinfo hint;
	struct addrinfo *res, *resw;
	char buf[128];
	char saddr[INET6_ADDRSTRLEN];
	int err;
	int curlen_stream = 0;
	int curlen_dgram = 0;
	int socktypes[] = {SOCK_STREAM, SOCK_DGRAM, 0};
	int *socktype = socktypes;

	tv.tv_sec = 0;
	tv.tv_usec = 500 * 1000;

	for (; *socktype != 0; socktype++) {
		memset(&hint, 0, sizeof(hint));
		hint.ai_family = PF_UNSPEC;
		hint.ai_socktype = *socktype;
		hint.ai_protocol = 0;
		hint.ai_flags = AI_ADDRCONFIG | AI_NUMERICSERV | AI_PASSIVE;
		snprintf(buf, sizeof(buf), "%u", port);

		if ((err = getaddrinfo(interface, buf, &hint, &res)) != 0) {
			logerr("getaddrinfo(%s, %s, ...) failed: %s\n",
					interface == NULL ? "NULL" : interface,
					buf, gai_strerror(err));
			return -1;
		}

		for (resw = res; resw != NULL; resw = resw->ai_next) {
			if (resw->ai_family != PF_INET && resw->ai_family != PF_INET6)
				continue;
			if (resw->ai_protocol != IPPROTO_TCP && resw->ai_protocol != IPPROTO_UDP)
				continue;
			if ((sock = socket(resw->ai_family, resw->ai_socktype, resw->ai_protocol)) < 0)
				continue;

			(void) setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
			optval = 1;  /* allow takeover */
			(void) setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(optval));
			if (resw->ai_family == PF_INET6) {
				optval = 1;
				(void) setsockopt(sock, IPPROTO_IPV6, IPV6_V6ONLY, &optval, sizeof(optval));
			}

			if (bind(sock, resw->ai_addr, resw->ai_addrlen) < 0) {
				close(sock);
				continue;
			}

			if (inet_ntop(resw->ai_family,
						&((struct sockaddr_in *)resw->ai_addr)->sin_addr,
						saddr, sizeof(saddr)) == NULL)
				snprintf(saddr, sizeof(saddr), "(unknown)");

			if (resw->ai_protocol == IPPROTO_TCP) {
				if (listen(sock, 3) < 0) {  /* backlog of 3, enough? */
					close(sock);
					continue;
				}
				if (curlen_stream < *retlen_stream) {
					logout("listening on tcp%d %s port %s\n",
							resw->ai_family == PF_INET6 ? 6 : 4, saddr, buf);
					ret_stream[curlen_stream++] = sock;
				}
			} else {
				if (curlen_dgram < *retlen_dgram) {
					logout("listening on udp%d %s port %s\n",
							resw->ai_family == PF_INET6 ? 6 : 4, saddr, buf);
					ret_dgram[curlen_dgram++] = sock;
				}
			}
		}
		freeaddrinfo(res);
	}

	if (curlen_stream + curlen_dgram == 0)
		return -1;

	/* fake loop to simplify breakout below */
	while (curlen_stream < *retlen_stream) {
		struct sockaddr_un server;

#ifndef PF_LOCAL
# define PF_LOCAL PF_UNIX
#endif
		if ((sock = socket(PF_LOCAL, SOCK_STREAM, 0)) < 0)
			break;

		snprintf(buf, sizeof(buf), "%s/%s.%u", TMPDIR, SOCKFILE, port);
		memset(&server, 0, sizeof(struct sockaddr_un));
		server.sun_family = PF_LOCAL;
		strncpy(server.sun_path, buf, sizeof(server.sun_path) - 1);

		unlink(buf);  /* avoid address already in use */
		if (bind(sock, (struct sockaddr *)&server, sizeof(struct sockaddr_un)) < 0) {
			logerr("failed to bind for %s: %s\n",
					buf, strerror(errno));
			close(sock);
			break;
		}

		if (listen(sock, 3) < 0) {  /* backlog of 3, enough? */
			close(sock);
			break;
		}

		logout("listening on UNIX socket %s\n", buf);

		ret_stream[curlen_stream++] = sock;
		break;
	}

	*retlen_stream = curlen_stream;
	*retlen_dgram = curlen_dgram;
	return 0;
}

void
destroy_usock(unsigned short port)
{
	char buf[512];

	snprintf(buf, sizeof(buf), "%s/%s.%u", TMPDIR, SOCKFILE, port);
	unlink(buf);
}
