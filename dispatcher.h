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


#ifndef DISPATCHER_H
#define DISPATCHER_H 1

#include <stdlib.h>

#include "router.h"

typedef struct _dispatcher dispatcher;

void dispatch_check_rlimit_and_warn(void);
int dispatch_addlistener(int sock);
int dispatch_addlistener_udp(int sock);
void dispatch_removelistener(int sock);
int dispatch_addconnection(int sock);
dispatcher *dispatch_new_listener(void);
dispatcher *dispatch_new_connection(route *routes);
void dispatch_stop(dispatcher *d);
void dispatch_shutdown(dispatcher *d);
size_t dispatch_get_ticks(dispatcher *self);
size_t dispatch_get_metrics(dispatcher *self);
char dispatch_busy(dispatcher *self);
size_t dispatch_get_accepted_connections(void);
size_t dispatch_get_closed_connections(void);
void dispatch_schedulereload(dispatcher *d, route *r);
char dispatch_reloadcomplete(dispatcher *d);


#endif
