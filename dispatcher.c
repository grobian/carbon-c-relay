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
#include "dispatcher.h"

enum conntype {
	LISTENER,
	CONNECTION
};

typedef struct _connection {
	int sock;
	char takenby;   /* -2: being setup, -1: free, 0: not taken, >0: tid */
	char buf[METRIC_BUFSIZ];
	int buflen;
	char needmore:1;
	char noexpire:1;
	char metric[METRIC_BUFSIZ];
	destination dests[CONN_DESTS_SIZE];
	size_t destlen;
	time_t wait;
} connection;

struct _dispatcher {
	pthread_t tid;
	enum conntype type;
	char id;
	size_t metrics;
	size_t ticks;
	enum { RUNNING, SLEEPING } state;
	char keep_running:1;
	route *routes;
	route *pending_routes;
	char route_refresh_pending:1;
};

static connection *listeners[32];       /* hopefully enough */
static connection *connections = NULL;
static size_t connectionslen = 0;
pthread_rwlock_t connectionslock = PTHREAD_RWLOCK_INITIALIZER;
static size_t acceptedconnections = 0;
static size_t closedconnections = 0;


/**
 * Helper function to try and be helpful to the user.  If errno
 * indicates no new fds could be made, checks what the current max open
 * files limit is, and if it's close to what we have in use now, write
 * an informative message to stderr.
 */
void
dispatch_check_rlimit_and_warn(void)
{
	if (errno == EISCONN || errno == EMFILE) {
		struct rlimit ofiles;
		/* rlimit can be changed for the running process (at least on
		 * Linux 2.6+) so refetch this value every time, should only
		 * occur on errors anyway */
		if (getrlimit(RLIMIT_NOFILE, &ofiles) < 0)
			ofiles.rlim_max = 0;
		if (ofiles.rlim_max != RLIM_INFINITY && ofiles.rlim_max > 0)
			logerr("process configured maximum connections = %d, "
					"consider raising max open files/max descriptor limit\n",
					(int)ofiles.rlim_max);
	}
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
	(void) fcntl(sock, F_SETFL, O_NONBLOCK);
	newconn->sock = sock;
	newconn->takenby = 0;
	newconn->buflen = 0;
	for (c = 0; c < sizeof(listeners) / sizeof(connection *); c++)
		if (__sync_bool_compare_and_swap(&(listeners[c]), NULL, newconn))
			break;
	if (c == sizeof(listeners) / sizeof(connection *)) {
		free(newconn);
		logerr("cannot add new listener: "
				"no more free listener slots (max = %zd)\n",
				sizeof(listeners) / sizeof(connection *));
		return 1;
	}

	return 0;
}

void
dispatch_removelistener(int sock)
{
	int c;
	connection *conn;

	/* find connection */
	for (c = 0; c < sizeof(listeners) / sizeof(connection *); c++)
		if (listeners[c] != NULL && listeners[c]->sock == sock)
			break;
	if (c == sizeof(listeners) / sizeof(connection *)) {
		/* not found?!? */
		logerr("dispatch: cannot find listener!\n");
		return;
	}
	/* make this connection no longer visible */
	conn = listeners[c];
	listeners[c] = NULL;
	/* if some other thread was looking at conn, make sure it
	 * will have moved on before freeing this object */
	usleep(10 * 1000);  /* 10ms */
	close(conn->sock);
	free(conn);
}

#define CONNGROWSZ  1024

/**
 * Adds a connection socket to the chain of connections.
 * Connection sockets are those which need to be read from.
 * Returns the connection id, or -1 if a failure occurred.
 */
int
dispatch_addconnection(int sock)
{
	size_t c;

	pthread_rwlock_rdlock(&connectionslock);
	for (c = 0; c < connectionslen; c++)
		if (__sync_bool_compare_and_swap(&(connections[c].takenby), -1, -2))
			break;
	pthread_rwlock_unlock(&connectionslock);

	if (c == connectionslen) {
		connection *newlst;

		pthread_rwlock_wrlock(&connectionslock);
		if (connectionslen > c) {
			/* another dispatcher just extended the list */
			pthread_rwlock_unlock(&connectionslock);
			return dispatch_addconnection(sock);
		}
		newlst = realloc(connections,
				sizeof(connection) * (connectionslen + CONNGROWSZ));
		if (newlst == NULL) {
			logerr("cannot add new connection: "
					"out of memory allocating more slots (max = %zd)\n",
					connectionslen);

			pthread_rwlock_unlock(&connectionslock);
			return -1;
		}

		memset(&newlst[connectionslen], '\0',
				sizeof(connection) * CONNGROWSZ);
		for (c = connectionslen; c < connectionslen + CONNGROWSZ; c++)
			newlst[c].takenby = -1;  /* free */
		connections = newlst;
		c = connectionslen;  /* for the setup code below */
		newlst[c].takenby = -2;
		connectionslen += CONNGROWSZ;

		pthread_rwlock_unlock(&connectionslock);
	}

	(void) fcntl(sock, F_SETFL, O_NONBLOCK);
	connections[c].sock = sock;
	connections[c].buflen = 0;
	connections[c].needmore = 0;
	connections[c].noexpire = 0;
	connections[c].destlen = 0;
	connections[c].wait = 0;
	connections[c].takenby = 0;  /* now dispatchers will pick this one up */
	acceptedconnections++;

	return c;
}

