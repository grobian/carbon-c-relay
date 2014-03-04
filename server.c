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
#include <pthread.h>
#include <errno.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <sys/resource.h>

#include "relay.h"
#include "queue.h"
#include "dispatcher.h"
#include "collector.h"

enum servertype {
	ORIGIN,
	BACKUP
};

typedef struct _server {
	const char *ip;
	unsigned short port;
	struct sockaddr_in serv_addr;
	int fd;
	queue *queue;
	enum servertype type;
	pthread_t tid;
	char failure:1;
	size_t metrics;
	size_t dropped;
	size_t ticks;
	struct _server *next;
} server;

#define BATCH_SIZE   2500
#define QUEUE_SIZE  25000

static server *servers = NULL;

/**
 * Reads from the queue and sends items to the remote server.  This
 * function is designed to be a thread.  Data sending is attempted to be
 * batched, but sent one by one to reduce loss on sending failure.
 * A connection with the server is maintained for as long as there is
 * data to be written.  As soon as there is none, the connection is
 * dropped.
 */
static void *
server_queuereader(void *d)
{
	server *self = (server *)d;
	size_t qlen;
	size_t len;
	size_t lastlen;
	ssize_t slen;
	const char *metrics[BATCH_SIZE + 1];
	const char **metric = metrics;
	struct timeval start, stop;
	struct timeval timeout;
	char nowbuf[24];

	lastlen = 0;
	*metric = NULL;
	self->metrics = 0;
	self->ticks = 0;

	timeout.tv_sec = 0;
	timeout.tv_usec = 650 * 1000;  /* less than 1s (dispatcher stall timeout) */

	while (1) {
		/* If there was a failure, or the queue is empty, we better just
		 * wait a bit, unless we are shutting down, of course.
		 * Hence, we will try to flush the queue before shutting down */
		if ((qlen = queue_len(self->queue)) == 0 || self->failure) {
			/* however, terminate directly if this isn't going to fly */
			if (!keep_running) {
				if (qlen > 0 && self->type == ORIGIN)
					fprintf(stderr, "dropping %zd metrics for %s:%u\n",
							qlen, self->ip, self->port);
				break;
			}
			/* if we're idling, close the connection, this allows us
			 * to reduce connections, while keeping the connection alive
			 * if we're writing a lot */
			if (self->fd >= 0) {
				close(self->fd);
				self->fd = -1;
			}
			usleep(250 * 1000);  /* 250ms */
			/* skip this run if pointless */
			if (qlen == 0)
				continue;
		} else if (!keep_running) {
			/* be noisy during shutdown so we can track any slowing down
			 * servers, possibly preventing us to shut down */
			if (qlen == lastlen) {
				fprintf(stderr, "server %s:%u stalled\n", self->ip, self->port);
				self->failure = 1;
				continue;
			}
			fprintf(stderr, "shutting down %s:%u: waiting for %zd metrics\n",
					self->ip, self->port, qlen);
		}
		lastlen = qlen;

		gettimeofday(&start, NULL);

		/* try to connect */
		if (self->fd < 0 &&
				((self->fd = socket(PF_INET, SOCK_STREAM, 0)) < 0 ||
				 connect(self->fd, (struct sockaddr *)&(self->serv_addr),
					 sizeof(self->serv_addr)) < 0))
		{
			if (!self->failure) {  /* avoid endless repetition of errors */
				fprintf(stderr, "[%s] failed to connect() to %s:%u: %s\n",
						fmtnow(nowbuf), self->ip, self->port, strerror(errno));
				if (errno == EISCONN || errno == EMFILE) {
					struct rlimit ofiles;
					/* rlimit can be changed for the running process (at
					 * least on Linux 2.6+) so refetch this value every
					 * time, should only occur on errors anyway */
					if (getrlimit(RLIMIT_NOFILE, &ofiles) < 0)
						ofiles.rlim_max = 0;
					if (ofiles.rlim_max != RLIM_INFINITY &&
							ofiles.rlim_max > 0)
						fprintf(stderr, "process configured maximum "
								"connections = %d, "
								"current connections: %zd, "
								"raise max open files/max descriptor limit\n",
								(int)ofiles.rlim_max,
								dispatch_get_connections());
				}
			}
			if (self->fd >= 0)
				close(self->fd);
			self->fd = -1;
			self->failure = 1;
			continue;
		}

		/* ensure we will break out of connections being stuck */
		setsockopt(self->fd, SOL_SOCKET, SO_SNDTIMEO,
				&timeout, sizeof(timeout));
#ifdef SO_NOSIGPIPE
		setsockopt(self->fd, SOL_SOCKET, SO_NOSIGPIPE, NULL, 0);
#endif

		/* send up to BATCH_SIZE */
		if (*metric == NULL) {
			len = queue_dequeue_vector(metrics, self->queue, BATCH_SIZE);
			metrics[len] = NULL;
			metric = metrics;
		}

		for (; *metric != NULL; metric++) {
			len = strlen(*metric);
			if ((slen = send(self->fd, *metric, len, 0)) != len) {
				/* not fully sent, or failure, close connection
				 * regardless so we don't get synchonisation problems,
				 * partially sent data is an error for us, since we use
				 * blocking sockets, and hence partial sent is
				 * indication of a failure */
				fprintf(stderr, "[%s] failed to send() to %s:%u: %s\n",
						fmtnow(nowbuf), self->ip, self->port,
						(slen < 0 ? strerror(errno) : "uncomplete write"));
				close(self->fd);
				self->failure = 1;
				self->fd = -1;
				/* put back stuff we couldn't process */
				for (; *metric != NULL; metric++) {
					if (!queue_putback(self->queue, *metric)) {
						fprintf(stderr, "dropping metric: %s", *metric);
						free((char *)*metric);
					}
				}
				break;
			} else if (self->failure) {
				fprintf(stderr, "[%s] server %s:%u: OK\n",
						fmtnow(nowbuf), self->ip, self->port);
				self->failure = 0;
			}
			free((char *)*metric);
			self->metrics++;
		}

		gettimeofday(&stop, NULL);
		self->ticks += timediff(start, stop);
	}

	close(self->fd);
	return NULL;
}

