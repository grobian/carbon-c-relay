/*
 * Copyright 2013-2014 Fabian Groffen
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

#include "server.h"

int router_readconfig(const char *path, size_t queuesize, size_t batchsize);
void router_optimise(void);
void router_printconfig(FILE *f, char all);
size_t router_route(server **ret, size_t retsize, const char *metric_path, const char *metric);
void router_test(const char *metric_path);
void router_shutdown(void);

#endif
