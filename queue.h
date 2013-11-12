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

#ifndef QUEUE_H
#define QUEUE_H 1

#include <stdlib.h>
#include <pthread.h>

typedef struct _queue {
	const char **queue;
	const char *end;  
	const char *write;
	const char *read;
	size_t len;
	pthread_mutex_t lock;
} queue;

queue* queue_new(size_t size);
void queue_destroy(queue *q);
void queue_enqueue(queue *q, const char *p);
const char *queue_dequeue(queue *q);
size_t queue_dequeue_vector(const char **ret, queue *q, size_t len);
size_t queue_len(queue *q);

#endif
