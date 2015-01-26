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
#include <string.h>
#include <pthread.h>

typedef struct _queue {
	const char **queue;
	size_t end;  
	size_t write;
	size_t read;
	size_t len;
	pthread_mutex_t lock;
} queue;


/**
 * Allocates a new queue structure with capacity to hold size elements.
 */
queue *
queue_new(size_t size)
{
	queue *ret = malloc(sizeof(queue));

	if (ret == NULL)
		return NULL;

	ret->queue = malloc(sizeof(char *) * size);
	if (ret->queue == NULL) {
		free(ret);
		return NULL;
	}

	memset(ret->queue, 0, sizeof(char *) * size);
	ret->end = size;
	ret->read = ret->write = 0;
	ret->len = 0;
	pthread_mutex_init(&ret->lock, NULL);

	return ret;
}

/**
 * Frees up allocated resources in use by the queue.  This doesn't take
 * into account any consumers at all.  That is, the caller needs to
 * ensure noone is using the queue any more.
 */
void
queue_destroy(queue *q)
{
	q->len = 0;
	pthread_mutex_destroy(&q->lock);
	free(q->queue);
	free(q);
}

/**
 * Enqueues the string pointed to by p at queue q.  If the queue is
 * full, the oldest entry is dropped.  For this reason, enqueuing will
 * never fail.  This function assumes the pointer p is a private copy
 * for this queue, and hence will be freed once processed.
 */
void
queue_enqueue(queue *q, const char *p)
{
	/* queue normal:
	 * |=====-----------------------------|  4
	 * ^    ^
	 * r    w
	 * queue wrap:
	 * |===---------------------------====|  6
	 *    ^                           ^
	 *    w                           r
	 * queue full
	 * |==================================| 23
	 *          ^
	 *         w+r
	 */
	pthread_mutex_lock(&q->lock);
	if (q->len == q->end) {
		if (q->read == q->end)
			q->read = 0;
		free((char *)(q->queue[q->read]));
		q->read++;
		q->len--;
	}
	if (q->write == q->end)
		q->write = 0;
	q->queue[q->write] = p;
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
		q->read = 0;
	ret = q->queue[q->read++];
	q->len--;
	pthread_mutex_unlock(&q->lock);
	return ret;
}

/**
 * Returns at most len elements from the queue.  Attempts to use a
 * single lock to read a vector of elements from the queue to minimise
 * effects of locking.  Returns the number of elements stored in ret.
 * The caller is responsible for freeing elements from ret, as well as
 * making sure it is large enough to store len elements.
 */
size_t
queue_dequeue_vector(const char **ret, queue *q, size_t len)
{
	size_t i;

	pthread_mutex_lock(&q->lock);
	if (q->len == 0) {
		pthread_mutex_unlock(&q->lock);
		return 0;
	}
	if (len > q->len)
		len = q->len;
	for (i = 0; i < len; i++) {
		if (q->read == q->end)
			q->read = 0;
		ret[i] = q->queue[q->read++];
	}
	q->len -= len;
	pthread_mutex_unlock(&q->lock);

	return len;
}

/**
 * Puts the entry p at the front of the queue, instead of the end, if
 * there is space available in the queue.  Returns 0 when no space is
 * available, non-zero otherwise.  Like queue_enqueue,
 * queue_putback assumes pointer p points to a private copy for the
 * queue.
 */
char
queue_putback(queue *q, const char *p)
{
	pthread_mutex_lock(&q->lock);
	if (q->len == q->end) {
		pthread_mutex_unlock(&q->lock);
		return 0;
	}

	if (q->read == 0)
		q->read = q->end;
	q->read--;
	q->queue[q->read] = p;
	q->len++;
	pthread_mutex_unlock(&q->lock);

	return 1;
}

/**
 * Returns the (approximate) size of entries waiting to be read in the
 * queue.  The returned value cannot be taken accurate with multiple
 * readers/writers concurrently in action.  Hence it can only be seen as
 * mere hint about the state of the queue.
 */
inline size_t
queue_len(queue *q)
{
	return q->len;
}

/**
 * Returns the (approximate) size of free entries in the queue.  The
 * same conditions as for queue_len apply.
 */
inline size_t
queue_free(queue *q)
{
	return q->end - q->len;
}
