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

#include "relay.h"
#include "queue.h"
#include "collector.h"

typedef struct _server {
	const char *ip;
	unsigned short port;
	struct sockaddr_in serv_addr;
	queue *queue;
	pthread_t tid;
	char failure;
	size_t metrics;
	size_t dropped;
	size_t ticks;
} server;

#define BATCH_SIZE   2500
#define QUEUE_SIZE  25000

/**
 * Reads from the queue and sends items to the remote server.  This
 * function is designed to be a thread.  Data sending is attempted to be
 * batched, but sent one by one to reduce loss on sending failure.
 * Reconnecting is done for each batch to allow the remote server to
 * find a good balance between efficiency and load balancing.
 */
static void *
server_queuereader(void *d)
{
	server *self = (server *)d;
	int fd;
	size_t qlen;
	size_t i;
	size_t len;
	const char *metric;
	struct timeval start, stop;

	self->metrics = 0;
	self->ticks = 0;

	while (1) {
		if ((qlen = queue_len(self->queue)) == 0) {
			if (!keep_running)
				break;  /* terminate gracefully */
			usleep(250 * 1000);  /* 250ms */
			/* skip this run */
			continue;
		}

		/* send up to BATCH_SIZE */
		if (qlen > BATCH_SIZE)
			qlen = BATCH_SIZE;

		gettimeofday(&start, NULL);

		/* try to connect */
		if ((fd = socket(PF_INET, SOCK_STREAM, 0)) < 0 ||
			connect(fd, (struct sockaddr *)&(self->serv_addr),
				sizeof(self->serv_addr)) < 0)
		{
			self->failure = 1;
			fprintf(stderr, "failed to connect() to %s:%u: %s\n",
					self->ip, self->port, strerror(errno));
			/* sleep a little to allow the server to catchup */
			usleep(1500 * 1000);  /* 1.5s */
			continue;
		}

		for (i = 0; i < qlen; i++) {
			metric = queue_dequeue(self->queue);
			if (metric == NULL)
				break;
			len = strlen(metric);
			if (send(fd, metric, len, 0) != len) {
				/* not fully sent, or failure, close connection
				 * regardless so we don't get synchonisation problems,
				 * partially sent data is an error for us, since we use
				 * blocking sockets, and hence partial sent is
				 * indication of a failure */
				fprintf(stderr, "failed to send() to %s:%u: %s\n",
						self->ip, self->port, strerror(errno));
				fprintf(stderr, "dropping metric: %s", metric);
				free((char *)metric);
				self->failure = 1;
				break;
			}
			free((char *)metric);
		}
		close(fd);
		gettimeofday(&stop, NULL);
		self->failure = 0;
		self->metrics += i;
		self->ticks += timediff(start, stop);
	}

	return NULL;
}

/**
 * Allocate a new (outbound) server.  Effectively this means a thread
 * that reads from the queue and sends this as good as it can to the ip
 * address and port associated.
 */
server *
server_new(const char *ip, unsigned short port)
{
	server *ret = malloc(sizeof(server));

	if (ret == NULL)
		return NULL;
	ret->ip = strdup(ip);
	ret->port = port;
	ret->serv_addr.sin_family = AF_INET;
	ret->serv_addr.sin_port = htons(port);
	if (inet_pton(AF_INET, ip, &(ret->serv_addr.sin_addr)) <= 0) {
		free((char *)ret->ip);
		free(ret);
		return NULL;
	}
	ret->queue = queue_new(QUEUE_SIZE);
	if (ret->queue == NULL) {
		free((char *)ret->ip);
		free(ret);
		return NULL;
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
 * Thin wrapper around the associated queue with the server object.
 */
inline void
server_send(server *s, const char *d)
{
	if (queue_free(s->queue) == 0) {
		if (s->failure) {
			s->dropped++;
			/* event will be dropped by the enqueue below */
		} else {
			/* wait */
			do {
				usleep(250 * 1000);  /* 250ms */
			} while (queue_free(s->queue) == 0);
		}
	}
	queue_enqueue(s->queue, d);
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
	return queue_len(s->queue);
}
