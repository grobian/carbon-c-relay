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


#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <pthread.h>
#include <sys/uio.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/socket.h>

#include "relay.h"
#include "router.h"
#include "collector.h"

enum conntype {
	LISTENER,
	CONNECTION
};

typedef struct _connection {
	int sock;
	enum conntype type;
	char takenby;
	char buf[8096];
	int buflen;
} connection;

typedef struct _dispatcher {
	pthread_t tid;
	char id;
	size_t metrics;
	size_t ticks;
} dispatcher;

static connection *connections[32768];  /* 32K conns, enough? */

/**
 * Look at conn and see if works needs to be done.  If so, do it.
 */
static int
dispatch_connection(connection *conn, dispatcher *self)
{
	struct timeval tv;
	fd_set fds;

	tv.tv_sec = 0;
	tv.tv_usec = 50 * 1000;  /* 50ms */
	FD_ZERO(&fds);
	FD_SET(conn->sock, &fds);
	if (select(conn->sock + 1, &fds, NULL, NULL, &tv) <= 0) {
		conn->takenby = 0;
		return 0;
	}

	if (conn->type == LISTENER) {
		/* a new connection should be waiting for us */
		int client;
		struct sockaddr addr;
		socklen_t addrlen = sizeof(addr);

		if ((client = accept(conn->sock, &addr, &addrlen)) < 0) {
			conn->takenby = 0;
			return 0;
		}
		tv.tv_sec = 0;
		tv.tv_usec = 100 * 1000;
		setsockopt(client, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
		if (dispatch_addconnection(client) != 0) {
			close(client);
			conn->takenby = 0;
			return 0;
		}
	} else if (conn->type == CONNECTION) {
		/* data should be available for reading */
		char *p, *q, *firstspace, *lastnl;
		char metric[sizeof(conn->buf)];
		char metric_path[sizeof(conn->buf)];
		int len;
		struct timeval start, stop;

		gettimeofday(&start, NULL);
		while ((len = read(conn->sock,
						conn->buf + conn->buflen, 
						sizeof(conn->buf) - conn->buflen)) > 0)
		{
			conn->buflen += len;

			/* metrics look like this: metric_path value timestamp\n
			 * due to various messups we need to sanitise the
			 * metrics_path here, to ensure we can calculate the metric
			 * name off the filesystem path (and actually retrieve it in
			 * the web interface). */
			q = metric;
			firstspace = NULL;
			lastnl = NULL;
			for (p = conn->buf; p - conn->buf < conn->buflen; p++) {
				if ((*p >= 'a' && *p <= 'z') ||
						(*p >= 'A' && *p <= 'Z')||
						(*p >= '0' && *p <= '9'))
				{
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

					/* just a newline on it's own? some random garbage? skip */
					if (q == metric || firstspace == NULL)
						continue;

					*q++ = *p;
					*q = '\0';

					/* copy metric_path alone (up to firstspace) */
					memcpy(metric_path, metric, firstspace + 1 - metric);
					*firstspace = ' ';
					/* perform routing of this metric */
					router_route(metric_path, metric);

					self->metrics++;

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
		gettimeofday(&stop, NULL);
		self->ticks += timediff(start, stop);
		if (len == 0 || (len < 0 && errno != EINTR)) {  /* EOF */
			int c;

			/* find connection */
			for (c = 0; c < sizeof(connections); c++)
				if (connections[c] == conn)
					break;
			if (c == sizeof(connections)) {
				/* not found?!? */
				fprintf(stderr, "PANIC: can't find my own connections!\n");
				return 1;
			}
			/* make this connection no longer visible */
			connections[c] = NULL;
			/* if some other thread was looking at conn, make sure it
			 * will have moved on before freeing this object */
			usleep(10 * 1000);  /* 10ms */
			close(conn->sock);
			free(conn);
			return 1;
		}
	}

	/* "release" this connection again */
	conn->takenby = 0;

	return 1;
}

/**
 * Adds an (initial) listener socket to the chain of connections.
 * Listener sockets are those which need to be accept()-ed on.
 */
int
dispatch_addlistener(int sock)
{
	connection *newconn;
	int c;

	newconn = malloc(sizeof(connection));
	if (newconn == NULL)
		return 1;
	newconn->sock = sock;
	newconn->type = LISTENER;
	newconn->takenby = 0;
	newconn->buflen = 0;
	for (c = 0; c < sizeof(connections); c++)
		if (__sync_bool_compare_and_swap(&(connections[c]), NULL, newconn))
			break;
	if (c == sizeof(connections)) {
		free(newconn);
		fprintf(stderr, "cannot add new listener: "
				"no more free connection slots\n");
		return 1;
	}

	return 0;
}

/**
 * Adds a connection socket to the chain of connections.
 * Connection sockets are those which need to be read from.
 */
int
dispatch_addconnection(int sock)
{
	connection *newconn;
	int c;

	newconn = malloc(sizeof(connection));
	if (newconn == NULL)
		return 1;
	newconn->sock = sock;
	newconn->type = CONNECTION;
	newconn->takenby = 0;
	newconn->buflen = 0;
	for (c = 0; c < sizeof(connections); c++)
		if (__sync_bool_compare_and_swap(&(connections[c]), NULL, newconn))
			break;
	if (c == sizeof(connections)) {
		free(newconn);
		fprintf(stderr, "cannot add new connection: "
				"no more free connection slots\n");
		return 1;
	}

	return 0;
}

/**
 * pthread compatible routine that handles connections and processes
 * whatever comes in on those.
 */
static void *
dispatch_runner(void *arg)
{
	dispatcher *self = (dispatcher *)arg;
	connection *conn;
	int work;
	int c;

	self->metrics = 0;
	self->ticks = 0;

	while (keep_running) {
		work = 0;
		for (c = 0; c < sizeof(connections) / sizeof(connection); c++) {
			conn = connections[c];
			if (conn == NULL)
				continue;
			/* atomically try to "claim" this connection */
			if (!__sync_bool_compare_and_swap(&(conn->takenby), 0, self->id))
				continue;
			work += dispatch_connection(conn, self);
		}

		if (work == 0)  /* nothing done, avoid spinlocking */
			usleep(250 * 1000);  /* 250ms */
	}

	return NULL;
}

/**
 * Starts a new dispatcher with the given id, and returns its handle.
 */
dispatcher *
dispatch_new(char id)
{
	dispatcher *ret = malloc(sizeof(dispatcher));

	if (ret == NULL)
		return NULL;

	ret->id = id;
	if (pthread_create(&ret->tid, NULL, dispatch_runner, ret) != 0) {
		free(ret);
		return NULL;
	}

	return ret;
}

/**
 * Shuts down and frees up dispatcher d.
 */
void
dispatch_shutdown(dispatcher *d)
{
	pthread_join(d->tid, NULL);
	free(d);
}

/**
 * Returns the wall-clock time in milliseconds consumed by this dispatcher.
 */
inline size_t
dispatch_get_ticks(dispatcher *self)
{
	return self->ticks;
}

/**
 * Returns the number of metrics dispatched since start.
 */
inline size_t
dispatch_get_metrics(dispatcher *self)
{
	return self->metrics;
}