/**
 * Allocates a new server and starts the thread.
 */
static server *
server_new_intern(
		const char *ip,
		unsigned short port,
		queue *queue,
		size_t qsize)
{
	server *ret = servers;

	/* first try and see if this server already exists, if so, return it */
	if (ret != NULL) {
		while (1) {
			if (strcmp(ret->ip, ip) == 0 && ret->port == port)
				return ret;
			if (ret->next == NULL) {
				ret = ret->next = malloc(sizeof(server));
				break;
			}
			ret = ret->next;
		}
	} else {
		ret = servers = malloc(sizeof(server));
	}

	if (ret == NULL)
		return NULL;

	ret->next = NULL;
	ret->ip = strdup(ip);
	ret->port = port;
	ret->fd = -1;
	ret->serv_addr.sin_family = AF_INET;
	ret->serv_addr.sin_port = htons(port);
	if (inet_pton(AF_INET, ip, &(ret->serv_addr.sin_addr)) <= 0) {
		free((char *)ret->ip);
		free(ret);
		return NULL;
	}
	if (queue == NULL) {
		ret->queue = queue_new(qsize);
		if (ret->queue == NULL) {
			free((char *)ret->ip);
			free(ret);
			return NULL;
		}
		ret->type = ORIGIN;
	} else {
		ret->queue = queue;
		ret->type = BACKUP;
	}

	ret->failure = 0;
	ret->metrics = 0;
	ret->dropped = 0;
	ret->ticks = 0;

	if (pthread_create(&ret->tid, NULL, &server_queuereader, ret) != 0) {
		free((char *)ret->ip);
		free(ret);
		return NULL;
	}

	return ret;
}

