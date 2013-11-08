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


#include <unistd.h>
#include <string.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>

/**
 * Opens up a listener socket.  Returns the socket fd or -1 on failure.
 */
int
bindlisten(unsigned short port)
{
	int sock;
	struct sockaddr_in addr;
	struct timeval tv;

	addr.sin_family = PF_INET;
	addr.sin_port = htons(port);
	addr.sin_addr.s_addr = INADDR_ANY;
	memset(addr.sin_zero, 0, 8);

	if ((sock = socket(PF_INET, SOCK_STREAM, 0)) < 0)
		return -1;

	tv.tv_sec = 0;
	tv.tv_usec = 500 * 1000;
	setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
	setsockopt(sock, SOL_SOCKET, SO_REUSEPORT, NULL, 0);  /* allow takeover */

	if (bind(sock, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
		close(sock);
		return -1;
	}

	if (listen(sock, 3) < 0) {  /* backlog of 3, enough? */
		close(sock);
		return -1;
	}

	return sock;
}
