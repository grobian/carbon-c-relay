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


#ifndef ROUTER_H
#define ROUTER_H 1

#include <stdio.h>
#include <regex.h>

#include "server.h"
#include "aggregator.h"

#define PMODE_NORM    (1 << 0)
#define PMODE_AGGR    (1 << 1)
#define PMODE_HASH    (1 << 2)
#define PMODE_STUB    (1 << 3)
#define PMODE_DEBUG   (PMODE_HASH | PMODE_STUB)

#define CONN_DESTS_SIZE    64
typedef struct {
	const char *metric;
	server *dest;
} destination;

typedef struct _router router;
typedef enum { SUB, CUM } col_mode;

#define RE_MAX_MATCHES     64

router *router_readconfig(router *orig, const char *path, size_t queuesize, size_t batchsize, int maxstalls, unsigned short iotimeout, unsigned int sockbufsize);
void router_optimise(router *r, int threshold);
char router_printdiffs(router *old, router *new, FILE *out);
void router_transplant_queues(router *new, router *old);
char router_start(router *r);
size_t router_rewrite_metric(char (*newmetric)[METRIC_BUFSIZ], char **newfirstspace, const char *metric, const char *firstspace, const char *replacement, const size_t nmatch, const regmatch_t *pmatch);
void router_printconfig(router *r, FILE *f, char mode);
char router_route(router *r, destination ret[], size_t *retcnt, size_t retsize, char *srcaddr, char *metric, char *firstspace, int dispatcher_id);
void router_test(router *r, char *metric_path);
server **router_getservers(router *r);
aggregator *router_getaggregators(router *r);
char *router_getcollectorstub(router *r);
int router_getcollectorinterval(router *r);
char *router_getcollectorprefix(router *r);
col_mode router_getcollectormode(router *r);
void router_shutdown(router *r);
void router_free(router *r);

#endif