/**
 * Allocate a new (outbound) server.  Effectively this means a thread
 * that reads from the queue and sends this as good as it can to the ip
 * address and port associated.
 */
server *
server_new(const char *ip, unsigned short port)
{
	return server_new_intern(ip, port, NULL, QUEUE_SIZE);
}

/**
 * Allocate a new backup server.  Effectively this means a thread that
 * reads from the same queue as the original server.  Due to
 * multi-thread handling, it is expected that in the long run, both the
 * original and backup server get roughly the same amount of data
 * (assuming both are up).  A backup server really is, just because when
 * one of them can't be sent to, everything will go to the other.  This
 * is e.g. ok when using an upstream relay pair as target.
 */
server *
server_backup(const char *ip, unsigned short port, server *original)
{
	return server_new_intern(ip, port, original->queue, QUEUE_SIZE);
}

/**
 * Allocate a new server with a queue size.  This is the same as
 * server_new() but with a configurable queue size.
 */
server *
server_new_qsize(const char *ip, unsigned short port, size_t qsize)
{
	return server_new_intern(ip, port, NULL, qsize);
}

/**
 * Thin wrapper around the associated queue with the server object.
 * Returns true if the metric could be queued for sending, or the metric
 * was dropped because the associated server is down.  Returns false
 * otherwise (when a retry seems like it could succeed shortly).
 */
inline char
server_send(server *s, const char *d, char force)
{
	if (queue_free(s->queue) == 0) {
		if (s->failure || force) {
			s->dropped++;
			/* excess event will be dropped by the enqueue below */
		} else {
			return 0;
		}
	}
	queue_enqueue(s->queue, d);

	return 1;
}

/**
 * Returns a NULL-terminated list of servers.
 */
server **
server_get_servers(void)
{
	size_t cnt = 0;
	server *walk;
	server **ret;

	for (walk = servers; walk != NULL; walk = walk->next)
		cnt++;

	if ((ret = malloc(sizeof(server *) * (cnt + 1))) == NULL)
		return NULL;
	for (cnt = 0, walk = servers; walk != NULL; walk = walk->next, cnt++)
		ret[cnt] = walk;
	ret[cnt] = NULL;

	return ret;
}

/**
 * Waits for this server to finish sending pending items from its queue.
 * This function assumes keep_running is set to 0, otherwise this
 * function will block indefenitely.
 */
void
server_shutdown(server *s)
{
	pthread_join(s->tid, NULL);
	if (s->type == ORIGIN)
		queue_destroy(s->queue);
	free((char *)s->ip);
	free(s);
}

/**
 * Returns the ip address this server points to.
 */
inline const char *
server_ip(server *s)
{
	if (s == NULL)
		return NULL;
	return s->ip;
}

/**
 * Returns the port this server connects at.
 */
inline unsigned short
server_port(server *s)
{
	if (s == NULL)
		return 0;
	return s->port;
}

/**
 * Returns whether the last action on this server caused a failure.
 */
inline char
server_failed(server *s)
{
	if (s == NULL)
		return 0;
	return s->failure;
}

/**
 * Returns the wall-clock time in milliseconds consumed sending metrics.
 */
inline size_t
server_get_ticks(server *s)
{
	if (s == NULL)
		return 0;
	return s->ticks;
}

/**
 * Returns the number of metrics sent since start.
 */
inline size_t
server_get_metrics(server *s)
{
	if (s == NULL)
		return 0;
	return s->metrics;
}

/**
 * Returns the number of metrics dropped since start.
 */
inline size_t
server_get_dropped(server *s)
{
	if (s == NULL)
		return 0;
	return s->dropped;
}

/**
 * Returns the (approximate) number of metrics waiting to be sent.
 */
inline size_t
server_get_queue_len(server *s)
{
	if (s == NULL)
		return 0;
	if (s->type == BACKUP)
		return 0;
	return queue_len(s->queue);
}
