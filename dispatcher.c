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


#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <errno.h>
#include <poll.h>
#include <pthread.h>
#include <sys/uio.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/resource.h>
#include <netinet/in.h>
#include <arpa/inet.h>

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
	char srcaddr[24];  /* string representation of source address */
	char buf[METRIC_BUFSIZ];
	int buflen;
	char needmore:1;
	char noexpire:1;
	char metric[METRIC_BUFSIZ];
	destination dests[CONN_DESTS_SIZE];
	size_t destlen;
	struct timeval lastwork;
	unsigned int maxsenddelay;
	char hadwork:1;
	char isaggr:1;
	char isudp:1;
} connection;

struct _dispatcher {
	pthread_t tid;
	enum conntype type;
	char id;
	size_t metrics;
	size_t blackholes;
	size_t ticks;
	size_t sleeps;
	size_t prevmetrics;
	size_t prevblackholes;
	size_t prevticks;
	size_t prevsleeps;
	char keep_running;  /* full byte for atomic access */
	router *rtr;
	router *pending_rtr;
	char route_refresh_pending;  /* full byte for atomic access */
	char hold:1;
	char *allowed_chars;
};

static connection **listeners = NULL;
static connection *connections = NULL;
static size_t connectionslen = 0;
pthread_rwlock_t listenerslock = PTHREAD_RWLOCK_INITIALIZER;
pthread_rwlock_t connectionslock = PTHREAD_RWLOCK_INITIALIZER;
static size_t acceptedconnections = 0;
static size_t closedconnections = 0;
static unsigned int sockbufsize = 0;


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

#define MAX_LISTENERS 32  /* hopefully enough */

/**
 * Adds an (initial) listener socket to the chain of connections.
 * Listener sockets are those which need to be accept()-ed on.
 */
int
dispatch_addlistener(int sock)
{
	connection *newconn;
	int c;

	pthread_rwlock_wrlock(&listenerslock);
	if (listeners == NULL) {
		if ((listeners = malloc(sizeof(connection *) * MAX_LISTENERS)) == NULL)
		{
			pthread_rwlock_unlock(&listenerslock);
			return 1;
		}
		for (c = 0; c < MAX_LISTENERS; c++)
			listeners[c] = NULL;
	}
	for (c = 0; c < MAX_LISTENERS; c++) {
		if (listeners[c] == NULL) {
			newconn = malloc(sizeof(connection));
			if (newconn == NULL) {
				pthread_rwlock_unlock(&listenerslock);
				return 1;
			}
			(void) fcntl(sock, F_SETFL, O_NONBLOCK);
			newconn->sock = sock;
			newconn->takenby = 0;
			newconn->buflen = 0;

			listeners[c] = newconn;
			break;
		}
	}
	if (c == MAX_LISTENERS) {
		logerr("cannot add new listener: "
				"no more free listener slots (max = %zu)\n",
				MAX_LISTENERS);
		pthread_rwlock_unlock(&listenerslock);
		return 1;
	}
	pthread_rwlock_unlock(&listenerslock);

	return 0;
}

/**
 * Remove listener from the listeners list.  Each removal will incur a
 * global lock.  Frequent usage of this function is not anticipated.
 */