/**
 * Adds a pseudo-listener for datagram (UDP) sockets, which is pseudo,
 * for in fact it adds a new connection, but makes sure that connection
 * won't be closed after being idle, and won't count that connection as
 * an incoming connection either.
 */
int
dispatch_addlistener_udp(int sock)
{
	int conn = dispatch_addconnection(sock);
	
	if (conn == -1)
		return 1;

	connections[conn].noexpire = 1;
	acceptedconnections--;

	return 0;
}


inline static char
dispatch_process_dests(connection *conn, dispatcher *self)
{
	int i;
	char force = time(NULL) - conn->wait > 1;  /* 1 sec timeout */

	if (conn->destlen > 0) {
		for (i = 0; i < conn->destlen; i++) {
			if (server_send(conn->dests[i].dest, conn->dests[i].metric, force) == 0)
				break;
		}
		if (i != conn->destlen) {
			conn->destlen -= i;
			memmove(&conn->dests[0], &conn->dests[i],
					(sizeof(destination) * conn->destlen));
			return 0;
		} else {
			/* finally "complete" this metric */
			conn->destlen = 0;
			conn->wait = 0;
			self->metrics++;
		}
	}

	return 1;
}

#define IDLE_DISCONNECT_TIME  (10 * 60)  /* 10 minutes */
/**
 * Look at conn and see if works needs to be done.  If so, do it.
 */
static int
dispatch_connection(connection *conn, dispatcher *self)
{
	char *p, *q, *firstspace, *lastnl;
	int len;
	struct timeval start, stop;

	gettimeofday(&start, NULL);
	/* first try to resume any work being blocked */
	if (dispatch_process_dests(conn, self) == 0) {
		gettimeofday(&stop, NULL);
		self->ticks += timediff(start, stop);
		conn->takenby = 0;
		return 0;
	}
	gettimeofday(&stop, NULL);
	self->ticks += timediff(start, stop);

	gettimeofday(&start, NULL);
	len = -2;
	/* try to read more data, if that succeeds, or we still have data
	 * left in the buffer, try to process the buffer */
	if (
			(!conn->needmore && conn->buflen > 0) || 
			(len = read(conn->sock,
						conn->buf + conn->buflen, 
						(sizeof(conn->buf) - 1) - conn->buflen)) > 0
	   )
	{
		if (len > 0)
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
			if (*p == '\n' || *p == '\r') {
				/* end of metric */
				lastnl = p;

				/* just a newline on it's own? some random garbage? skip */
				if (q == conn->metric || firstspace == NULL) {
					q = conn->metric;
					firstspace = NULL;
					continue;
				}

				*q++ = '\n';
				*q = '\0';  /* can do this because we substract one from buf */

				/* perform routing of this metric */
				conn->destlen =
					router_route(conn->dests, sizeof(conn->dests),
							conn->metric, firstspace, self->routes);

				/* restart building new one from the start */
				q = conn->metric;
				firstspace = NULL;

				conn->wait = time(NULL);
				/* send the metric to where it is supposed to go */
				if (dispatch_process_dests(conn, self) == 0)
					break;
			} else if (*p == ' ' || *p == '\t' || *p == '.') {
				/* separator */
				if (q == conn->metric) {
					/* make sure we skip this on next iteration to
					 * avoid an infinite loop, issues #8 and #51 */
					lastnl = p;
					continue;
				}
				if (*p == '\t')
					*p = ' ';
				if (*p == ' ' && firstspace == NULL) {
					if (*(q - 1) == '.')
						q--;  /* strip trailing separator */
					firstspace = q;
					*q++ = ' ';
				} else {
					/* metric_path separator or space,
					 * - duplicate elimination
					 * - don't start with separator/space */
					if (*(q - 1) != *p && (q - 1) != firstspace)
						*q++ = *p;
				}
			} else if (firstspace != NULL ||
					(*p >= 'a' && *p <= 'z') ||
					(*p >= 'A' && *p <= 'Z') ||
					(*p >= '0' && *p <= '9') ||
					*p == '-' || *p == '_' || *p == ':' || *p == '#')
			{
				/* copy char */
				*q++ = *p;
			} else {
				/* something barf, replace by underscore */
				*q++ = '_';
			}
		}
		conn->needmore = q != conn->metric;
		if (lastnl != NULL) {
			/* move remaining stuff to the front */
			conn->buflen -= lastnl + 1 - conn->buf;
			memmove(conn->buf, lastnl + 1, conn->buflen);
		}
	}
	gettimeofday(&stop, NULL);
	self->ticks += timediff(start, stop);
	if (len == -1 && (errno == EINTR ||
				errno == EAGAIN ||
				errno == EWOULDBLOCK))
	{
		/* nothing available/no work done */
		if (conn->wait == 0) {
			conn->wait = time(NULL);
			conn->takenby = 0;
			return 0;
		} else if (!conn->noexpire &&
				time(NULL) - conn->wait > IDLE_DISCONNECT_TIME)
		{
			/* force close connection below */
			len = 0;
		} else {
			conn->takenby = 0;
			return 0;
		}
	}
	if (len == -1 || len == 0) {  /* error + EOF */
		/* we also disconnect the client in this case if our reading
		 * buffer is full, but we still need more (read returns 0 if the
		 * size argument is 0) -> this is good, because we can't do much
		 * with such client */

		closedconnections++;
		close(conn->sock);

		/* flag this connection as no longer in use */
		conn->takenby = -1;

		return 0;
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
		while (self->keep_running) {
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
							logerr("dispatch: failed to "
									"accept() new connection: %s\n",
									strerror(errno));
							dispatch_check_rlimit_and_warn();
							continue;
						}
						if (dispatch_addconnection(client) == -1) {
							close(client);
							continue;
						}
					}
				}
			}
		}
	} else if (self->type == CONNECTION) {
		while (self->keep_running) {
			work = 0;
			if (self->route_refresh_pending) {
				self->routes = self->pending_routes;
				self->pending_routes = NULL;
				self->route_refresh_pending = 0;
			}

			pthread_rwlock_rdlock(&connectionslock);
			for (c = 0; c < connectionslen; c++) {
				conn = &(connections[c]);
				/* atomically try to "claim" this connection */
				if (!__sync_bool_compare_and_swap(&(conn->takenby), 0, self->id))
					continue;
				self->state = RUNNING;
				work += dispatch_connection(conn, self);
			}
			pthread_rwlock_unlock(&connectionslock);

			self->state = SLEEPING;
			/* nothing done, avoid spinlocking */
			if (self->keep_running && work == 0)
				usleep((100 + (rand() % 200)) * 1000);  /* 100ms - 300ms */
		}
	} else {
		logerr("huh? unknown self type!\n");
	}

	return NULL;
}

