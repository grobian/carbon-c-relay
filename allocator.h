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

#ifndef _ALLOCATOR_H
#define _ALLOCATOR_H 1

#include <stdlib.h>

typedef struct _cr_allocator allocator;

void ra_free(allocator *ra);
allocator *ra_new(void);
void *ra_malloc(allocator *ra, size_t sz);
char *ra_strdup(allocator *ra, const char *s);

#endif

