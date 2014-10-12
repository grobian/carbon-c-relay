/*
 * Copyright 2013-2014 Fabian Groffen
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
#include <fcntl.h>
#include <pthread.h>
#include <errno.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <sys/resource.h>

#include "relay.h"
#include "queue.h"
#include "dispatcher.h"
#include "collector.h"


typedef struct _server {
	const char *ip;
	unsigned short port;
	struct addrinfo *saddr;
	int fd;
	queue *queue;
	size_t bsize;
	const char **batch;
	serv_ctype ctype;
	pthread_t tid;
	struct _server **secondaries;
	size_t secondariescnt;
	char failure:6;
	char running:1;
	char keep_running:1;
	size_t metrics;
	size_t dropped;
	size_t ticks;
	struct _server *next;
} server;

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
	size_t len;
	ssize_t slen;
	const char **metric = self->batch;
	struct timeval start, stop;
	struct timeval timeout;
	char nowbuf[24];
	queue *queue;
	char idle = 0;

	*metric = NULL;
	self->metrics = 0;
	self->ticks = 0;

	timeout.tv_sec = 0;

#define FAIL_WAIT_TIME   6  /* 6 * 250ms = 1.5s */
#define DISCONNECT_WAIT_TIME   12  /* 12 * 250ms = 3s */
#define LEN_CRITICAL(Q)  (queue_free(Q) < self->bsize)
	self->running = 1;
	while (1) {
		if (queue_len(self->queue) == 0) {
			/* if we're idling, close the TCP connection, this allows us
			 * to reduce connections, while keeping the connection alive
			 * if we're writing a lot */
			if (self->ctype == CON_TCP && self->fd >= 0 &&
					idle++ > DISCONNECT_WAIT_TIME)
			{
				close(self->fd);
				self->fd = -1;
			}
			if (!self->keep_running)
				break;
			/* nothing to do, so slow down for a bit */
			usleep((200 + (rand() % 100)) * 1000);  /* 200ms - 300ms */
			/* if we are in failure mode, keep checking if we can
			 * connect, this avoids unnecessary queue moves */
			if (!self->failure)
				/* it makes no sense to try and do something, so skip */
				continue;
		} else if (self->secondariescnt > 0 &&
				(self->failure >= FAIL_WAIT_TIME || LEN_CRITICAL(self->queue)))
		{
			size_t i;
			/* offload data from our queue to our secondaries
			 * when doing so, observe the following:
			 * - avoid nodes that are in failure mode
			 * - avoid nodes which queues are >= critical_len
			 * when no nodes remain given the above
			 * - send to nodes which queue size < critical_len
			 * where there are no such nodes
			 * - do nothing (we will overflow, since we can't send
			 *   anywhere) */
			*metric = NULL;
			queue = NULL;
			for (i = 0; i < self->secondariescnt; i++) {
				/* both conditions below make sure we skip ourself */
				if (self->secondaries[i]->failure)
					continue;
				queue = self->secondaries[i]->queue;
				if (LEN_CRITICAL(queue)) {
					queue = NULL;
					continue;
				}
				if (*metric == NULL) {
					/* send up to batch size of our queue to this queue */
					len = queue_dequeue_vector(
							self->batch, self->queue, self->bsize);
					self->batch[len] = NULL;
					metric = self->batch;
				}

				for (; *metric != NULL; metric++)
					if (!queue_putback(queue, *metric))
						break;
				/* try to put back stuff that didn't fit */
				for (; *metric != NULL; metric++)
					if (!queue_putback(self->queue, *metric))
						break;
			}
			for (; *metric != NULL; metric++) {
				fprintf(stderr, "dropping metric: %s", *metric);
				free((char *)*metric);
				self->dropped++;
			}
			if (queue == NULL) {
				/* we couldn't do anything, take it easy for a bit */
				if (self->failure)
					self->failure = 1;
				if (!self->keep_running)
					break;
				usleep((200 + (rand() % 100)) * 1000);  /* 200ms - 300ms */
			}
		} else if (self->failure) {
			if (!self->keep_running)
				break;
			usleep((200 + (rand() % 100)) * 1000);  /* 200ms - 300ms */
		}

		/* at this point we've got work to do, if we're instructed to
		 * shut down, however, try to get everything out of the door
		 * (until we fail, see top of this loop) */

		gettimeofday(&start, NULL);

		/* try to connect */
		if (self->fd < 0) {
			if (self->ctype == CON_PIPE) {
				int intconn[2];
				if (pipe(intconn) < 0) {
					if (!self->failure)
						fprintf(stderr, "[%s] failed to create pipe: %s\n",
								fmtnow(nowbuf), strerror(errno));
					self->failure += self->failure >= FAIL_WAIT_TIME ? 0 : 1;
					continue;
				}
				dispatch_addconnection(intconn[0]);
				self->fd = intconn[1];
			} else if (self->ctype == CON_UDP) {
				if ((self->fd = socket(self->saddr->ai_family,
								self->saddr->ai_socktype,
								self->saddr->ai_protocol)) < 0)
				{
					if (!self->failure)
						fprintf(stderr, "[%s] failed to create udp socket: "
								"%s\n", fmtnow(nowbuf), strerror(errno));
					self->failure += self->failure >= FAIL_WAIT_TIME ? 0 : 1;
					continue;
				}
				if (connect(self->fd,
						self->saddr->ai_addr, self->saddr->ai_addrlen) < 0)
				{
					if (!self->failure)
						fprintf(stderr, "[%s] failed to connect udp socket: "
								"%s\n", fmtnow(nowbuf), strerror(errno));
					self->failure += self->failure >= FAIL_WAIT_TIME ? 0 : 1;
					close(self->fd);
					continue;
				}
			} else {
				int ret;
				int args;

				if ((self->fd = socket(self->saddr->ai_family,
								self->saddr->ai_socktype,
								self->saddr->ai_protocol)) < 0)
				{
					if (!self->failure)
						fprintf(stderr, "[%s] failed to create socket: %s\n",
								fmtnow(nowbuf), strerror(errno));
					self->failure += self->failure >= FAIL_WAIT_TIME ? 0 : 1;
					continue;
				}

				/* put socket in non-blocking mode such that we can
				 * select() (time-out) on the connect() call */
				args = fcntl(self->fd, F_GETFL, NULL);
				(void) fcntl(self->fd, F_SETFL, args | O_NONBLOCK);
				ret = connect(self->fd,
						self->saddr->ai_addr, self->saddr->ai_addrlen);

				if (ret < 0 && errno == EINPROGRESS) {
					/* wait for connection to succeed if the OS thinks
					 * it can succeed */
					fd_set fds;
					FD_ZERO(&fds);
					FD_SET(self->fd, &fds);
					timeout.tv_usec = (600 + (rand() % 100)) * 1000;
					ret = select(self->fd + 1, NULL, &fds, NULL, &timeout);
					if (ret == 0) {
						/* time limit expired */
						if (!self->failure)
							fprintf(stderr, "[%s] failed to connect() to "
									"%s:%u: Operation timed out\n",
									fmtnow(nowbuf), self->ip, self->port);
						close(self->fd);
						self->fd = -1;
						self->failure += self->failure >= FAIL_WAIT_TIME ? 0 : 1;
						continue;
					} else if (ret < 0) {
						/* some error occurred */
						if (!self->failure)
							fprintf(stderr, "[%s] failed to connect() to "
									"%s:%u: %s\n",
									fmtnow(nowbuf), self->ip, self->port,
									strerror(errno));
						close(self->fd);
						self->fd = -1;
						self->failure += self->failure >= FAIL_WAIT_TIME ? 0 : 1;
						continue;
					} else {
						int serr = 0;
						socklen_t serrlen = sizeof(serr);
						if (getsockopt(self->fd, SOL_SOCKET, SO_ERROR,
									(void *)(&serr), &serrlen) < 0)
							serr = errno;
						if (serr != 0) {
							if (!self->failure)
								fprintf(stderr, "[%s] failed to connect() to "
										"%s:%u: %s\n",
										fmtnow(nowbuf), self->ip, self->port,
										strerror(serr));
							close(self->fd);
							self->fd = -1;
							self->failure += self->failure >= FAIL_WAIT_TIME ? 0 : 1;
							continue;
						}
					}
				} else if (ret < 0) {
					if (!self->failure) {
						fprintf(stderr, "[%s] failed to connect() to "
								"%s:%u: %s\n",
								fmtnow(nowbuf), self->ip, self->port,
								strerror(errno));
						dispatch_check_rlimit_and_warn();
					}
					close(self->fd);
					self->fd = -1;
					self->failure += self->failure >= FAIL_WAIT_TIME ? 0 : 1;
					continue;
				}

				/* make socket blocking again */
				(void) fcntl(self->fd, F_SETFL, args);
			}

			/* ensure we will break out of connections being stuck */
			timeout.tv_usec = (600 + (rand() % 100)) * 1000;
			setsockopt(self->fd, SOL_SOCKET, SO_SNDTIMEO,
					&timeout, sizeof(timeout));
#ifdef SO_NOSIGPIPE
			setsockopt(self->fd, SOL_SOCKET, SO_NOSIGPIPE, NULL, 0);
#endif
		}

		/* send up to batch size */
		len = queue_dequeue_vector(self->batch, self->queue, self->bsize);
		self->batch[len] = NULL;
		metric = self->batch;

		if (len != 0 && !self->keep_running) {
			/* be noisy during shutdown so we can track any slowing down
			 * servers, possibly preventing us to shut down */
			fprintf(stderr, "shutting down %s:%u: waiting for %zd metrics\n",
					self->ip, self->port, len + queue_len(self->queue));
		}

		if (len == 0 && self->failure) {
			/* if we don't have anything to send, we have at least a
			 * connection succeed, so assume the server is up again,
			 * this is in particular important for recovering this
			 * node by probes, to avoid starvation of this server since
			 * its queue is possibly being offloaded to secondaries */
			if (self->ctype != CON_UDP)
				fprintf(stderr, "[%s] server %s:%u: OK after probe\n",
						fmtnow(nowbuf), self->ip, self->port);
			self->failure = 0;
		}

		for (; *metric != NULL; metric++) {
			len = strlen(*metric);
			if ((slen = write(self->fd, *metric, len)) != len) {
				/* not fully sent, or failure, close connection
				 * regardless so we don't get synchonisation problems,
				 * partially sent data is an error for us, since we use
				 * blocking sockets, and hence partial sent is
				 * indication of a failure */
				if (self->ctype != CON_UDP)
					fprintf(stderr, "[%s] failed to write() to %s:%u: %s\n",
							fmtnow(nowbuf), self->ip, self->port,
							(slen < 0 ? strerror(errno) : "uncomplete write"));
				close(self->fd);
				self->fd = -1;
				self->failure += self->failure >= FAIL_WAIT_TIME ? 0 : 1;
				/* put back stuff we couldn't process */
				for (; *metric != NULL; metric++) {
					if (!queue_putback(self->queue, *metric)) {
						fprintf(stderr, "server %s:%u: dropping metric: %s",
								self->ip, self->port, *metric);
						free((char *)*metric);
						self->dropped++;
					}
				}
				break;
			} else if (self->failure) {
				if (self->ctype != CON_UDP)
					fprintf(stderr, "[%s] server %s:%u: OK\n",
							fmtnow(nowbuf), self->ip, self->port);
				self->failure = 0;
			}
			free((char *)*metric);
			self->metrics++;
		}

		gettimeofday(&stop, NULL);
		self->ticks += timediff(start, stop);

		idle = 0;
	}
	self->running = 0;

	if (self->fd >= 0)
		close(self->fd);
	return NULL;
}

