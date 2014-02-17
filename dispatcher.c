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
#include <fcntl.h>
#include <string.h>
#include <errno.h>
#include <pthread.h>
#include <sys/uio.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/resource.h>

#include "relay.h"
#include "router.h"
#include "server.h"
#include "collector.h"

enum conntype {
	LISTENER,
	CONNECTION
};

typedef struct _connection {
	int sock;
	char takenby;
	char buf[8096];
	int buflen;
	char metric[8096];
	server **dests;
	size_t destlen;
	size_t destsize;
} connection;

typedef struct _dispatcher {
	pthread_t tid;
	enum conntype type;
	char id;
	size_t metrics;
	size_t ticks;
	enum { RUNNING, SLEEPING } state;
} dispatcher;

static connection *listeners[32];       /* hopefully enough */
static connection *connections[32768];  /* 32K conns, enough? */

size_t dispatch_get_connections(void);

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
	fcntl(sock, F_SETFL, O_NONBLOCK);
	newconn->sock = sock;
	newconn->takenby = 0;
	newconn->buflen = 0;
	for (c = 0; c < sizeof(listeners) / sizeof(connection *); c++)
		if (__sync_bool_compare_and_swap(&(listeners[c]), NULL, newconn))
			break;
	if (c == sizeof(listeners) / sizeof(connection *)) {
		char nowbuf[24];
		free(newconn);
		fprintf(stderr, "[%s] cannot add new listener: "
				"no more free listener slots (max = %zd)\n",
				fmtnow(nowbuf), sizeof(listeners) / sizeof(connection *));
		return 1;
	}

	return 0;
}

#define DESTSZ  32

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
	fcntl(sock, F_SETFL, O_NONBLOCK);
	newconn->sock = sock;
	newconn->takenby = 0;
	newconn->buflen = 0;
	newconn->dests = (server **)malloc(sizeof(server *) * DESTSZ);
	newconn->destlen = 0;
	newconn->destsize = DESTSZ;
	for (c = 0; c < sizeof(connections) / sizeof(connection *); c++)
		if (__sync_bool_compare_and_swap(&(connections[c]), NULL, newconn))
			break;
	if (c == sizeof(connections) / sizeof(connection *)) {
		char nowbuf[24];
		free(newconn);
		fprintf(stderr, "[%s] cannot add new connection: "
				"no more free connection slots (max = %zd)\n",
				fmtnow(nowbuf), sizeof(connections) / sizeof(connection *));
		return 1;
	}

	return 0;
}

inline static char
dispatch_process_dests(connection *conn, dispatcher *self)
{
	int i;

	if (conn->destlen > 0) {
		for (i = 0; i < conn->destlen; i++) {
			if (server_send(conn->dests[i], conn->metric) == 0)
				break;
		}
		if (i != conn->destlen) {
			conn->destlen -= i;
			memmove(conn->dests, &conn->dests[i],
					(sizeof(server *) * conn->destlen));
			return 0;
		} else {
			/* finally "complete" this metric */
			conn->destlen = 0;
			self->metrics++;
		}
	}

	return 1;
}

/**
 * Look at conn and see if works needs to be done.  If so, do it.
 */
