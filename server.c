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
#include <time.h>
#include <unistd.h>
#include <string.h>
#include <pthread.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>

#include "relay.h"
#include "queue.h"

typedef struct _server {
	const char *ip;
	unsigned short port;
	int sock;
	struct sockaddr_in serv_addr;
	queue *queue;
	pthread_t tid;
} server;

#define BATCH_SIZE   2500
#define QUEUE_SIZE  25000

/**
 * Reads from the queue and sends items to the remote server.  This
 * function is designed to be a thread.  Data sending is attempted to be
 * batched, but sizeable chunks with reconnects per chunk to allow the
 * remote server to find a good balance between efficiency and load
 * balancing.
 */
void *
server_queuereader(void *d)
{
	server *self = (server *)d;
	int fd;
	size_t qlen;
	size_t i;
	size_t len;
	struct timespec yield;
	const char *metric;

	while (1) {
		if ((qlen = queue_len(self->queue)) == 0) {
			if (!keep_running)
				break;  /* terminate gracefully */
			/* skip this run */
			yield.tv_sec = 0;
			yield.tv_nsec = 10 * 1000;  /* 10ms */
			nanosleep(&yield, NULL);
			continue;
		}

		/* send up to BATCH_SIZE */
		if (qlen > BATCH_SIZE)
			qlen = BATCH_SIZE;

		/* try to connect */
		fd = connect(self->sock,
				(struct sockaddr *)&(self->serv_addr),
				sizeof(self->serv_addr));
		if (fd < 0) {
			fprintf(stderr, "failed to connect() to %s:%u: %s\n",
					self->ip, self->port, strerror(errno));
			/* sleep a little to allow the server to catchup */
			yield.tv_sec = 0;
			yield.tv_nsec = 1500 * 1000;  /* 1.5s */
			nanosleep(&yield, NULL);
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
				break;
			}
			free((char *)metric);
		}
		close(fd);
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
	ret->sock = socket(PF_INET, SOCK_STREAM, 0);
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