/**
 * Allocate a new (outbound) server.  Effectively this means a thread
 * that reads from the queue and sends this as good as it can to the ip
 * address and port associated.
 */
server *
server_new(
		const char *ip,
		unsigned short port,
		serv_ctype ctype,
		size_t qsize,
		size_t bsize)
{
	server *ret;

	if ((ret = malloc(sizeof(server))) == NULL)
		return NULL;

	ret->ctype = ctype;
	ret->tid = 0;
	ret->next = NULL;
	ret->secondaries = NULL;
	ret->secondariescnt = 0;
	ret->ip = strdup(ip);
	ret->port = port;
	ret->bsize = bsize;
	if ((ret->batch = malloc(sizeof(char *) * (bsize + 1))) == NULL) {
		free(ret);
		return NULL;
	}
	ret->fd = -1;
	if (ctype == CON_PIPE) {
		ret->saddr = NULL;
	} else {
		struct addrinfo hint;
		char sport[8];
		int err;

		memset(&hint, 0, sizeof(hint));

		hint.ai_family = PF_UNSPEC;
		hint.ai_socktype = ctype == CON_UDP ? SOCK_DGRAM : SOCK_STREAM;
		hint.ai_protocol = ctype == CON_UDP ? IPPROTO_UDP : IPPROTO_TCP;
		hint.ai_flags = AI_NUMERICSERV;
		snprintf(sport, sizeof(sport), "%u", port);

		if ((err = getaddrinfo(ip, sport, &hint, &(ret->saddr))) != 0) {
			fprintf(stderr, "%s:%s: invalid host: %s\n",
					ip, sport, gai_strerror(err));
			free((char *)ret->ip);
			free(ret);
			return NULL;
		}
	}
	ret->queue = queue_new(qsize);
	if (ret->queue == NULL) {
		free((char *)ret->ip);
		free(ret);
		return NULL;
	}

	ret->failure = 0;
	ret->running = 0;
	ret->keep_running = 1;
	ret->metrics = 0;
	ret->dropped = 0;
	ret->ticks = 0;

	if (pthread_create(&ret->tid, NULL, &server_queuereader, ret) != 0) {
		free((char *)ret->ip);
		free(ret);
		return NULL;
	}

	ret->next = servers;
	servers = ret;
	return ret;
}

