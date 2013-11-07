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

#include <regex.h>

#include "carbon-hash.h"

enum clusttype {
	FORWARD,
	CARBON_CH
	/* room for a better/different hash definition */
};

typedef struct _server {
	const char *server;
	struct _server *next;
} server;

typedef struct _cluster {
	enum clusttype type;
	union {
		struct {
			unsigned char repl_factor;
			carbon_ring *ring;
		} carbon_ch;
		server *ips;
	} members;
} cluster;

typedef struct _route {
	regex_t *rule;  /* regex on metric, NULL means match all */
	cluster *dest;  /* where matches should go */
	char stop;      /* whether to continue matching rules after this one */
	struct _route *next;
} route;

static route *routes = NULL;

/**
 * Populates the routing tables by reading the config file.
 */
void
router_readconfig()
{
}

/**
 * Looks up the locations the given metric_path should be sent to, and
 * enqueues the metric for the matching destinations.
 */
void
router_route(const char *metric_path, const char *metric)
{
	route *w;
	
	for (w = routes; w != NULL; w = w->next) {
		if (w->rule == NULL || regexec(w->rule, metric, 0, NULL, 0) == 0) {
			/* rule matches, send to destination(s) */
			switch (w->dest->type) {
				case FORWARD: {
					/* simple case, no logic necessary */
					server *w;
					for (w = w->dest.members.ips; w != NULL; w = w->next)
						queue_enqueue(w->queue, metric);
				}	break;
				case CARBON_CH: {
					/* let the ring(bearer) decide */
					carbon_ring *dests[4];
					int i;
					if (w->dest.members.carbon_ch.repl_factor > sizeof(dests)) {
						/* need to increase size of dests, for
						 * performance and ssp don't want to malloc and
						 * can't alloca */
						fprintf(stderr, "PANIC: increase dests array size!\n");
						return;
					}
					carbon_get_nodes(
							dests,
							w->dest.members.carbon_ch.ring,
							w->dest.members.carbon_ch.repl_factor,
							metric_path);
					for (i = 0; i < w->dest.members.carbon_ch.repl_factor; i++)
						queue_enqueue(dests[i]->queue, metric);
				}	break;
			}

			/* stop processing further rules if requested */
			if (w->stop)
				break;
		}
	}
}
