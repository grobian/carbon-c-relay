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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <regex.h>
#include <sys/stat.h>

#include "carbon-hash.h"

enum clusttype {
	FORWARD,
	CARBON_CH
	/* room for a better/different hash definition */
};

typedef struct _server {
	const char *server;
	unsigned short port;
	struct _server *next;
} server;

typedef struct _cluster {
	char *name;
	enum clusttype type;
	union {
		struct {
			unsigned char repl_factor;
			carbon_ring *ring;
		} carbon_ch;
		server *ips;
	} members;
	struct _cluster *next;
} cluster;

typedef struct _route {
	regex_t rule;   /* regex on metric, NULL means match all */
	cluster *dest;  /* where matches should go */
	char stop;      /* whether to continue matching rules after this one */
	struct _route *next;
} route;

static route *routes = NULL;
static cluster *clusters = NULL;

/**
 * Populates the routing tables by reading the config file.
 *
 * Config file supports the following:
 *
 * cluster (name) (forward | carbon_ch [replication (count)]) (ip:port[ ip:port ...]) ;
 * match (* | regex) send to (group) [stop] ;
 *
 * Comments start with a #-char.
 *
 * Example:
 *
 * cluster ams4
 *    carbon_ch replication 2
 *       10.0.0.1:2003
 *       10.0.0.2:2003
 *       10.0.0.3:2003
 *    ;
 * match *
 *    send to ams4
 *    stop;
 */