/**
 * Adds a list of secondary servers to this server.  A secondary server
 * is a server which' queue will be checked when this server has nothing
 * to do.  This is different from a backup server in that all servers
 * involved have their own queue which they are supposed to deal with.
 */
void
server_add_secondaries(server *self, server **secondaries, size_t count)
{
	self->secondaries = secondaries;
	self->secondariescnt = count;
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
		char failure = s->failure;
		if (!force && s->secondariescnt > 0) {
			size_t i;
			/* don't immediately drop if we know there are others that
			 * back us up */
			for (i = 0; i < s->secondariescnt; i++) {
				if (!s->secondaries[i]->failure) {
					failure = 0;
					break;
				}
			}
		}
		if (failure || force) {
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
 */
void
server_shutdown(server *s)
{
	int i;
	pthread_t tid;
	size_t failures;
	size_t inqueue;
	int err;
	
	if (s->tid == 0)
		return;

	tid = s->tid;
	s->tid = 0;

	if (s->secondariescnt > 0) {
		/* if we have a working connection, or we still have stuff in
		 * our queue, wait for our secondaries, as they might need us,
		 * or we need them */
		do {
			failures = 0;
			inqueue = 0;
			for (i = 0; i < s->secondariescnt; i++) {
				if (s->secondaries[i]->failure)
					failures++;
				if (s->secondaries[i]->running)
					inqueue += queue_len(s->secondaries[i]->queue);
			}
			/* loop until we all failed, or nothing is in the queues */
		} while (failures != s->secondariescnt &&
				inqueue != 0 &&
				fprintf(stderr, "any_of cluster pending %zd metrics "
					"(with %zd failed nodes)\n", inqueue, failures) >= -1 &&
				usleep((200 + (rand() % 100)) * 1000) <= 0);
		/* shut down entire cluster */
		for (i = 0; i < s->secondariescnt; i++)
			s->secondaries[i]->keep_running = 0;
		/* to pretend to be dead for above loop (just in case) */
		if (inqueue != 0)
			for (i = 0; i < s->secondariescnt; i++)
				s->secondaries[i]->failure = 1;
	}

	s->keep_running = 0;
	if ((err = pthread_join(tid, NULL)) != 0)
		fprintf(stderr, "%s:%u: failed to join server thread: %s\n",
				s->ip, s->port, strerror(err));

	if (s->ctype == CON_TCP) {
		size_t qlen = queue_len(s->queue);
		queue_destroy(s->queue);
		if (qlen > 0)
			fprintf(stderr, "dropping %zd metrics for %s:%u\n",
					qlen, s->ip, s->port);
	}
	free(s->batch);
	if (s->saddr != NULL)
		freeaddrinfo(s->saddr);
	free((char *)s->ip);
	s->ip = NULL;
}

/**
 * Waits for all servers to be shut down.  This is a small optimisation
 * over looping over server_shutdown for it tries to speed up the
 * waiting time by shutting down in parallel as many servers as
 * possible.
 */
void
server_shutdown_all(void)
{
	server *walk;

	for (walk = servers; walk != NULL; walk = walk->next)
		if (walk->secondariescnt == 0)
			walk->keep_running = 0;
	for (walk = servers; walk != NULL; walk = walk->next)
		server_shutdown(walk);
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
 * Returns the connection type of this server.
 */
inline serv_ctype
server_ctype(server *s)
{
	if (s == NULL)
		return CON_PIPE;
	return s->ctype;
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
 * Returns the wall-clock time in microseconds (us) consumed sending metrics.
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
	return queue_len(s->queue);
}
