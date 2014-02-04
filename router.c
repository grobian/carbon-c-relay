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
#include "relay.h"

enum clusttype {
	GROUP,      /* pseudo type to create a matching tree */
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
		struct _route *routes;
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

typedef struct _block {
	char *pattern;
	size_t hash;
	char prio;
	size_t refcnt;
	size_t seqnr;
	route *firstroute;
	route *lastroute;
	struct _block *prev;
	struct _block *next;
} block;

/**
 * Tries to optimise the match and aggregation rules in such a way that
 * the number of matches for non-matching metrics are reduced.  The
 * problem is that with many metrics flowing in, the time to perform
 * lots of regex matches is high.  This is not too bad if that time
 * spent actually results in a metric being counted (aggregation) or
 * sent further (match), but it is when the metric would be discarded
 * for it did not match anything.
 * Hence, we employ a simple strategy to try and reduce the incoming
 * stream of metrics as soon as possible before performing the more
 * specific and expensive matches to confirm fit.
 */
void
router_optimise(void)
{
	char *p;
	char pblock[64];
	char *b;
	route *rwalk;
	route *rnext;
	block *blocks;
	block *bwalk;
	block *bstart;
	block *blast;
	size_t bsum;
	size_t seq;

	/* avoid optimising anything if it won't pay off */
	seq = 0;
	for (rwalk = routes; rwalk != NULL && seq < 50; rwalk = rwalk->next)
		seq++;
	if (seq < 50)
		return;

	/* Heuristic: the last part of the matching regex is the most
	 * discriminating part of the metric.  The last part is defined as a
	 * block of characters matching [a-zA-Z_]+ at the end disregarding
	 * any characters not matched by the previous expression.  Then from
	 * these last parts we create groups, that -- if having enough
	 * members -- is used to reduce the amount of comparisons done
	 * before determining that an input metric cannot match any regex we
	 * have defined. */
	seq = 0;
	blast = bstart = blocks = malloc(sizeof(block));
	blocks->refcnt = 0;
	blocks->seqnr = seq++;
	blocks->prev = NULL;
	blocks->next = NULL;
	for (rwalk = routes; rwalk != NULL; rwalk = rnext) {
		/* matchall rules cannot be in a group */
		if (rwalk->matchall) {
			blast->next = malloc(sizeof(block));
			blast->next->prev = blast;
			blast = blast->next;
			blast->pattern = NULL;
			blast->hash = 0;
			blast->prio = 1;
			blast->refcnt = 1;
			blast->seqnr = seq++;
			blast->firstroute = rwalk;
			blast->lastroute = rwalk;
			blast->next = NULL;
			rnext = rwalk->next;
			rwalk->next = NULL;
			bstart = blast;
			continue;
		}

		p = rwalk->pattern + strlen(rwalk->pattern);
		/* strip off chars that won't belong to a block */
		while (
				p > rwalk->pattern &&
				(*p < 'a' || *p > 'z') &&
				(*p < 'A' || *p > 'Z') &&
				*p != '_'
			  )
			p--;
		if (p == rwalk->pattern) {
			/* nothing we can do with a pattern like this */
			blast->next = malloc(sizeof(block));
			blast->next->prev = blast;
			blast = blast->next;
			blast->pattern = NULL;
			blast->hash = 0;
			blast->prio = rwalk->stop ? 1 : 2;
			blast->refcnt = 1;
			blast->seqnr = seq;
			blast->firstroute = rwalk;
			blast->lastroute = rwalk;
			blast->next = NULL;
			rnext = rwalk->next;
			rwalk->next = NULL;
			if (rwalk->stop) {
				bstart = blast;
				seq++;
			}
			continue;
		}
		/* find the block */
		bsum = 0;
		b = pblock;
		while (
				p > rwalk->pattern && b - pblock < sizeof(pblock) &&
				(
				(*p >= 'a' && *p <= 'z') ||
				(*p >= 'A' && *p <= 'Z') ||
				*p == '_'
				)
			  )
		{
			bsum += *p;
			*b++ = *p--;
		}
		*b = '\0';
		b = pblock;
		if (strlen(b) < 3) {
			/* this probably isn't selective enough */
			blast->next = malloc(sizeof(block));
			blast->next->prev = blast;
			blast = blast->next;
			blast->pattern = NULL;
			blast->hash = 0;
			blast->prio = rwalk->stop ? 1 : 2;
			blast->refcnt = 1;
			blast->seqnr = seq;
			blast->firstroute = rwalk;
			blast->lastroute = rwalk;
			blast->next = NULL;
			rnext = rwalk->next;
			rwalk->next = NULL;
			if (rwalk->stop) {
				bstart = blast;
				seq++;
			}
			continue;
		}

		/* at this point, b points to the tail block in reverse, see if
		 * we already had such tail in place */
		for (bwalk = bstart->next; bwalk != NULL; bwalk = bwalk->next) {
			if (bwalk->hash != bsum || strcmp(bwalk->pattern, b) != 0)
				continue;
			break;
		}
		if (bwalk == NULL) {
			blast->next = malloc(sizeof(block));
			blast->next->prev = blast;
			blast = blast->next;
			blast->pattern = strdup(b);
			blast->hash = bsum;
			blast->prio = rwalk->stop ? 1 : 2;
			blast->refcnt = 1;
			blast->seqnr = seq;
			blast->firstroute = rwalk;
			blast->lastroute = rwalk;
			blast->next = NULL;
			rnext = rwalk->next;
			rwalk->next = NULL;
			if (rwalk->stop) {
				bstart = blast;
				seq++;
			}
			continue;
		}

		bwalk->refcnt++;
		bwalk->lastroute = bwalk->lastroute->next = rwalk;
		rnext = rwalk->next;
		rwalk->next = NULL;
		if (rwalk->stop) {
			/* move this one to the end */
			if (bwalk->next != NULL) {
				bwalk->prev->next = bwalk->next;
				blast = blast->next = bwalk;
				bwalk->next = NULL;
			}
			bwalk->prio = 1;
			bstart = blast;
			seq++;
		}
	}
	/* make loop below easier by appending a dummy (reuse the one from
	 * start) */
	blast = blast->next = blocks;
	blocks = blocks->next;
	blast->next = NULL;
	blast->seqnr = seq;

	rwalk = routes = NULL;
	seq = 1;
	bstart = NULL;
	/* create groups, if feasible */
	for (bwalk = blocks; bwalk != NULL; bwalk = blast) {
		if (bwalk->seqnr != seq) {
			seq++;
			if (bstart != NULL) {
				bstart->next = bwalk;
				bwalk = bstart;
			} else {
				blast = bwalk;
				continue;
			}
		} else if (bwalk->prio == 1) {
			bstart = bwalk;
			blast = bwalk->next;
			bstart->next = NULL;
			continue;
		}

		if (bwalk->refcnt == 0) {
			blast = bwalk->next;
			free(bwalk);
			continue;
		} else if (bwalk->refcnt < 3) {
			if (routes == NULL) {
				rwalk = routes = bwalk->firstroute;
			} else {
				rwalk->next = bwalk->firstroute;
			}
			rwalk = bwalk->lastroute;
			blast = bwalk->next;
			free(bwalk->pattern);
			free(bwalk);
		} else {
			if (routes == NULL) {
				rwalk = routes = malloc(sizeof(route));
			} else {
				rwalk = rwalk->next = malloc(sizeof(route));
			}
			rwalk->pattern = NULL;
			rwalk->stop = 0;
			rwalk->matchall = 0;
			rwalk->dest = malloc(sizeof(cluster));
			rwalk->dest->name = bwalk->pattern;
			rwalk->dest->type = GROUP;
			rwalk->dest->members.routes = bwalk->firstroute;
			rwalk->dest->next = NULL;
			rwalk->next = NULL;
			blast = bwalk->next;
			free(bwalk);
		}
		if (bwalk == bstart)
			bstart = NULL;
	}
}

/**
 * Mere debugging function to check if the configuration is picked up
 * alright.  If all is set to false, aggregation rules won't be printed.
 * This comes in handy because aggregations usually come in the order of
 * thousands.
 */
void
router_printconfig(FILE *f, char all)
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

