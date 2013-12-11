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

#ifndef DISPATCHER_H
#define DISPATCHER_H 1

#include <stdlib.h>

typedef struct _dispatcher dispatcher;

int dispatch_addlistener(int sock);
int dispatch_addconnection(int sock);
dispatcher *dispatch_new(char id);
void dispatch_shutdown(dispatcher *d);
size_t dispatch_get_ticks(dispatcher *self);
size_t dispatch_get_metrics(dispatcher *self);
size_t dispatch_get_connections(void);

#endif
