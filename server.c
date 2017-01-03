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


#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <fcntl.h>
#include <pthread.h>
#include <poll.h>
#include <errno.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <netdb.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>
#include <sys/resource.h>
#include <assert.h>

#include "relay.h"
#include "queue.h"
#include "dispatcher.h"
#include "collector.h"
#include "server.h"

struct _server {
	const char *ip;
	unsigned short port;
	char *instance;
	struct addrinfo *saddr;
	int fd;
	queue *queue;
	size_t bsize;
	short iotimeout;
	unsigned int sockbufsize;
	unsigned char maxstalls:SERVER_STALL_BITS;
	const char **batch;
	serv_ctype ctype;
	pthread_t tid;
	struct _server **secondaries;
	size_t secondariescnt;
	char failover:1;
	char failure;       /* full byte for atomic access */
	char running:1;
	char keep_running;  /* full byte for atomic access */
	unsigned char stallseq:SERVER_STALL_BITS;
	size_t metrics;
	size_t dropped;
	size_t stalls;
	size_t ticks;
	size_t prevmetrics;
	size_t prevdropped;
	size_t prevstalls;
	size_t prevticks;
};


/**
 * Reads from the queue and sends items to the remote server.  This
 * function is designed to be a thread.  Data sending is attempted to be
 * batched, but sent one by one to reduce loss on sending failure.
 * A connection with the server is maintained for as long as there is
 * data to be written.  As soon as there is none, the connection is
 * dropped if a timeout of DISCONNECT_WAIT_TIME exceeds.
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
	queue *squeue;
	char idle = 0;
	size_t *secpos = NULL;
	unsigned char cnt;
	const char *p;

	*metric = NULL;

#define FAIL_WAIT_TIME   6  /* 6 * 250ms = 1.5s */
#define DISCONNECT_WAIT_TIME   12  /* 12 * 250ms = 3s */
#define LEN_CRITICAL(Q)  (queue_free(Q) < self->bsize)
	self->running = 1;
	while (1) {
		if (queue_len(self->queue) == 0) {
			/* if we're idling, close the TCP connection, this allows us
			 * to reduce connections, while keeping the connection alive
			 * if we're writing a lot */
			gettimeofday(&start, NULL);
			if (self->ctype == CON_TCP && self->fd >= 0 &&
					idle++ > DISCONNECT_WAIT_TIME)
			{
				close(self->fd);
				self->fd = -1;
			}
			gettimeofday(&stop, NULL);
			__sync_add_and_fetch(&(self->ticks), timediff(start, stop));
			if (__sync_bool_compare_and_swap(&(self->keep_running), 0, 0))
				break;
			/* nothing to do, so slow down for a bit */
			usleep((200 + (rand() % 100)) * 1000);  /* 200ms - 300ms */
			/* if we are in failure mode, keep checking if we can
			 * connect, this avoids unnecessary queue moves */
			if (__sync_bool_compare_and_swap(&(self->failure), 0, 0))
				/* it makes no sense to try and do something, so skip */
				continue;
		} else if (self->secondariescnt > 0 &&
				(__sync_add_and_fetch(&(self->failure), 0) >= FAIL_WAIT_TIME ||
				 (!self->failover && LEN_CRITICAL(self->queue))))
		{
			size_t i;

			gettimeofday(&start, NULL);
			if (self->secondariescnt > 0) {
				if (secpos == NULL) {
					secpos = malloc(sizeof(size_t) * self->secondariescnt);
					if (secpos == NULL) {
						logerr("server: failed to allocate memory "
								"for secpos\n");
						gettimeofday(&stop, NULL);
						__sync_add_and_fetch(&(self->ticks),
								timediff(start, stop));
						continue;
					}
					for (i = 0; i < self->secondariescnt; i++)
						secpos[i] = i;
				}
				if (!self->failover) {
					/* randomise the failover list such that in the
					 * grand scheme of things we don't punish the first
					 * working server in the list to deal with all
					 * traffic meant for a now failing server */
					for (i = 0; i < self->secondariescnt; i++) {
						size_t n = rand() % (self->secondariescnt - i);
						if (n != i) {
							size_t t = secpos[n];
							secpos[n] = secpos[i];
							secpos[i] = t;
						}
					}
				}
			}

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
			squeue = NULL;
			for (i = 0; i < self->secondariescnt; i++) {
				/* both conditions below make sure we skip ourself */
				if (__sync_add_and_fetch(
							&(self->secondaries[secpos[i]]->failure), 0))
					continue;
				squeue = self->secondaries[secpos[i]]->queue;
				if (!self->failover && LEN_CRITICAL(squeue)) {
					squeue = NULL;
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
					if (!queue_putback(squeue, *metric))
						break;
				/* try to put back stuff that didn't fit */
				for (; *metric != NULL; metric++)
					if (!queue_putback(self->queue, *metric))
						break;
			}
			for (; *metric != NULL; metric++) {
				if (mode & MODE_DEBUG)
					logerr("dropping metric: %s", *metric);
				free((char *)*metric);
				__sync_add_and_fetch(&(self->dropped), 1);
			}
			gettimeofday(&stop, NULL);
			self->ticks += timediff(start, stop);
			if (squeue == NULL) {
				/* we couldn't do anything, take it easy for a bit */
				if (__sync_add_and_fetch(&(self->failure), 0) > 1) {
					/* This is a compound because I can't seem to figure
					 * out how to atomically just "set" a variable.
					 * It's not bad when in the middle there is a ++,
					 * all that counts is that afterwards its > 0. */
					__sync_and_and_fetch(&(self->failure), 0);
					__sync_add_and_fetch(&(self->failure), 1);
				}
				if (__sync_bool_compare_and_swap(&(self->keep_running), 0, 0))
					break;
				usleep((200 + (rand() % 100)) * 1000);  /* 200ms - 300ms */
			}
		} else if (__sync_add_and_fetch(&(self->failure), 0) > 0) {
			if (__sync_bool_compare_and_swap(&(self->keep_running), 0, 0))
				break;
			usleep((200 + (rand() % 100)) * 1000);  /* 200ms - 300ms */
			/* avoid overflowing */
			if (__sync_add_and_fetch(&(self->failure), 0) > FAIL_WAIT_TIME)
				__sync_sub_and_fetch(&(self->failure), 1);
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
					if (__sync_fetch_and_add(&(self->failure), 1) == 0)
						logerr("failed to create pipe: %s\n", strerror(errno));
					continue;
				}
				dispatch_addconnection(intconn[0]);
				self->fd = intconn[1];
			} else if (self->ctype == CON_UDP) {
				if ((self->fd = socket(self->saddr->ai_family,
								self->saddr->ai_socktype,
								self->saddr->ai_protocol)) < 0)
				{
					if (__sync_fetch_and_add(&(self->failure), 1) == 0)
						logerr("failed to create udp socket: %s\n",
								strerror(errno));
					continue;
				}
				if (connect(self->fd,
						self->saddr->ai_addr, self->saddr->ai_addrlen) < 0)
				{
					if (__sync_fetch_and_add(&(self->failure), 1) == 0)
						logerr("failed to connect udp socket: %s\n",
								strerror(errno));
					close(self->fd);
					self->fd = -1;
					continue;
				}
			} else if (self->ctype == CON_FILE) {
				if ((self->fd = open(self->ip,
								O_WRONLY | O_APPEND | O_CREAT,
								S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH)) < 0)
				{
					if (__sync_fetch_and_add(&(self->failure), 1) == 0)
						logerr("failed to open file '%s': %s\n",
								self->ip, strerror(errno));
					continue;
				}
			} else {
				int ret;
				int args;

				if ((self->fd = socket(self->saddr->ai_family,
								self->saddr->ai_socktype,
								self->saddr->ai_protocol)) < 0)
				{
					if (__sync_fetch_and_add(&(self->failure), 1) == 0)
						logerr("failed to create socket: %s\n",
								strerror(errno));
					continue;
				}

				/* put socket in non-blocking mode such that we can
				 * poll() (time-out) on the connect() call */
				args = fcntl(self->fd, F_GETFL, NULL);
				if (fcntl(self->fd, F_SETFL, args | O_NONBLOCK) < 0) {
					logerr("failed to set socket non-blocking mode: %s\n",
							strerror(errno));
					close(self->fd);
					self->fd = -1;
					continue;
				}
				ret = connect(self->fd,
						self->saddr->ai_addr, self->saddr->ai_addrlen);

				if (ret < 0 && errno == EINPROGRESS) {
					/* wait for connection to succeed if the OS thinks
					 * it can succeed */
					struct pollfd ufds[1];
					ufds[0].fd = self->fd;
					ufds[0].events = POLLIN | POLLOUT;
					ret = poll(ufds, 1, self->iotimeout + (rand() % 100));
					if (ret == 0) {
						/* time limit expired */
						if (__sync_fetch_and_add(&(self->failure), 1) == 0)
							logerr("failed to connect() to "
									"%s:%u: Operation timed out\n",
									self->ip, self->port);
						close(self->fd);
						self->fd = -1;
						continue;
					} else if (ret < 0) {
						/* some select error occurred */
						if (__sync_fetch_and_add(&(self->failure), 1) == 0)
							logerr("failed to poll() for %s:%u: %s\n",
									self->ip, self->port, strerror(errno));
						close(self->fd);
						self->fd = -1;
						continue;
					} else {
						if (ufds[0].revents & POLLHUP) {
							if (__sync_fetch_and_add(&(self->failure), 1) == 0)
								logerr("failed to connect() for %s:%u: "
										"Hangup\n", self->ip, self->port);
							close(self->fd);
							self->fd = -1;
							continue;
						}
					}
				} else if (ret < 0) {
					if (__sync_fetch_and_add(&(self->failure), 1) == 0) {
						logerr("failed to connect() to %s:%u: %s\n",
								self->ip, self->port, strerror(errno));
						dispatch_check_rlimit_and_warn();
					}
					close(self->fd);
					self->fd = -1;
					continue;
				}

				/* make socket blocking again */
				if (fcntl(self->fd, F_SETFL, args) < 0) {
					logerr("failed to remove socket non-blocking mode: %s\n",
							strerror(errno));
					close(self->fd);
					self->fd = -1;
					continue;
				}

				/* disable Nagle's algorithm, issue #208 */
				args = 1;
				if (setsockopt(self->fd, IPPROTO_TCP, TCP_NODELAY,
							&args, sizeof(args)) != 0)
					; /* ignore */
			}

			/* ensure we will break out of connections being stuck more
			 * quickly than the kernel would give up */
			timeout.tv_sec = 10;
			timeout.tv_usec = (rand() % 300) * 1000;
			if (setsockopt(self->fd, SOL_SOCKET, SO_SNDTIMEO,
					&timeout, sizeof(timeout)) != 0)
				; /* ignore */
			if (self->sockbufsize > 0)
				if (setsockopt(self->fd, SOL_SOCKET, SO_SNDBUF,
						&self->sockbufsize, sizeof(self->sockbufsize)) != 0)
					; /* ignore */
#ifdef SO_NOSIGPIPE
			if (self->ctype == CON_TCP || self->ctype == CON_UDP)
				if (setsockopt(self->fd, SOL_SOCKET, SO_NOSIGPIPE, NULL, 0) != 0)
					logout("warning: failed to ignore SIGPIPE on socket: %s\n",
							strerror(errno));
#endif
		}

		/* send up to batch size */
		len = queue_dequeue_vector(self->batch, self->queue, self->bsize);
		self->batch[len] = NULL;
		metric = self->batch;

		if (len != 0 &&
				__sync_bool_compare_and_swap(&(self->keep_running), 0, 0))
		{
			/* be noisy during shutdown so we can track any slowing down
			 * servers, possibly preventing us to shut down */
			logerr("shutting down %s:%u: waiting for %zu metrics\n",
					self->ip, self->port, len + queue_len(self->queue));
		}

		if (len == 0 && __sync_add_and_fetch(&(self->failure), 0)) {
			/* if we don't have anything to send, we have at least a
			 * connection succeed, so assume the server is up again,
			 * this is in particular important for recovering this
			 * node by probes, to avoid starvation of this server since
			 * its queue is possibly being offloaded to secondaries */
			if (self->ctype != CON_UDP)
				logerr("server %s:%u: OK after probe\n", self->ip, self->port);
			__sync_and_and_fetch(&(self->failure), 0);
		}

		for (; *metric != NULL; metric++) {
			len = strlen(*metric);
			/* Write to the stream, this may not succeed completely due
			 * to flow control and whatnot, which the docs suggest need
			 * resuming to complete.  So, use a loop, but to avoid
			 * getting endlessly stuck on this, only try a limited
			 * number of times for a single metric. */
			for (cnt = 0, p = *metric;
					cnt < 10 &&
						(slen = write(self->fd, p, len)) != len &&
						slen >= 0;
					cnt++)
			{
				p += slen;
				len -= slen;
			}
			if (slen != len) {
				/* not fully sent (after tries), or failure
				 * close connection regardless so we don't get
				 * synchonisation problems */
				if (self->ctype != CON_UDP &&
						__sync_fetch_and_add(&(self->failure), 1) == 0)
					logerr("failed to write() to %s:%u: %s\n",
							self->ip, self->port,
							(slen < 0 ? strerror(errno) : "incomplete write"));
				close(self->fd);
				self->fd = -1;
				/* put back stuff we couldn't process */
				for (; *metric != NULL; metric++) {
					if (!queue_putback(self->queue, *metric)) {
						if (mode & MODE_DEBUG)
							logerr("server %s:%u: dropping metric: %s",
									self->ip, self->port, *metric);
						free((char *)*metric);
						__sync_add_and_fetch(&(self->dropped), 1);
					}
				}
				break;
			} else if (!__sync_bool_compare_and_swap(&(self->failure), 0, 0)) {
				if (self->ctype != CON_UDP)
					logerr("server %s:%u: OK\n", self->ip, self->port);
				__sync_and_and_fetch(&(self->failure), 0);
			}
			free((char *)*metric);
			__sync_add_and_fetch(&(self->metrics), 1);
		}

		gettimeofday(&stop, NULL);
		__sync_add_and_fetch(&(self->ticks), timediff(start, stop));

		idle = 0;
	}
	self->running = 0;

	if (self->fd >= 0)
		close(self->fd);
	if (secpos != NULL)
		free(secpos);
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
		struct addrinfo *saddr,
		size_t qsize,
		size_t bsize,
		int maxstalls,
		unsigned short iotimeout,
		unsigned int sockbufsize)
{
	server *ret;

	if ((ret = malloc(sizeof(server))) == NULL)
		return NULL;

	ret->ctype = ctype;
	ret->tid = 0;
	ret->secondaries = NULL;
	ret->secondariescnt = 0;
	ret->ip = strdup(ip);
	if (ret->ip == NULL) {
		free(ret);
		return NULL;
	}
	ret->port = port;
	ret->instance = NULL;
	ret->bsize = bsize;
	ret->iotimeout = iotimeout < 250 ? 600 : iotimeout;
	ret->sockbufsize = sockbufsize;
	ret->maxstalls = maxstalls;
	if ((ret->batch = malloc(sizeof(char *) * (bsize + 1))) == NULL) {
		free((char *)ret->ip);
		free(ret);
		return NULL;
	}
	ret->fd = -1;
	ret->saddr = saddr;
	ret->queue = queue_new(qsize);
	if (ret->queue == NULL) {
		free(ret->batch);
		free((char *)ret->ip);
		free(ret);
		return NULL;
	}

	ret->failover = 0;
	ret->failure = 0;
	ret->running = 0;
	ret->keep_running = 1;
	ret->stallseq = 0;
	ret->metrics = 0;
	ret->dropped = 0;
	ret->stalls = 0;
	ret->ticks = 0;
	ret->prevmetrics = 0;
	ret->prevdropped = 0;
	ret->prevstalls = 0;
	ret->prevticks = 0;
	ret->tid = 0;

	return ret;
}

