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


#ifndef ROUTER_H
#define ROUTER_H 1

#include <stdio.h>
#include <regex.h>

#include "server.h"

#define CONN_DESTS_SIZE    32
typedef struct {
	const char *metric;
	server *dest;
} destination;

typedef struct _cluster cluster;
typedef struct _route route;

#define RE_MAX_MATCHES     64

int router_readconfig(cluster **clret, route **rret, const char *path, size_t queuesize, size_t batchsize);
void router_optimise(route **routes);
size_t router_rewrite_metric(char (*newmetric)[METRIC_BUFSIZ], char **newfirstspace, const char *metric, const char *firstspace, const char *replacement, const size_t nmatch, const regmatch_t *pmatch);
void router_printconfig(FILE *f, char all, cluster *clusters, route *routes);
size_t router_route(destination ret[], size_t retsize, char *metric, char *firstspace, route *routes);
void router_test(char *metric_path, route *routes);
server **router_getservers(cluster *clusters);
void router_shutdown(void);
void router_free(cluster *clusters, route *r);

#endif
