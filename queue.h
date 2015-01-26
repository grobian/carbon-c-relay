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


#ifndef QUEUE_H
#define QUEUE_H 1

#include <stdlib.h>

typedef struct _queue queue;

queue* queue_new(size_t size);
void queue_destroy(queue *q);
void queue_enqueue(queue *q, const char *p);
const char *queue_dequeue(queue *q);
size_t queue_dequeue_vector(const char **ret, queue *q, size_t len);
char queue_putback(queue *q, const char *p);
size_t queue_len(queue *q);
size_t queue_free(queue *q);

#endif