/**
 * Starts a previously created server using server_new().  Returns
 * errno if starting a thread failed, after which the caller should
 * server_free() the given s pointer.
 */
char
server_start(server *s)
{
	return pthread_create(&s->tid, NULL, &server_queuereader, s);
}

/**
 * Adds a list of secondary servers to this server.  A secondary server
 * is a server which' queue will be checked when this server has nothing
 * to do.
 */
void
server_add_secondaries(server *self, server **secondaries, size_t count)
{
	self->secondaries = secondaries;
	self->secondariescnt = count;
}

/**
 * Flags this server as part of a failover cluster, which means the
 * secondaries are used only to offload on failure, not on queue stress.
 */
void
server_set_failover(server *self)
{
	self->failover = 1;
}

/**
 * Sets instance name only used for carbon_ch cluster type.
 */
void
server_set_instance(server *self, char *instance)
{
	if (self->instance != NULL)
		free(self->instance);
	self->instance = strdup(instance);
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
		char failure = __sync_add_and_fetch(&(s->failure), 0);
		if (!force && s->secondariescnt > 0) {
			size_t i;
			/* don't immediately drop if we know there are others that
			 * back us up */
			for (i = 0; i < s->secondariescnt; i++) {
				if (!__sync_add_and_fetch(&(s->secondaries[i]->failure), 0)) {
					failure = 0;
					break;
				}
			}
		}
		if (failure || force || s->stallseq == s->maxstalls) {
			__sync_add_and_fetch(&(s->dropped), 1);
			/* excess event will be dropped by the enqueue below */
		} else {
			s->stallseq++;
			__sync_add_and_fetch(&(s->stalls), 1);
			return 0;
		}
	} else {
		s->stallseq = 0;
	}
	queue_enqueue(s->queue, d);

	return 1;
}