int
router_readconfig(const char *path)
{
	FILE *cnf;
	char *buf;
	size_t len = 0;
	char *p;
	cluster *cl;
	struct stat st;
	route *r = routes;

	if ((cnf = fopen(path, "r")) == NULL || stat(path, &st) == -1)
		return 0;
	buf = malloc(st.st_size) + 1;
	while ((len = fread(buf + len, 1, st.st_size - len, cnf)) != 0)
		;
	buf[len] = '\0';
	fclose(cnf);

	p = buf;
	do {
		for (; *p != '\0' && isspace(*p); p++)
			;
		/* skip comments */
		if (*p == '#') {
			for (; *p != '\0' && *p != '\n'; p++)
				;
			continue;
		}
		if (*p == '\0')
			break;

		if (strncmp(p, "cluster", 7) == 0 && isspace(*(p + 7))) {
			/* group config */
			server *w;
			char *name;
			p += 8;
			for (; *p != '\0' && isspace(*p); p++)
				;
			name = p;
			for (; *p != '\0' && !isspace(*p); p++)
				;
			if (*p == '\0') {
				fprintf(stderr, "unexpected end of file after 'cluster'\n");
				return 0;
			}
			*p++ = '\0';
			cl = NULL;
			if (strncmp(p, "carbon_ch", 9) == 0 && isspace(*(p + 9))) {
				int replcnt = 1;
				p += 10;
				for (; *p != '\0' && isspace(*p); p++)
					;
				if (strncmp(p, "replication", 11) == 0 && isspace(*(p + 11))) {
					char *repl;
					p += 12;
					for (; *p != '\0' && isspace(*p); p++)
						;
					repl = p;
					/* parse int */
					for (; *p != '\0' && !isspace(*p); p++)
						;
					if (*p == '\0') {
						fprintf(stderr, "unexpected end of file after "
								"'replication %s' for cluster %s\n",
								repl, name);
						return 0;
					}
					*p++ = '\0';
					if ((replcnt = atoi(repl)) == 0)
						replcnt = 1;
				}

				if ((cl = malloc(sizeof cluster)) == NULL) {
					fprintf(stderr, "malloc failed in cluster carbon_ch\n");
					return 0;
				}
				cl->type = CARBON_CH;
				cl->members.carbon_ch.repl_factor = (unsigned char)replcnt;
			} else if (strncmp(p, "forward", 7) == 0 && isspace(*(p + 7))) {
				p += 8;

				if ((cl = malloc(sizeof cluster)) == NULL) {
					fprintf(stderr, "malloc failed in cluster forward\n");
					return 0;
				}
				cl->type = FORWARD;
			} else {
				type = p;
				for (; *p != '\0' && !isspace(*p); p++)
					;
				*p = 0;
				fprintf(stderr, "unknown cluster type '%s' for cluster %s\n",
						type, name);
				return 0;
			}

			if (cl != NULL) {
				/* parse ips */
				for (; *p != '\0' && isspace(*p); p++)
					;
				w = NULL;
				do {
					char *ip = p;
					int port = 2003;
					for (; *p != '\0' && !isspace(*p) && *p != ';'; p++) {
						if (*p == ':') {
							*p = '\0';
							port = atoi(p + 1);
						}
					}
					if (*p == '\0') {
						fprintf(stderr, "unexpected end of file at '%s' "
								"for cluster %s\n", ip, name);
						free(cl);
						return 0;
					} else if (*p == ';') {
						*p = '\0';
						ip = strdup(ip);
						*p = ';';
					} else {
						*p++ = '\0';
						ip = strdup(ip);
					}

					if (cl->type == CARBON_CH) {
						cl->members.carbon_ch.ring = carbon_addnode(
								cl->members.carbon_ch.ring,
								ip, (unsigned short)port);
						if (cl->members.carbon_ch.ring == NULL) {
							fprintf(stderr, "out of memory allocating ring members\n");
							free(cl);
							return 0;
						}
					} else if (cl->type == FORWARD) {
						if (w == NULL) {
							cl->members.ips = w = malloc(sizeof(server));
						} else {
							w = w->next = malloc(sizeof(server));
						}
						if (w == NULL) {
							free(cl);
							fprintf(stderr, "malloc failed in cluster forward ip\n");
							return 0;
						}
						w->next = NULL;
						w->server = ip;
						w->port = (unsigned short)port;
					}
					for (; *p != '\0' && isspace(*p) && *p != ';'; p++)
						;
				} while (*p != ';');
				p++; /* skip over ';' */
				cl->name = strdup(name);
				cl->next = clusters;
				clusters = cl;
			}
		} else if (strncmp(p, "match", 5) == 0 && isspace(*(p + 5))) {
			/* match rule */
			char *pat;
			char *dest;
			char stop = -1;
			cluster *w;

			p += 6;
			for (; *p != '\0' && isspace(*p); p++)
				;
			pat = p;
			for (; *p != '\0' && !isspace(*p); p++)
				;
			if (*p == '\0') {
				fprintf(stderr, "unexpected end of file after 'match'\n");
				return 0;
			}
			*p++ = '\0';
			for (; *p != '\0' && isspace(*p); p++)
				;
			if (strncmp(p, "send", 4) != 0 || !isspace(*(p + 4))) {
				fprintf(stderr, "expected 'send to' after match %s\n", pat);
				return 0;
			}
			p += 5;
			for (; *p != '\0' && isspace(*p); p++)
				;
			if (strncmp(p, "to", 2) != 0 || !isspace(*(p + 2))) {
				fprintf(stderr, "expected 'send to' after match %s\n", pat);
				return 0;
			}
			p += 2;
			for (; *p != '\0' && isspace(*p); p++)
				;
			dest = p;
			for (; *p != '\0' && !isspace(*p) && *p != ';'; p++)
				;
			if (*p == '\0') {
				fprintf(stderr, "unexpected end of file after 'send to %s'\n",
						dest);
				return 0;
			} else if (*p == ';') {
				*p = '\0';
				stop = 0;
			} else {
				*p++ = '\0';
			}
			if (stop == -1) {
				/* look for either ';' or 'stop' followed by ';' */
				for (; *p != '\0' && isspace(*p); p++)
					;
				if (*p == ';') {
					stop = 0;
				} else if (strncmp(p, "stop", 4) == 0 &&
						(isspace(*(p + 4)) || *(p + 4) == ';'))
				{
					p += 4;
					if (*p != ';') {
						for (; *p != '\0' && isspace(*p); p++)
							;
						if (*p != ';') {
							fprintf(stderr, "expected ';' after stop for "
									"match %s\n", name);
							return 0;
						}
					}
					p++;
					stop = 1;
				} else {
					fprintf(stderr, "expected 'stop' and/or ';' after '%s' "
							"for %s\n", dest, name);
					return 0;
				}
			}

			/* lookup dest */
			for (w = clusters; w != NULL; w = w->next) {
				if (strcmp(w->name, dest) == 0)
					break;
			}
			if (w == NULL) {
				fprintf(stderr, "no such cluster '%s' for match\n", dest);
				return 0;
			}
			if (r == NULL) {
				routes = r = malloc(sizeof(route));
			} else {
				r = r->next = malloc(sizeof(route));
			}
			r->dest = w;
			if (strcmp(pat, "*") == 0) {
				r->rule = NULL;
			} else {
				if (regcomp(&r->rule, pat, REG_EXTENDED | REG_NOSUB) != 0) {
					fprintf(stderr, "invalid expression '%s' for match\n",
							pat);
					free(r);
					return 0;
				}
			}
			r->stop = stop;
			r->next = NULL;
		} else {
			/* garbage? */
			fprintf(stderr, "garbage in config: %s", p);
			return 0;
		}
	} while (*p != '\0');
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