static int
dispatch_connection(connection *conn, dispatcher *self)
{
	char *p, *q, *firstspace, *lastnl;
	char metric_path[sizeof(conn->buf)];
	int len;
	struct timeval start, stop;

	gettimeofday(&start, NULL);

	/* first try to resume any work being blocked */
	if (dispatch_process_dests(conn, self) == 0) {
		conn->takenby = 0;
		return 0;
	}

	if ((len = read(conn->sock,
					conn->buf + conn->buflen, 
					sizeof(conn->buf) - conn->buflen)) > 0)
	{
		conn->buflen += len;

		/* metrics look like this: metric_path value timestamp\n
		 * due to various messups we need to sanitise the
		 * metrics_path here, to ensure we can calculate the metric
		 * name off the filesystem path (and actually retrieve it in
		 * the web interface). */
		q = conn->metric;
		firstspace = NULL;
		lastnl = NULL;
		for (p = conn->buf; p - conn->buf < conn->buflen; p++) {
			if ((*p >= 'a' && *p <= 'z') ||
					(*p >= 'A' && *p <= 'Z') ||
					(*p >= '0' && *p <= '9') ||
					*p == '-' || *p == '_' || *p == ':')
			{
				/* copy char */
				*q++ = *p;
			} else if (*p == ' ') {
				/* separator */
				if (firstspace == NULL) {
					if (q == conn->metric)
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
				if (q != conn->metric && *(q - 1) != '.')
					*q++ = *p;
			} else if (*p == '\n') {
				/* end of metric */
				lastnl = p;

				/* just a newline on it's own? some random garbage? skip */
				if (q == conn->metric || firstspace == NULL)
					continue;

				*q++ = *p;
				*q = '\0';

				/* copy metric_path alone (up to firstspace) */
				memcpy(metric_path, conn->metric,
						firstspace + 1 - conn->metric);
				*firstspace = ' ';

				/* perform routing of this metric */
				conn->destlen =
					router_route(conn->dests, &conn->destsize,
							metric_path, conn->metric);

				/* send the metric to where it is supposed to go */
				if (dispatch_process_dests(conn, self) == 0)
					break;

				/* restart building new one from the start */
				q = conn->metric;
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
	if (len < 0 && (errno == EINTR ||
				errno == EAGAIN ||
				errno == EWOULDBLOCK))
	{
		/* nothing available/no work done */
		conn->takenby = 0;
		return 0;
	} else if (len <= 0) {  /* EOF + error */
		int c;

		/* find connection */
		for (c = 0; c < sizeof(connections) / sizeof(connection *); c++)
			if (connections[c] == conn)
				break;
		if (c == sizeof(connections) / sizeof(connection *)) {
			/* not found?!? */
			fprintf(stderr, "PANIC: can't find my own connection!\n");
			return 1;
		}
		/* make this connection no longer visible */
		connections[c] = NULL;
		/* if some other thread was looking at conn, make sure it
		 * will have moved on before freeing this object */
		usleep(10 * 1000);  /* 10ms */
		close(conn->sock);
		free(conn->dests);
		free(conn);
		return 1;
	}

	/* "release" this connection again */
	conn->takenby = 0;

	return 1;
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
	self->state = SLEEPING;

	if (self->type == LISTENER) {
		fd_set fds;
		int maxfd = -1;
		struct timeval tv;
		while (keep_running) {
			FD_ZERO(&fds);
			tv.tv_sec = 0;
			tv.tv_usec = 250 * 1000;  /* 250 ms */
			for (c = 0; c < sizeof(listeners) / sizeof(connection *); c++) {
				conn = listeners[c];
				if (conn == NULL)
					break;
				FD_SET(conn->sock, &fds);
				if (conn->sock > maxfd)
					maxfd = conn->sock;
			}
			if (select(maxfd + 1, &fds, NULL, NULL, &tv) > 0) {
				for (c = 0; c < sizeof(listeners) / sizeof(connection *); c++) {
					conn = listeners[c];
					if (conn == NULL)
						break;
					if (FD_ISSET(conn->sock, &fds)) {
						int client;
						struct sockaddr addr;
						socklen_t addrlen = sizeof(addr);

						if ((client = accept(conn->sock, &addr, &addrlen)) < 0)
						{
							fprintf(stderr, "dispatch: failed to accept() "
									"new connection: %s\n", strerror(errno));
							if (errno == EMFILE) {
								struct rlimit ofiles;
								/* rlimit can be changed for the running
								 * process (at least on Linux 2.6+) so
								 * refetch this value every time, should
								 * only occur on errors anyway */
								if (getrlimit(RLIMIT_NOFILE, &ofiles) < 0)
									ofiles.rlim_max = 0;
								if (ofiles.rlim_max != RLIM_INFINITY &&
										ofiles.rlim_max > 0)
									fprintf(stderr, "process configured "
											"maximum connections = %d, "
											"current connections: %zd, "
											"raise max open files/max "
											"descriptor limit\n",
											(int)ofiles.rlim_max,
											dispatch_get_connections());
							}
							continue;
						}
						if (dispatch_addconnection(client) != 0) {
							close(client);
							continue;
						}
					}
				}
			}
		}
	} else if (self->type == CONNECTION) {
		while (keep_running) {
			work = 0;
			for (c = 0; c < sizeof(connections) / sizeof(connection *); c++) {
				conn = connections[c];
				if (conn == NULL)
					continue;
				/* atomically try to "claim" this connection */
				if (!__sync_bool_compare_and_swap(&(conn->takenby), 0, self->id))
					continue;
				self->state = RUNNING;
				work += dispatch_connection(conn, self);
			}

			self->state = SLEEPING;
			if (work == 0)  /* nothing done, avoid spinlocking */
				usleep((100 + (rand() % 200)) * 1000);  /* 100ms - 300ms */
		}
	} else {
		fprintf(stderr, "huh? unknown self type!\n");
	}

	return NULL;
}

/**
 * Starts a new dispatcher for the given type and with the given id.
 * Returns its handle.
 */
static dispatcher *
dispatch_new(char id, enum conntype type)
{
	dispatcher *ret = malloc(sizeof(dispatcher));

	if (ret == NULL)
		return NULL;

	ret->id = id;
	ret->type = type;
	if (pthread_create(&ret->tid, NULL, dispatch_runner, ret) != 0) {
		free(ret);
		return NULL;
	}

	return ret;
}

static char globalid = 0;

/**
 * Starts a new dispatcher specialised in handling incoming connections
 * (and putting them on the queue for handling the connections).
 */
dispatcher *
dispatch_new_listener(void)
{
	char id = __sync_fetch_and_add(&globalid, 1);
	return dispatch_new(id, LISTENER);
}

/**
 * Starts a new dispatcher specialised in handling incoming data on
 * existing connections.
 */
dispatcher *
dispatch_new_connection(void)
{
	char id = __sync_fetch_and_add(&globalid, 1);
	return dispatch_new(id, CONNECTION);
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

/**
 * Returns whether this dispatcher is currently running, or not.  A
 * dispatcher is running when it is actively handling a connection, and
 * all tasks related to getting the data received in the place where it
 * should be.
 */
inline char
dispatch_busy(dispatcher *self)
{
	return self->state == RUNNING;
}

/**
 * Returns approximate number of connections in use
 */
size_t
dispatch_get_connections(void)
{
	int c;
	size_t ret = 0;

	for (c = 0; c < sizeof(connections) / sizeof(connection *); c++)
		if (connections[c] != NULL)
			ret++;

	return ret;
}