			if (!all)
				continue;

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
		} else if (r->dest->type == GROUP) {
			size_t cnt = 0;
			route *rwalk;
			char blockname[64];
			char *b = &blockname[sizeof(blockname) - 1];
			char *p;
			
			for (rwalk = r->dest->members.routes; rwalk != NULL; rwalk = rwalk->next)
				cnt++;
			/* reverse the name, to make it human consumable */
			*b-- ='\0';
			for (p = r->dest->name; *p != '\0' && b > blockname; p++)
				*b-- = *p;
			fprintf(f, "# group %s: contains %zd aggregations/matches\n",
					++b, cnt);
		} else {
			fprintf(f, "match %s\n\tsend to %s%s\n\t;\n",
					r->matchall ? "*" : r->pattern,
					r->dest->name,
					r->stop ? "\n\tstop" : "");
		}
	}
	fflush(f);
}

static char
router_route_intern(
		const char *metric_path,
		const char *metric,
		const route *r)
{
	const route *w;
	char stop = 0;
	const char *p;
	const char *q = NULL;
	const char *t;

	for (w = r; w != NULL && keep_running != 0; w = w->next) {
		if (w->dest->type == GROUP) {
			/* strrstr doesn't exist, grrr
			 * therefore the pattern in the group is stored in reverse,
			 * such that we can start matching the tail easily without
			 * having to calculate the end of the pattern string all the
			 * time */
			for (p = metric_path + strlen(metric_path) - 1; p >= metric_path; p--) {
				for (q = w->dest->name, t = p; *q != '\0' && t >= metric_path; q++, t--) {
					if (*q != *t)
						break;
				}
				if (*q == '\0')
					break;
			}
			/* indirection */
			if (*q == '\0')
				stop = router_route_intern(
						metric_path,
						metric,
						w->dest->members.routes);
		} else if (w->matchall || regexec(&w->rule, metric_path, 0, NULL, 0) == 0) {
			stop = w->stop;
			/* rule matches, send to destination(s) */
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
						return 1;
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
				case GROUP: {
					/* this should not happen */
				}	break;
			}

			/* stop processing further rules if requested */
			if (stop)
				break;
		}
	}

	return stop;
}

/**
 * Looks up the locations the given metric_path should be sent to, and
 * enqueues the metric for the matching destinations.
 */
inline void
router_route(const char *metric_path, const char *metric)
{
	(void)router_route_intern(metric_path, metric, routes);
}

/**
 * Prints for metric_path which rules and/or aggregations would be
 * triggered.  Useful for testing regular expressions.
 */
void
router_test(const char *metric_path)
{
	route *w;
	char gotmatch = 0;

	for (w = routes; w != NULL; w = w->next) {
		if (w->matchall || regexec(&w->rule, metric_path, 0, NULL, 0) == 0) {
			gotmatch = 1;
			fprintf(stdout, "%s is matched by %s (%s)\n",
					metric_path, w->matchall ? "*" : w->pattern,
					w->dest->type == AGGREGATION ? "aggregation" : "match");
			if (w->stop) {
				fprintf(stdout, "stop\n");
				break;
			}
		}
	}
	if (!gotmatch)
		fprintf(stdout, "nothing matched %s\n", metric_path);
	fflush(stdout);
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