/**
 * Tells this server to finish sending pending items from its queue.
 */
void
server_shutdown(server *s)
{
	int i;
	size_t failures;
	size_t inqueue;

	/* this function should only be called on a running server */
	if (__sync_bool_compare_and_swap(&(s->keep_running), 0, 0))
		return;

	if (s->secondariescnt > 0) {
		/* if we have a working connection, or we still have stuff in
		 * our queue, wait for our secondaries, as they might need us,
		 * or we need them */
		do {
			failures = 0;
			inqueue = 0;
			for (i = 0; i < s->secondariescnt; i++) {
				if (__sync_add_and_fetch(&(s->secondaries[i]->failure), 0))
					failures++;
				if (s->secondaries[i]->running)
					inqueue += queue_len(s->secondaries[i]->queue);
			}
			/* loop until we all failed, or nothing is in the queues */
		} while (failures != s->secondariescnt &&
				inqueue != 0 &&
				logout("any_of cluster pending %zu metrics "
					"(with %zu failed nodes)\n", inqueue, failures) >= -1 &&
				usleep((200 + (rand() % 100)) * 1000) <= 0);
		/* shut down entire cluster */
		for (i = 0; i < s->secondariescnt; i++)
			__sync_bool_compare_and_swap(
					&(s->secondaries[i]->keep_running), 1, 0);
		/* to pretend to be dead for above loop (just in case) */
		if (inqueue != 0)
			for (i = 0; i < s->secondariescnt; i++)
				__sync_add_and_fetch(&(s->secondaries[i]->failure), 1);
	}

	__sync_bool_compare_and_swap(&(s->keep_running), 1, 0);
}

