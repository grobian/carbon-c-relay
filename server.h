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

#ifndef SERVER_H
#define SERVER_H 1

typedef struct _server server;

server *server_new(const char *ip, unsigned short port);
void server_send(server *s, const char *d);
void server_shutdown(server *s);
const char *server_ip(server *s);
unsigned short server_port(server *s);
size_t server_get_ticks(server *s);
size_t server_get_metrics(server *s);
size_t server_get_dropped(server *s);

#endif
