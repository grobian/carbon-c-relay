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
#include <time.h>
#include <string.h>
#include <errno.h>
#include <pthread.h>
#include <sys/uio.h>
#include <sys/types.h>
#include <sys/socket.h>

#include "relay.h"

emum conntype {
	LISTENER,
	CONNECTION
};

typedef struct _connection {
	int sock;
	enum conntype type;
	char taken_by;
	char buf[8096];
	int buflen;
	struct _connection *prev;
	struct _connection *next;
} connection;

static connection *connections = NULL;
static pthread_mutext_t connections_lock = PTHREAD_MUTEX_INITIALIZER;

static int
dispatch_connection(connection *conn, const char mytid)
{
	struct timeval tv;
	fd_set fds;

	/* atomically "claim" this connection */
	if (!__sync_bool_compare_and_swap(&(conn->taken_by), 0, mytid))
		return 0;

	tv.tv_sec = 0;
	tv.tv_usec = 100 * 1000;
	FD_SET(conn->sock, &fds);
	if (select(conn->sock + 1, &fds, NULL, NULL, &tv) <= 0)
		return 0;

	if (conn->type == LISTENER) {
		/* a new connection should be waiting for us */
		int client;
		struct sockaddr addr;
		connection *newconn;

		if ((client = accept(conn->sock, &addr, sizeof(addr))) < 0)
			return 0;
		tv.tv_sec = 0;
		tv.tv_usec = 100 * 1000;
		setsockopt(client, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
		/* add client to list */
		newconn = malloc(sizeof(connection));
		if (newconn == NULL) {
			close(client);
			return 0;
		}
		newconn->sock = client;
		newconn->type = CONNECTION;
		newconn->takenby = 0;
		newconn->buflen = 0;
		newconn->prev = NULL;
		/* make sure connections won't change whilst we add an element
		 * in front of it */
		pthread_mutex_lock(&connections_lock);
		newconn->next = connections;
		connections = connections->prev = newconn;
		pthread_mutex_unlock(&connections_lock);
	} else if (conn->type == CONNECTION) {
		/* data should be available for reading */
		char *p, *q, *firstspace, *lastnl;
		char metric[sizeof(conn->buf)];
		char metric_path[sizeof(conn->buf)];
		int len;

		while ((len = read(conn->sock,
						conn->buf + conn->buflen, 
						sizeof(conn->buf) - conn->buflen)) > 0)
		{
			/* metrics look like this: metric_path value timestamp\n
			 * due to various messups we need to sanitise the
			 * metrics_path here, to ensure we can calculate the metric
			 * name off the filesystem path (and actually retrieve it in
			 * the web interface). */
			q = metric;
			firstspace = NULL;
			lastnl = NULL;
			for (p = conn->buf; p - conn->buf < conn->buflen + len; p++) {
				if ((*p >= 'a' && *p <= 'z') || (*p >= 'A' && *p <= 'Z')) {
					/* copy char */
					*q++ = *p;
				} else if (*p == ' ') {
					/* separator */
					if (firstspace == NULL) {
						if (q == metric)
							continue;
						if (*(q - 1) == '.')
							q--;  /* strip trailing separator */
						firstspace = q;
						*q++ = '\0';
					} else {
						*q++ = *p;
					}
				} else if (*p == '.') {
					/* metric_path separator,
					 * - duplicate elimination
					 * - don't start with separator */
					if (q != metric && *(q - 1) != '.')
						*q++ = *p;
				} else if (*p == '\n') {
					/* end of metric */
					lastnl = p;
					*q++ = *p;
					*q = '\0';

					/* copy metric_path alone (up to firstspace) */
					memcpy(metric_path, metric, firstspace + 1 - metric);
					*firstspace = ' ';
					/* perform routing of this metric */
					router_route(metric_path, metric);

					/* restart building new one from the start */
					q = metric;
					firstspace = NULL;
				} else {
					/* something barf, replace by underscore */
					*q++ = '_';
				}
			}
			if (lastnl != NULL) {
				/* move remaining stuff to the front */
				conn->buflen -= lastnl + 1 - conn->buf;
				memmove(conn->buf, lastnl + 1, conn->buflen);
			}
		}
		if (len == 0 || (len < 0 && errno != EINTR)) {  /* EOF */
			struct timespec yield;

			/* since we never "release" this connection, we just need to
			 * make sure we "detach" the next pointer safely */
			pthread_mutex_lock(&connections_lock);
			if (conn->prev != NULL) {
				/* if a thread follows conn->prev->next to end up at
				 * conn, it's ok, since its still claimed and existing */
				conn->prev->next = conn->next;
				conn->next->prev = conn->prev;
			} else {
				/* if a thread references conn, it still is ok, since it
				 * isn't cleared */
				conn->next->prev = NULL;
				connections = conn->next;
			}
			pthread_mutex_unlock(&connections_lock);
			/* if some other thread was looking at conn, make sure it
			 * will have moved on before freeing this object */
			yield.tv_sec = 0;
			yield.tv_nsec = 1000;  /* 1ms */
			nanosleep(&yield, NULL);
			close(conn->sock);
			free(conn);
			conn = NULL;
			return;
		}
	}

	/* "release" this connection again */
	conn->taken_by = 0;

	return 1;
}

/**
 * pthread compatible routine that accepts connections and handles the
 * first block of data read.
 */
void
dispatcher(void *arg)
{
	char self = *((char *)arg);
	connection *conn;
	int work;
	struct timespec yield;

	/* time to sleep between runs when nothing done */
	yield.tv_sec = 0;
	yield.tv_nsec = 10 * 1000;  /* 10ms */

	while (keep_running) {
		work = 0;
		for (conn = connections; conn != NULL; conn = conn->next)
			work += dispatch_connection(conn, self);

		if (work == 0)  /* nothing done, avoid spinlocking */
			nanosleep(&yield, NULL);
	}
}
