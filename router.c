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
#include <errno.h>

#include "carbon-hash.h"
#include "server.h"
#include "queue.h"
#include "aggregator.h"

enum clusttype {
	FORWARD,
	CARBON_CH,  /* room for a better/different hash definition */
	ANYOF,
	AGGREGATION
};

typedef struct _servers {
	server *server;
	struct _servers *next;
} servers;

typedef struct _cluster {
	char *name;
	enum clusttype type;
	union {
		struct {
			unsigned char repl_factor;
			carbon_ring *ring;
			servers *servers;
		} carbon_ch;
		servers *forward;
		aggregator *aggregation;
	} members;
	struct _cluster *next;
} cluster;

typedef struct _route {
	char *pattern;    /* original regex input, used for printing only */
	regex_t rule;     /* regex on metric, only if !matchall */
	cluster *dest;    /* where matches should go */
	char stop:1;      /* whether to continue matching rules after this one */
	char matchall:1;  /* whether we should use the regex when matching */
	struct _route *next;
} route;

static route *routes = NULL;
static cluster *clusters = NULL;

/**
 * Populates the routing tables by reading the config file.
 *
 * Config file supports the following:
 *
 * cluster (name)
 *     (forward | any_of | carbon_ch [replication (count)])
 *         (ip:port[ ip:port ...])
 *     ;
 * match (* | regex)
 *     send to (cluster)
 *     [stop]
 *     ;
 * aggregate
 *         (regex[ regex ...])
 *     every (interval) seconds
 *     expire after (expiration) seconds
 *     compute (sum | count | max | min | average) write to
 *         (metric)
 *     [compute ...]
 *     ;
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
	buf = malloc(st.st_size + 1);
	while ((len = fread(buf + len, 1, st.st_size - len, cnf)) != 0)
		;
	buf[st.st_size] = '\0';
	fclose(cnf);

	/* remove all comments to ease parsing below */
	p = buf;
	for (; *p != '\0'; p++)
		if (*p == '#')
			for (; *p != '\0' && *p != '\n'; p++)
				*p = ' ';

	p = buf;
	do {
		for (; *p != '\0' && isspace(*p); p++)
			;
		if (*p == '\0')
			break;

		if (strncmp(p, "cluster", 7) == 0 && isspace(*(p + 7))) {
			/* group config */
			servers *w;
			char *name;
			p += 8;
			for (; *p != '\0' && isspace(*p); p++)
				;
			name = p;
			for (; *p != '\0' && !isspace(*p); p++)
				;
			if (*p == '\0') {
				fprintf(stderr, "unexpected end of file after 'cluster'\n");
				free(buf);
				return 0;
			}
			*p++ = '\0';
			cl = NULL;
			for (; *p != '\0' && isspace(*p); p++)
				;
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
						free(buf);
						return 0;
					}
					*p++ = '\0';
					if ((replcnt = atoi(repl)) == 0)
						replcnt = 1;
				}

				if ((cl = malloc(sizeof(cluster))) == NULL) {
					fprintf(stderr, "malloc failed in cluster carbon_ch\n");
					free(buf);
					return 0;
				}
				cl->type = CARBON_CH;
				cl->members.carbon_ch.repl_factor = (unsigned char)replcnt;
				cl->members.carbon_ch.ring = NULL;
			} else if (strncmp(p, "forward", 7) == 0 && isspace(*(p + 7))) {
				p += 8;

				if ((cl = malloc(sizeof(cluster))) == NULL) {
					fprintf(stderr, "malloc failed in cluster forward\n");
					free(buf);
					return 0;
				}
				cl->type = FORWARD;
				cl->members.forward = NULL;
			} else if (strncmp(p, "any_of", 6) == 0 && isspace(*(p + 6))) {
				p += 7;

				if ((cl = malloc(sizeof(cluster))) == NULL) {
					fprintf(stderr, "malloc failed in cluster any_of\n");
					free(buf);
					return 0;
				}
				cl->type = ANYOF;
				cl->members.forward = NULL;
			} else {
				char *type = p;
				for (; *p != '\0' && !isspace(*p); p++)
					;
				*p = 0;
				fprintf(stderr, "unknown cluster type '%s' for cluster %s\n",
						type, name);
				free(buf);
				return 0;
			}

			if (cl != NULL) {
				/* parse ips */
				for (; *p != '\0' && isspace(*p); p++)
					;
				w = NULL;
				do {
					char ipbuf[64];  /* should be enough for everyone */
					char *ip = ipbuf;
					int port = 2003;
					for (; *p != '\0' && !isspace(*p) && *p != ';'; p++) {
						if (*p == ':') {
							*p = '\0';
							port = atoi(p + 1);
						}
						if (ip - ipbuf < sizeof(ipbuf) - 1)
							*ip++ = *p;
					}
					*ip = '\0';
					if (*p == '\0') {
						fprintf(stderr, "unexpected end of file at '%s' "
								"for cluster %s\n", ip, name);
						free(cl);
						free(buf);
						return 0;
					} else if (*p != ';') {
						*p++ = '\0';
					}

					if (cl->type == CARBON_CH) {
						if (w == NULL) {
							cl->members.carbon_ch.servers = w =
								malloc(sizeof(servers));
						} else {
							w = w->next = malloc(sizeof(servers));
						}
						if (w == NULL) {
							fprintf(stderr, "malloc failed in carbon_ch ip\n");
							free(cl);
							free(buf);
							return 0;
						}
						w->next = NULL;
						w->server = server_new(ipbuf, (unsigned short)port);
						if (w->server == NULL) {
							fprintf(stderr, "failed to add server %s:%d "
									"to carbon_ch: %s\n", ipbuf, port,
									strerror(errno));
							free(w);
							free(cl);
							free(buf);
							return 0;
						}
						cl->members.carbon_ch.ring = carbon_addnode(
								cl->members.carbon_ch.ring,
								w->server);
						if (cl->members.carbon_ch.ring == NULL) {
							fprintf(stderr, "failed to add server %s:%d "
									"to ring: out of memory\n", ipbuf, port);
							free(cl);
							free(buf);
							return 0;
						}
					} else if (cl->type == FORWARD || cl->type == ANYOF) {
						if (w == NULL) {
							w = malloc(sizeof(servers));
						} else {
							w = w->next = malloc(sizeof(servers));
						}
						if (w == NULL) {
							fprintf(stderr, "malloc failed in cluster %s ip\n",
									cl->type == FORWARD ? "forward" : "any_of");
							free(cl);
							free(buf);
							return 0;
						}
						w->next = NULL;
						if (cl->type == ANYOF && cl->members.forward != NULL) {
							w->server = server_backup(
									ipbuf,
									(unsigned short)port,
									cl->members.forward->server);
						} else {
							w->server = server_new(ipbuf, (unsigned short)port);
						}
						if (w->server == NULL) {
							fprintf(stderr, "failed to add server %s:%d "
									"to forwarders: %s\n", ipbuf, port,
									strerror(errno));
							free(w);
							free(cl);
							free(buf);
							return 0;
						}
						if (cl->members.forward == NULL)
							cl->members.forward = w;
					}
					for (; *p != '\0' && isspace(*p); p++)
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
				free(buf);
				return 0;
			}
			*p++ = '\0';
			for (; *p != '\0' && isspace(*p); p++)
				;
			if (strncmp(p, "send", 4) != 0 || !isspace(*(p + 4))) {
				fprintf(stderr, "expected 'send to' after match %s\n", pat);
				free(buf);
				return 0;
			}
			p += 5;
			for (; *p != '\0' && isspace(*p); p++)
				;
			if (strncmp(p, "to", 2) != 0 || !isspace(*(p + 2))) {
				fprintf(stderr, "expected 'send to' after match %s\n", pat);
				free(buf);
				return 0;
			}
			p += 3;
			for (; *p != '\0' && isspace(*p); p++)
				;
			dest = p;
			for (; *p != '\0' && !isspace(*p) && *p != ';'; p++)
				;
			if (*p == '\0') {
				fprintf(stderr, "unexpected end of file after 'send to %s'\n",
						dest);
				free(buf);
				return 0;
			} else if (*p == ';') {
				*p++ = '\0';
				stop = 0;
			} else {
				*p++ = '\0';
			}
			if (stop == -1) {
				/* look for either ';' or 'stop' followed by ';' */
				for (; *p != '\0' && isspace(*p); p++)
					;
				if (*p == ';') {
					p++;
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
									"match %s\n", pat);
							free(buf);
							return 0;
						}
					}
					p++;
					stop = 1;
				} else {
					fprintf(stderr, "expected 'stop' and/or ';' after '%s' "
							"for match %s\n", dest, pat);
					free(buf);
					return 0;
				}
			}

			/* lookup dest */
			for (w = clusters; w != NULL; w = w->next) {
				if (strcmp(w->name, dest) == 0)
					break;
			}
			if (w == NULL) {
				fprintf(stderr, "no such cluster '%s' for 'match %s'\n",
						dest, pat);
				free(buf);
				return 0;
			}
			if (r == NULL) {
				routes = r = malloc(sizeof(route));
			} else {
				r = r->next = malloc(sizeof(route));
			}
			r->dest = w;
			if (strcmp(pat, "*") == 0) {
				r->matchall = 1;
				r->pattern = NULL;
			} else {
				if (regcomp(&r->rule, pat, REG_EXTENDED | REG_NOSUB) != 0) {
					fprintf(stderr, "invalid expression '%s' for match\n",
							pat);
					free(r);
					free(buf);
					return 0;
				}
				r->pattern = strdup(pat);
				r->matchall = 0;
			}
			r->stop = stop;
			r->next = NULL;
		} else if (strncmp(p, "aggregate", 9) == 0 && isspace(*(p + 9))) {
			/* aggregation rule */
			char *pat;
			char *interval;
			char *expire;
			cluster *w;
			struct _aggr_computes *ac = NULL;

			p += 10;
			for (; *p != '\0' && isspace(*p); p++)
				;

			w = malloc(sizeof(cluster));
			w->name = NULL;
			w->type = AGGREGATION;
			w->next = NULL;

			do {
				pat = p;
				for (; *p != '\0' && !isspace(*p); p++)
					;
				if (*p == '\0') {
					fprintf(stderr, "unexpected end of file after 'aggregate'\n");
					free(buf);
					return 0;
				}
				*p++ = '\0';
				for (; *p != '\0' && isspace(*p); p++)
					;

				if (r == NULL) {
					routes = r = malloc(sizeof(route));
				} else {
					r = r->next = malloc(sizeof(route));
				}
				if (regcomp(&r->rule, pat, REG_EXTENDED | REG_NOSUB) != 0) {
					fprintf(stderr, "invalid expression '%s' for aggregation\n",
							pat);
					free(r);
					free(buf);
					return 0;
				}
				r->pattern = strdup(pat);
				r->dest = w;
				r->stop = 0;
				r->matchall = 0;
				r->next = NULL;
			} while (strncmp(p, "every", 5) != 0 || !isspace(*(p + 5)));
			p += 6;
			for (; *p != '\0' && isspace(*p); p++)
				;
			interval = p;
			for (; *p != '\0' && isdigit(*p); p++)
				;
			if (*p == '\0') {
				fprintf(stderr, "unexpected end of file after 'every %s'\n",
						interval);
				free(buf);
				return 0;
			}
			if (!isspace(*p)) {
				fprintf(stderr, "unexpected character '%c', "
						"expected number after 'every'\n", *p);
				free(buf);
				return 0;
			}
			*p++ = '\0';
			for (; *p != '\0' && isspace(*p); p++)
				;
			if (strncmp(p, "seconds", 7) != 0 || !isspace(*(p + 7))) {
				fprintf(stderr, "expected 'seconds' after 'every %s'\n",
						interval);
				free(buf);
				return 0;
			}
			p += 7;
			for (; *p != '\0' && isspace(*p); p++)
				;
			if (strncmp(p, "expire", 6) != 0 || !isspace(*(p + 6))) {
				fprintf(stderr, "expected 'expire after' after 'every %s seconds\n",
						interval);
				free(buf);
				return 0;
			}
			p += 7;
			for (; *p != '\0' && isspace(*p); p++)
				;
			if (strncmp(p, "after", 5) != 0 || !isspace(*(p + 5))) {
				fprintf(stderr, "expected 'after' after 'expire'\n");
				free(buf);
				return 0;
			}
			p += 6;
			for (; *p != '\0' && isspace(*p); p++)
				;
			expire = p;
			for (; *p != '\0' && isdigit(*p); p++)
				;
			if (*p == '\0') {
				fprintf(stderr, "unexpected end of file after 'expire after %s'\n",
						expire);
				free(buf);
				return 0;
			}
			if (!isspace(*p)) {
				fprintf(stderr, "unexpected character '%c', "
						"expected number after 'expire after'\n", *p);
				free(buf);
				return 0;
			}
			*p++ = '\0';
			w->members.aggregation = aggregator_new(atoi(interval), atoi(expire));
			for (; *p != '\0' && isspace(*p); p++)
				;
			if (strncmp(p, "seconds", 7) != 0 || !isspace(*(p + 7))) {
				fprintf(stderr, "expected 'seconds' after 'expire after %s'\n",
						expire);
				free(buf);
				return 0;
			}
			p += 8;
			for (; *p != '\0' && isspace(*p); p++)
				;
			do {
				if (strncmp(p, "compute", 7) != 0 || !isspace(*(p + 7))) {
					fprintf(stderr, "expected 'compute' at %s'\n", p);
					free(buf);
					return 0;
				}
				p += 8;
				pat = p;
				for (; *p != '\0' && !isspace(*p); p++)
					;
				if (*p == '\0') {
					fprintf(stderr, "unexpected end of file after 'compute'\n");
					free(buf);
					return 0;
				}
				*p++ = '\0';

				if (ac == NULL) {
					ac = w->members.aggregation->computes = malloc(sizeof(struct _aggr_computes));
				} else {
					ac = ac->next = malloc(sizeof(struct _aggr_computes));
				}
				if (strcmp(pat, "sum") == 0) {
					ac->type = SUM;
				} else if (strcmp(pat, "count") == 0) {
					ac->type = CNT;
				} else if (strcmp(pat, "max") == 0) {
					ac->type = MAX;
				} else if (strcmp(pat, "min") == 0) {
					ac->type = MIN;
				} else if (strcmp(pat, "average") == 0) {
					ac->type = AVG;
				} else {
					fprintf(stderr, "expected sum, count, max, min or average "
							"after 'compute', got '%s'\n", pat);
					free(buf);
					return 0;
				}
				for (; *p != '\0' && isspace(*p); p++)
					;
				if (strncmp(p, "write", 5) != 0 || !isspace(*(p + 5))) {
					fprintf(stderr, "expected 'write to' after 'compute %s'\n", pat);
					free(buf);
					return 0;
				}
				p += 6;
				for (; *p != '\0' && isspace(*p); p++)
					;
				if (strncmp(p, "to", 2) != 0 || !isspace(*(p + 2))) {
					fprintf(stderr, "expected 'write to' after 'compute %s'\n", pat);
					free(buf);
					return 0;
				}
				p += 3;
				for (; *p != '\0' && isspace(*p); p++)
					;
				pat = p;
				for (; *p != '\0' && !isspace(*p); p++)
					;
				if (*p == '\0') {
					fprintf(stderr, "unexpected end of file after 'write to'\n");
					free(buf);
					return 0;
				}
				*p++ = '\0';
				ac->metric = strdup(pat);
				ac->next = NULL;
				for (; *p != '\0' && isspace(*p); p++)
					;
			} while (*p != ';');
			p++;
		} else {
			/* garbage? */
			fprintf(stderr, "garbage in config: %s\n", p);
			free(buf);
			return 0;
		}
	} while (*p != '\0');

	free(buf);
	return 1;
}

