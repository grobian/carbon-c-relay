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
#include <unistd.h>
#include <string.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <netdb.h>
#include <netinet/in.h>
#include <errno.h>

#ifndef TMPDIR
# define TMPDIR "/tmp"
#endif

#define SOCKFILE ".s.carbon-c-relay"

/**
 * Opens up listener sockets.  Returns the socket fds in ret, and
 * updates retlen.  If opening sockets failed, -1 is returned.  The
 * caller should ensure retlen is at least 1, and ret should be an array
 * large enough to old it.
 */
int
bindlisten(int ret[], int *retlen, unsigned short port)
{
	int sock;
	int optval;
	struct timeval tv;
	struct addrinfo hint;
	struct addrinfo *res, *resw;
	char buf[128];
	int err;
	int curlen = 0;

	tv.tv_sec = 0;
	tv.tv_usec = 500 * 1000;

	memset(&hint, 0, sizeof(hint));
	hint.ai_family = PF_UNSPEC;
	hint.ai_socktype = SOCK_STREAM;
	hint.ai_protocol = IPPROTO_TCP;
	hint.ai_flags = AI_NUMERICSERV | AI_PASSIVE;
	snprintf(buf, sizeof(buf), "%u", port);

	if ((err = getaddrinfo(NULL, buf, &hint, &res)) != 0)
		return -1;

	for (resw = res; resw != NULL && curlen < *retlen; resw = resw->ai_next) {
		if ((sock = socket(resw->ai_family, resw->ai_socktype, resw->ai_protocol)) < 0)
			continue;

		setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
		optval = 1;  /* allow takeover */
		setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(optval));

		if (bind(sock, resw->ai_addr, resw->ai_addrlen) < 0) {
			close(sock);
			continue;
		}

		if (listen(sock, 3) < 0) {  /* backlog of 3, enough? */
			close(sock);
			continue;
		}

		ret[curlen++] = sock;
	}
	freeaddrinfo(res);

	if (curlen == 0)
		return -1;

	/* fake loop to simplify breakout below */
	while (curlen < *retlen) {
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
			fprintf(stderr, "failed to bind for %s: %s\n",
					buf, strerror(errno));
			close(sock);
			break;
        }

		if (listen(sock, 3) < 0) {  /* backlog of 3, enough? */
			close(sock);
			break;
		}

		ret[curlen++] = sock;
		break;
	}

	*retlen = curlen;
	return 0;
}

void
destroy_usock(unsigned short port)
{
	char buf[512];

	snprintf(buf, sizeof(buf), "%s/%s.%u", TMPDIR, SOCKFILE, port);
	unlink(buf);
}