void
dispatch_removelistener(int sock)
{
	int c;
	connection *conn;

	pthread_rwlock_wrlock(&listenerslock);
	/* find connection */
	for (c = 0; c < MAX_LISTENERS; c++)
		if (listeners[c] != NULL && listeners[c]->sock == sock)
			break;
	if (c == MAX_LISTENERS) {
		/* not found?!? */
		logerr("dispatch: cannot find listener!\n");
		pthread_rwlock_unlock(&listenerslock);
		return;
	}
	/* cleanup */
	conn = listeners[c];
	close(conn->sock);
	free(conn);
	listeners[c] = NULL;
	pthread_rwlock_unlock(&listenerslock);
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
	struct sockaddr_in6 saddr;
	socklen_t saddr_len = sizeof(saddr);

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
					"out of memory allocating more slots (max = %zu)\n",
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

	/* figure out who's calling */
	if (getpeername(sock, (struct sockaddr *)&saddr, &saddr_len) == 0) {
		snprintf(connections[c].srcaddr, sizeof(connections[c].srcaddr),
				"(unknown)");
		switch (saddr.sin6_family) {
			case PF_INET:
				inet_ntop(saddr.sin6_family,
						&((struct sockaddr_in *)&saddr)->sin_addr,
						connections[c].srcaddr, sizeof(connections[c].srcaddr));
				break;
			case PF_INET6:
				inet_ntop(saddr.sin6_family, &saddr.sin6_addr,
						connections[c].srcaddr, sizeof(connections[c].srcaddr));
				break;
		}
	}

	(void) fcntl(sock, F_SETFL, O_NONBLOCK);
	if (sockbufsize > 0) {
		if (setsockopt(sock, SOL_SOCKET, SO_RCVBUF,
				&sockbufsize, sizeof(sockbufsize)) != 0)
			;
	}
	connections[c].sock = sock;
	connections[c].buflen = 0;
	connections[c].needmore = 0;
	connections[c].noexpire = 0;
	connections[c].isaggr = 0;
	connections[c].isudp = 0;
	connections[c].destlen = 0;
	gettimeofday(&connections[c].lastwork, NULL);
	connections[c].hadwork = 1;  /* force first iteration before stalling */
	/* after this dispatchers will pick this connection up */
	__sync_bool_compare_and_swap(&(connections[c].takenby), -2, 0);
	__sync_add_and_fetch(&acceptedconnections, 1);

	return c;
}

/**
 * Adds a connection which we know is from an aggregator, so direct
 * pipe.  This is different from normal connections that we don't want
 * to count them, never expire them, and want to recognise them when
 * we're doing reloads.
 */
int
dispatch_addconnection_aggr(int sock)
{
	int conn = dispatch_addconnection(sock);

	if (conn == -1)
		return 1;

	connections[conn].noexpire = 1;
	connections[conn].isaggr = 1;
	acceptedconnections--;

	return 0;
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
	connections[conn].isudp = 1;
	acceptedconnections--;

	return 0;
}


inline static char
dispatch_process_dests(connection *conn, dispatcher *self, struct timeval now)
{
	int i;
	char force;

	if (conn->destlen > 0) {
		if (conn->maxsenddelay == 0)
			conn->maxsenddelay = ((rand() % 750) + 250) * 1000;
		/* force when aggr (don't stall it) or after timeout */
		force = conn->isaggr ? 1 :
			timediff(conn->lastwork, now) > conn->maxsenddelay;
		for (i = 0; i < conn->destlen; i++) {
			tracef("dispatcher %d, connfd %d, metric %s, queueing to %s:%d\n",
					self->id, conn->sock, conn->dests[i].metric,
					server_ip(conn->dests[i].dest),
					server_port(conn->dests[i].dest));
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
			conn->lastwork = now;
			conn->hadwork = 1;
		}
	}

	return 1;
}

#define IDLE_DISCONNECT_TIME  (10 * 60 * 1000 * 1000)  /* 10 minutes */
/**
 * Look at conn and see if works needs to be done.  If so, do it.  This
 * function operates on an (exclusive) lock on the connection it serves.
 * Schematically, what this function does is like this:
 *
 *   read (partial) data  <----
 *         |                   |
 *         v                   |
 *   split and clean metrics   |
 *         |                   |
 *         v                   |
 *   route metrics             | feedback loop
 *         |                   | (stall client)
 *         v                   |
 *   send 1st attempt          |
 *         \                   |
 *          v*                 | * this is optional, but if a server's
 *   retry send (<1s)  --------    queue is full, the client is stalled
 *      block reads
 */
