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


#ifndef SERVER_H
#define SERVER_H 1

#include <netdb.h>

#include "relay.h"

typedef struct _server server;

server *server_new(
		const char *ip,
		unsigned short port,
		serv_ctype ctype,
		struct addrinfo *saddr,
		size_t queuesize,
		size_t batchsize);
void server_add_secondaries(server *d, server **sec, size_t cnt);
void server_set_failover(server *d);
void server_set_instance(server *d, char *inst);
char server_send(server *s, const char *d, char force);
void server_stop(server *s);
void server_shutdown(server *s);
const char *server_ip(server *s);
unsigned short server_port(server *s);
char *server_instance(server *s);
inline serv_ctype server_ctype(server *s);
char server_failed(server *s);
size_t server_get_ticks(server *s);
size_t server_get_metrics(server *s);
size_t server_get_stalls(server *s);
size_t server_get_dropped(server *s);
size_t server_get_queue_len(server *s);

#endif
