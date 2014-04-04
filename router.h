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

#ifndef ROUTER_H
#define ROUTER_H 1

#include <stdio.h>

#include "server.h"

int router_readconfig(const char *path);
void router_optimise(void);
void router_printconfig(FILE *f, char all);
size_t router_route(server **ret, size_t retsize, const char *metric_path, const char *metric);
void router_test(const char *metric_path);
void router_shutdown(void);

#endif