/**
 * Mere debugging function to check if the configuration is picked up
 * alright.
 */
void
router_printconfig(FILE *f)
{
	cluster *c;
	route *r;
	servers *s;

	for (c = clusters; c != NULL; c = c->next) {
		fprintf(f, "cluster %s\n", c->name);
		if (c->type == FORWARD || c->type == ANYOF) {
			fprintf(f, "\t%s\n", c->type == FORWARD ? "forward" : "any_of");
			for (s = c->members.forward; s != NULL; s = s->next)
				fprintf(f, "\t\t%s:%d\n",
						server_ip(s->server), server_port(s->server));
		} else if (c->type == CARBON_CH) {
			fprintf(f, "\tcarbon_ch replication %d\n",
					c->members.carbon_ch.repl_factor);
			for (s = c->members.carbon_ch.servers; s != NULL; s = s->next)
				fprintf(f, "\t\t%s:%d\n",
						server_ip(s->server), server_port(s->server));
		}
		fprintf(f, "\t;\n");
	}
	fprintf(f, "\n");
	for (r = routes; r != NULL; r = r->next) {
		if (r->dest->type == AGGREGATION) {
			cluster *aggr = r->dest;
			struct _aggr_computes *ac;

			fprintf(f, "aggregate\n");
			do {
				fprintf(f, "\t\t%s\n", r->pattern);
			} while (r->next != NULL && r->next->dest == aggr
					&& (r = r->next) != NULL);
			fprintf(f, "\tevery %u seconds\n"
					"\texpire after %u seconds\n",
					aggr->members.aggregation->interval,
					aggr->members.aggregation->expire);
			for (ac = aggr->members.aggregation->computes; ac != NULL; ac = ac->next)
				fprintf(f, "\tcompute %s write to\n"
						"\t\t%s\n",
						ac->type == SUM ? "sum" : ac->type == CNT ? "count" :
						ac->type == MAX ? "max" : ac->type == MIN ? "min" :
						ac->type == AVG ? "average" : "<unknown>", ac->metric);
			fprintf(f, "\t;\n");
		} else {
			fprintf(f, "match %s\n\tsend to %s%s\n\t;\n",
					r->matchall ? "*" : r->pattern,
					r->dest->name,
					r->stop ? "\n\tstop" : "");
		}
	}
	fflush(f);
}

