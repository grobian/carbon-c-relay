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

#include "queue.h"

/**
 * Allocates a new queue structure with capacity to hold size elements.
 */
queue *
queue_new(size_t size)
{
	queue *ret = malloc(sizeof(queue));

	if (ret == NULL)
		return NULL;

	ret->queue = malloc(sizeof(void *) * size);
	ret->write = ret->read = ret->queue[0];

	if (ret->queue == NULL) {
		free(ret);
		return NULL;
	}

	ret->end = ret->queue[size];
	ret->len = 0;
	pthread_mutex_init(&ret->lock, NULL);

	return ret;
}

/**
 * Enqueues the string pointed to by p at queue q.  If the queue is
 * full, the oldest entry is dropped.  For this reason, enqueuing will
 * never fail.
 */
void
queue_enqueue(queue *q, const char *p)
{
	pthread_mutex_lock(&q->lock);
	if (q->write == q->read) {
		free((void *)(q->read));
		q->read++;
		q->len--;
		if (q->write == q->end)
			q->read = q->queue[1];
	}
	if (q->write == q->end)
		q->write = q->queue[0];
	q->write = p;
	q->write++;
	q->len++;
	pthread_mutex_unlock(&q->lock);
}

/**
 * Returns the oldest entry in the queue.  If there are no entries, NULL
 * is returned.  The caller should free the returned string.
 */
const char *
queue_dequeue(queue *q)
{
	const char *ret;
	pthread_mutex_lock(&q->lock);
	if (q->len == 0) {
		pthread_mutex_unlock(&q->lock);
		return NULL;
	}
	if (q->read == q->end)
		q->read = q->queue[0];
	ret = q->read++;
	q->len--;
	pthread_mutex_unlock(&q->lock);
	return ret;
}