static int
dispatch_connection(connection *conn, dispatcher *self, struct timeval start)
{
	char *p, *q, *firstspace, *lastnl;
	int len;

	/* first try to resume any work being blocked */
	if (dispatch_process_dests(conn, self, start) == 0) {
		__sync_bool_compare_and_swap(&(conn->takenby), self->id, 0);
		return 0;
	}

	/* don't poll (read) when the last time we ran nothing happened,
	 * this is to avoid excessive CPU usage, issue #126 */
	if (!conn->hadwork && timediff(conn->lastwork, start) < 100 * 1000) {
		__sync_bool_compare_and_swap(&(conn->takenby), self->id, 0);
		return 0;
	}
	conn->hadwork = 0;

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
		if (len > 0) {
			conn->buflen += len;
			tracef("dispatcher %d, connfd %d, read %d bytes from socket\n",
					self->id, conn->sock, len);
		}

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

				__sync_add_and_fetch(&(self->metrics), 1);
				*q++ = '\n';
				*q = '\0';  /* can do this because we substract one from buf */

				/* perform routing of this metric */
				tracef("dispatcher %d, connfd %d, metric %s",
						self->id, conn->sock, conn->metric);
				__sync_add_and_fetch(&(self->blackholes),
						router_route(self->rtr,
						conn->dests, &conn->destlen, CONN_DESTS_SIZE,
						conn->srcaddr,
						conn->metric, firstspace, self->id));
				tracef("dispatcher %d, connfd %d, destinations %zd\n",
						self->id, conn->sock, conn->destlen);

				/* restart building new one from the start */
				q = conn->metric;
				firstspace = NULL;

				conn->hadwork = 1;
				gettimeofday(&conn->lastwork, NULL);
				conn->maxsenddelay = 0;
				/* send the metric to where it is supposed to go */
				if (dispatch_process_dests(conn, self, conn->lastwork) == 0)
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
			} else if (
					*p != '\0' && (
						firstspace != NULL ||
						(*p >= 'a' && *p <= 'z') ||
						(*p >= 'A' && *p <= 'Z') ||
						(*p >= '0' && *p <= '9') ||
						strchr(self->allowed_chars, *p)
					)
				)
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
	if (len == -1 && (errno == EINTR ||
				errno == EAGAIN ||
				errno == EWOULDBLOCK))
	{
		/* nothing available/no work done */
		struct timeval stop;
		gettimeofday(&stop, NULL);
		if (!conn->noexpire &&
				timediff(conn->lastwork, stop) > IDLE_DISCONNECT_TIME)
		{
			/* force close connection below */
			len = 0;
		} else {
			__sync_bool_compare_and_swap(&(conn->takenby), self->id, 0);
			return 0;
		}
	}
	if (len == -1 || len == 0 || conn->isudp) {  /* error + EOF */
		/* we also disconnect the client in this case if our reading
		 * buffer is full, but we still need more (read returns 0 if the
		 * size argument is 0) -> this is good, because we can't do much
		 * with such client */

		if (conn->noexpire) {
			/* reset buffer only (UDP/aggregations) and move on */
			conn->needmore = 1;
			conn->buflen = 0;
			__sync_bool_compare_and_swap(&(conn->takenby), self->id, 0);

			return len > 0;
		} else {
			__sync_add_and_fetch(&closedconnections, 1);
			close(conn->sock);

			/* flag this connection as no longer in use */
			__sync_bool_compare_and_swap(&(conn->takenby), self->id, -1);

			return 0;
		}
	}

	/* "release" this connection again */
	__sync_bool_compare_and_swap(&(conn->takenby), self->id, 0);

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
	int c;

	if (self->type == LISTENER) {
		struct pollfd ufds[MAX_LISTENERS];
		int fds;
		while (__sync_bool_compare_and_swap(&(self->keep_running), 1, 1)) {
			pthread_rwlock_rdlock(&listenerslock);
			fds = 0;
			for (c = 0; c < MAX_LISTENERS; c++) {
				if (listeners[c] == NULL)
					continue;
				ufds[fds].fd = listeners[c]->sock;
				ufds[fds].events = POLLIN;
				fds++;
			}
			pthread_rwlock_unlock(&listenerslock);
			if (poll(ufds, fds, 1000) > 0) {
				for (c = fds - 1; c >= 0; c--) {
					if (ufds[c].revents & POLLIN) {
						int client;
						struct sockaddr addr;
						socklen_t addrlen = sizeof(addr);

						if ((client = accept(ufds[c].fd, &addr, &addrlen)) < 0)
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
		int work;
		struct timeval start, stop;

		while (__sync_bool_compare_and_swap(&(self->keep_running), 1, 1)) {
			work = 0;

			if (__sync_bool_compare_and_swap(&(self->route_refresh_pending), 1, 1)) {
				self->rtr = self->pending_rtr;
				self->pending_rtr = NULL;
				__sync_bool_compare_and_swap(&(self->route_refresh_pending), 1, 0);
				self->hold = 0;
			}

			gettimeofday(&start, NULL);
			pthread_rwlock_rdlock(&connectionslock);
			for (c = 0; c < connectionslen; c++) {
				conn = &(connections[c]);
				/* atomically try to "claim" this connection */
				if (!__sync_bool_compare_and_swap(
							&(conn->takenby), 0, self->id))
					continue;
				if (self->hold && !conn->isaggr) {
					__sync_bool_compare_and_swap(
							&(conn->takenby), self->id, 0);
					continue;
				}
				work += dispatch_connection(conn, self, start);
			}
			pthread_rwlock_unlock(&connectionslock);
			gettimeofday(&stop, NULL);
			__sync_add_and_fetch(&(self->ticks), timediff(start, stop));

			/* nothing done, avoid spinlocking */
			if (__sync_bool_compare_and_swap(&(self->keep_running), 1, 1) &&
					work == 0)
			{
				gettimeofday(&start, NULL);
				usleep((100 + (rand() % 200)) * 1000);  /* 100ms - 300ms */
				gettimeofday(&stop, NULL);
				__sync_add_and_fetch(&(self->sleeps), timediff(start, stop));
			}
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
dispatch_new(char id, enum conntype type, router *r, char *allowed_chars)
{
	dispatcher *ret = malloc(sizeof(dispatcher));

	if (ret == NULL)
		return NULL;

	ret->id = id;
	ret->type = type;
	ret->keep_running = 1;
	ret->rtr = r;
	ret->route_refresh_pending = 0;
	ret->hold = 0;
	ret->allowed_chars = allowed_chars;

	ret->metrics = 0;
	ret->blackholes = 0;
	ret->ticks = 0;
	ret->sleeps = 0;
	ret->prevmetrics = 0;
	ret->prevblackholes = 0;
	ret->prevticks = 0;
	ret->prevsleeps = 0;

	if (pthread_create(&ret->tid, NULL, dispatch_runner, ret) != 0) {
		free(ret);
		return NULL;
	}

	return ret;
}

void
dispatch_set_bufsize(unsigned int nsockbufsize)
{
	sockbufsize = nsockbufsize;
}

static char globalid = 0;

/**
 * Starts a new dispatcher specialised in handling incoming connections
 * (and putting them on the queue for handling the connections).
 */
dispatcher *
dispatch_new_listener(void)
{
	char id = globalid++;
	return dispatch_new(id, LISTENER, NULL, NULL);
}

/**
 * Starts a new dispatcher specialised in handling incoming data on
 * existing connections.
 */
dispatcher *
dispatch_new_connection(router *r, char *allowed_chars)
{
	char id = globalid++;
	return dispatch_new(id, CONNECTION, r, allowed_chars);
}

/**
 * Signals this dispatcher to stop whatever it's doing.
 */
void
dispatch_stop(dispatcher *d)
{
	__sync_bool_compare_and_swap(&(d->keep_running), 1, 0);
}

/**
 * Shuts down dispatcher d.  Returns when the dispatcher has terminated.
 */
void
dispatch_shutdown(dispatcher *d)
{
	dispatch_stop(d);
	pthread_join(d->tid, NULL);
}

/**
 * Free up resources taken by dispatcher d.  The caller should make sure
 * the dispatcher has been shut down at this point.
 */
void
dispatch_free(dispatcher *d)
{
	free(d);
}

/**
 * Requests this dispatcher to stop processing connections.  As soon as
 * schedulereload finishes reloading the routes, this dispatcher will
 * un-hold and continue processing connections.
 * Returns when the dispatcher is no longer doing work.
 */

inline void
dispatch_hold(dispatcher *d)
{
	d->hold = 1;
}

/**
 * Schedules routes r to be put in place for the current routes.  The
 * replacement is performed at the next cycle of the dispatcher.
 */
inline void
dispatch_schedulereload(dispatcher *d, router *r)
{
	d->pending_rtr = r;
	__sync_bool_compare_and_swap(&(d->route_refresh_pending), 0, 1);
}

/**
 * Returns true if the routes scheduled to be reloaded by a call to
 * dispatch_schedulereload() have been activated.
 */
inline char
dispatch_reloadcomplete(dispatcher *d)
{
	return __sync_bool_compare_and_swap(&(d->route_refresh_pending), 0, 0);
}

/**
 * Returns the wall-clock time in milliseconds consumed by this dispatcher.
 */
inline size_t
dispatch_get_ticks(dispatcher *self)
{
	return __sync_add_and_fetch(&(self->ticks), 0);
}

/**
 * Returns the wall-clock time consumed since last call to this
 * function.
 */
inline size_t
dispatch_get_ticks_sub(dispatcher *self)
{
	size_t d = dispatch_get_ticks(self) - self->prevticks;
	self->prevticks += d;
	return d;
}

/**
 * Returns the wall-clock time in milliseconds consumed while sleeping
 * by this dispatcher.
 */
inline size_t
dispatch_get_sleeps(dispatcher *self)
{
	return __sync_add_and_fetch(&(self->sleeps), 0);
}

/**
 * Returns the wall-clock time consumed while sleeping since last call
 * to this function.
 */
inline size_t
dispatch_get_sleeps_sub(dispatcher *self)
{
	size_t d = dispatch_get_sleeps(self) - self->prevsleeps;
	self->prevsleeps += d;
	return d;
}

/**
 * Returns the number of metrics dispatched since start.
 */
inline size_t
dispatch_get_metrics(dispatcher *self)
{
	return __sync_add_and_fetch(&(self->metrics), 0);
}

/**
 * Returns the number of metrics dispatched since last call to this
 * function.
 */
inline size_t
dispatch_get_metrics_sub(dispatcher *self)
{
	size_t d = dispatch_get_metrics(self) - self->prevmetrics;
	self->prevmetrics += d;
	return d;
}

/**
 * Returns the number of metrics that were explicitly or implicitly
 * blackholed since start.
 */
inline size_t
dispatch_get_blackholes(dispatcher *self)
{
	return __sync_add_and_fetch(&(self->blackholes), 0);
}

/**
 * Returns the number of metrics that were blackholed since last call to
 * this function.
 */
inline size_t
dispatch_get_blackholes_sub(dispatcher *self)
{
	size_t d = dispatch_get_blackholes(self) - self->prevblackholes;
	self->prevblackholes += d;
	return d;
}

/**
 * Returns the number of accepted connections thusfar.
 */
inline size_t
dispatch_get_accepted_connections(void)
{
	return __sync_add_and_fetch(&(acceptedconnections), 0);
}

/**
 * Returns the number of closed connections thusfar.
 */
inline size_t
dispatch_get_closed_connections(void)
{
	return __sync_add_and_fetch(&(closedconnections), 0);
}