/**
 * Starts a new dispatcher for the given type and with the given id.
 * Returns its handle.
 */
static dispatcher *
dispatch_new(char id, enum conntype type, route *routes)
{
	dispatcher *ret = malloc(sizeof(dispatcher));

	if (ret == NULL)
		return NULL;

	ret->id = id;
	ret->type = type;
	ret->keep_running = 1;
	if (pthread_create(&ret->tid, NULL, dispatch_runner, ret) != 0) {
		free(ret);
		return NULL;
	}
	ret->routes = routes;
	ret->route_refresh_pending = 0;

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
	return dispatch_new(id, LISTENER, NULL);
}

/**
 * Starts a new dispatcher specialised in handling incoming data on
 * existing connections.
 */
dispatcher *
dispatch_new_connection(route *routes)
{
	char id = __sync_fetch_and_add(&globalid, 1);
	return dispatch_new(id, CONNECTION, routes);
}

/**
 * Signals this dispatcher to stop whatever it's doing.
 */
void
dispatch_stop(dispatcher *d)
{
	d->keep_running = 0;
}

/**
 * Shuts down and frees up dispatcher d.  Returns when the dispatcher
 * has terminated.
 */
void
dispatch_shutdown(dispatcher *d)
{
	dispatch_stop(d);
	pthread_join(d->tid, NULL);
	free(d);
}

/**
 * Schedules routes r to be put in place for the current routes.  The
 * replacement is performed at the next cycle of the dispatcher.
 */
inline void
dispatch_schedulereload(dispatcher *d, route *r)
{
	d->pending_routes = r;
	d->route_refresh_pending = 1;
}

/**
 * Returns true if the routes scheduled to be reloaded by a call to
 * dispatch_schedulereload() have been activated.
 */
inline char
dispatch_reloadcomplete(dispatcher *d)
{
	return d->route_refresh_pending == 0;
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
 * Returns the number of accepted connections thusfar.
 */
size_t
dispatch_get_accepted_connections(void)
{
	return acceptedconnections;
}

/**
 * Returns the number of closed connections thusfar.
 */
size_t
dispatch_get_closed_connections(void)
{
	return closedconnections;
}