static void
router_route_destination(
		const route *w,
		const char *metric_path,
		const char *metric)
{
	switch (w->dest->type) {
		case FORWARD: {
			/* simple case, no logic necessary */
			servers *s;
			for (s = w->dest->members.forward; s != NULL; s = s->next)
				server_send(s->server, metric);
		}	break;
		case ANYOF: {
			/* only queue at the first, since all servers here
			 * share the same queue */
			if (w->dest->members.forward != NULL)
				server_send(w->dest->members.forward->server, metric);
		}	break;
		case CARBON_CH: {
			/* let the ring(bearer) decide */
			server *dests[4];
			int i;
			if (w->dest->members.carbon_ch.repl_factor > sizeof(dests)) {
				/* need to increase size of dests, for
				 * performance and ssp don't want to malloc and
				 * can't alloca */
				fprintf(stderr, "PANIC: increase dests array size!\n");
				return;
			}
			carbon_get_nodes(
					dests,
					w->dest->members.carbon_ch.ring,
					w->dest->members.carbon_ch.repl_factor,
					metric_path);
			for (i = 0; i < w->dest->members.carbon_ch.repl_factor; i++)
				server_send(dests[i], metric);
		}	break;
		case AGGREGATION: {
			/* aggregation rule */
			aggregator_putmetric(
					w->dest->members.aggregation,
					metric);
		}	break;
	}
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
		if (w->matchall || regexec(&w->rule, metric, 0, NULL, 0) == 0) {
			/* rule matches, send to destination(s) */
			router_route_destination(w, metric_path, metric);

			/* stop processing further rules if requested */
			if (w->stop)
				break;
		}
	}
}

void
router_shutdown(void)
{
	server **s = server_get_servers();
	server **w;

	for (w = s; *w != NULL; w++)
		server_shutdown(*w);

	free(s);
}