/**
 * Frees this server and associated resources.  This includes joining
 * the server thread.
 */
void
server_free(server *s) {
	int err;

	if (s->tid != 0 && (err = pthread_join(s->tid, NULL)) != 0)
		logerr("%s:%u: failed to join server thread: %s\n",
				s->ip, s->port, strerror(err));
	s->tid = 0;

	if (s->ctype == CON_TCP) {
		size_t qlen = queue_len(s->queue);
		if (qlen > 0)
			logerr("dropping %zu metrics for %s:%u\n",
					qlen, s->ip, s->port);
	}

	queue_destroy(s->queue);
	free(s->batch);
	if (s->instance)
		free(s->instance);
	if (s->saddr != NULL)
		freeaddrinfo(s->saddr);
	free((char *)s->ip);
	s->ip = NULL;
	free(s);
}

/**
 * Swaps the queue between server l to r.  This is assumes both l and r
 * are not running.
 */
void
server_swap_queue(server *l, server *r)
{
	queue *t;

	assert(l->keep_running == 0 || l->tid == 0);
	assert(r->keep_running == 0 || r->tid == 0);

	t = l->queue;
	l->queue = r->queue;
	r->queue = t;
	
	/* swap associated statistics as well */
	l->metrics = r->metrics;
	l->dropped = r->dropped;
	l->stalls = r->stalls;
	l->ticks = r->ticks;
	l->prevmetrics = r->prevmetrics;
	l->prevdropped = r->prevdropped;
	l->prevstalls = r->prevstalls;
	l->prevticks = r->prevticks;
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
 * Returns the instance associated with this server.
 */
inline char *
server_instance(server *s)
{
	return s->instance;
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
	return __sync_add_and_fetch(&(s->failure), 0);
}

/**
 * Returns the wall-clock time in microseconds (us) consumed sending metrics.
 */
inline size_t
server_get_ticks(server *s)
{
	if (s == NULL)
		return 0;
	return __sync_add_and_fetch(&(s->ticks), 0);
}

/**
 * Returns the wall-clock time in microseconds (us) consumed since last
 * call to this function.
 */
inline size_t
server_get_ticks_sub(server *s)
{
	size_t d;
	if (s == NULL)
		return 0;
	d = __sync_add_and_fetch(&(s->ticks), 0) - s->prevticks;
	s->prevticks += d;
	return d;
}

/**
 * Returns the number of metrics sent since start.
 */
inline size_t
server_get_metrics(server *s)
{
	if (s == NULL)
		return 0;
	return __sync_add_and_fetch(&(s->metrics), 0);
}

/**
 * Returns the number of metrics sent since last call to this function.
 */
inline size_t
server_get_metrics_sub(server *s)
{
	size_t d;
	if (s == NULL)
		return 0;
	d = __sync_add_and_fetch(&(s->metrics), 0) - s->prevmetrics;
	s->prevmetrics += d;
	return d;
}

/**
 * Returns the number of metrics dropped since start.
 */
inline size_t
server_get_dropped(server *s)
{
	if (s == NULL)
		return 0;
	return __sync_add_and_fetch(&(s->dropped), 0);
}

/**
 * Returns the number of metrics dropped since last call to this function.
 */
inline size_t
server_get_dropped_sub(server *s)
{
	size_t d;
	if (s == NULL)
		return 0;
	d = __sync_add_and_fetch(&(s->dropped), 0) - s->prevdropped;
	s->prevdropped += d;
	return d;
}

/**
 * Returns the number of stalls since start.  A stall happens when the
 * queue is full, but it appears as if it would be a good idea to wait
 * for a brief period and retry.
 */
inline size_t
server_get_stalls(server *s)
{
	if (s == NULL)
		return 0;
	return __sync_add_and_fetch(&(s->stalls), 0);
}

/**
 * Returns the number of stalls since last call to this function.
 */
inline size_t
server_get_stalls_sub(server *s)
{
	size_t d;
	if (s == NULL)
		return 0;
	d = __sync_add_and_fetch(&(s->stalls), 0) - s->prevstalls;
	s->prevstalls += d;
	return d;
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

/**
 * Returns the allocated size of the queue backing metrics waiting to be
 * sent.
 */
inline size_t
server_get_queue_size(server *s)
{
	if (s == NULL)
		return 0;
	return queue_size(s->queue);
}
