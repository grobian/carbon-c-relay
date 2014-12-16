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


#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <regex.h>
#include <sys/stat.h>
#include <errno.h>

#include "consistent-hash.h"
#include "server.h"
#include "queue.h"
#include "aggregator.h"
#include "relay.h"

enum clusttype {
	BLACKHOLE,  /* /dev/null-like destination */
	GROUP,      /* pseudo type to create a matching tree */
	FORWARD,
	CARBON_CH,  /* room for a better/different hash definition */
	FNV1A_CH,   /* FNV1a-based consistent-hash */
	ANYOF,      /* FNV1a-based hash, but with backup by others */
	AGGREGATION,
	REWRITE
};

typedef struct _servers {
	server *server;
	struct _servers *next;
} servers;

typedef struct {
	unsigned char repl_factor;
	ch_ring *ring;
	servers *servers;
} chashring;

typedef struct {
	unsigned short count;
	server **servers;
	servers *list;
} serverlist;

typedef struct _cluster {
	char *name;
	enum clusttype type;
	union {
		chashring *ch;
		servers *forward;
		serverlist *anyof;
		aggregator *aggregation;
		struct _route *routes;
		char *replacement;
	} members;
	struct _cluster *next;
} cluster;

typedef struct _route {
	char *pattern;    /* original regex input, used for printing only */
	regex_t rule;     /* regex on metric, only if type == REGEX */
	size_t nmatch;    /* number of match groups */
	char *strmatch;   /* string to search for if type not REGEX or MATCHALL */
	cluster *dest;    /* where matches should go */
	char stop:1;      /* whether to continue matching rules after this one */
	enum {
		MATCHALL,     /* the '*', don't do anything, just match everything */
		REGEX,        /* a regex match */
		CONTAINS,     /* find string occurrence */
		STARTS_WITH,  /* metric must start with string */
		ENDS_WITH,    /* metric must end with string */ 
		MATCHES       /* metric matches string exactly */
	} matchtype;      /* how to interpret the pattern */
	struct _route *next;
} route;

static route *routes = NULL;
static cluster *clusters = NULL;
static char keep_running = 1;

/* custom constant, meant to force regex mode matching */
#define REG_FORCE   01000000 

/**
 * Examines pattern and sets matchtype and rule or strmatch in route.
 */
static int
determine_if_regex(route *r, char *pat, int flags)
{
	/* try and see if we can avoid using a regex match, for
	 * it is simply very slow/expensive to do so: most of
	 * the time, people don't need fancy matching rules */
	char patbuf[8192];
	char *e = pat;
	char *pb = patbuf;
	char escape = 0;
	r->matchtype = CONTAINS;
	r->nmatch = 0;

	if (flags & REG_FORCE) {
		flags &= ~REG_NOSUB;
		r->matchtype = REGEX;
	}

	if (*e == '^' && r->matchtype == CONTAINS) {
		e++;
		r->matchtype = STARTS_WITH;
	}
	for (; *e != '\0'; e++) {
		switch (*e) {
			case '\\':
				if (escape)
					*pb++ = *e;
				escape = !escape;
				break;
			case '.':
			case '^':
			case '*':
			case '+':
				if (!escape)
					r->matchtype = REGEX;
				*pb++ = *e;
				escape = 0;
				break;
			case '$':
				if (!escape && e[1] == '\0') {
					if (r->matchtype == STARTS_WITH) {
						r->matchtype = MATCHES;
					} else {
						r->matchtype = ENDS_WITH;
					}
				} else {
					r->matchtype = REGEX;
				}
				escape = 0;
				break;
			default:
				if (
						!escape && (
							(*e == '_') || (*e == '-') ||
							(*e >= '0' && *e <= '9') ||
							(*e >= 'a' && *e <= 'z') ||
							(*e >= 'A' && *e <= 'Z')
						)
				   )
				{
					*pb++ = *e;
				} else {
					r->matchtype = REGEX;
				}
				escape = 0;
				break;
		}
		if (pb - patbuf == sizeof(patbuf))
			r->matchtype = REGEX;
		if (r->matchtype == REGEX)
			break;
	}
	if (r->matchtype != REGEX) {
		*pb = '\0';
		r->strmatch = strdup(patbuf);
		r->pattern = strdup(pat);
	} else {
		int ret = regcomp(&r->rule, pat, flags & ~REG_FORCE);
		if (ret != 0)
			return ret;  /* allow use of regerror */
		r->strmatch = NULL;
		r->pattern = strdup(pat);
		if (((flags & REG_NOSUB) == 0 && r->rule.re_nsub > 0) ||
				flags & REG_FORCE)
		{
			/* we need +1 because position 0 contains the entire
			 * expression */
			r->nmatch = r->rule.re_nsub + 1;
			if (r->nmatch > RE_MAX_MATCHES) {
				fprintf(stderr, "determine_if_regex: too many match groups, "
						"please increase RE_MAX_MATCHES in router.h\n");
				free(r->pattern);
				return REG_ESPACE;  /* lie closest to the truth */
			}
		}
	}

	return 0;
}

/**
 * Populates the routing tables by reading the config file.
 *
 * Config file supports the following:
 *
 * cluster (name)
 *     (forward | any_of | carbon_ch [replication (count)])
 *         (ip:port [proto (tcp | udp)] ...)
 *     ;
 * match (* | regex)
 *     send to (cluster | blackhole)
 *     [stop]
 *     ;
 * rewrite (regex)
 *     into (replacement)
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
router_readconfig(const char *path, size_t queuesize, size_t batchsize)
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

	/* create virtual blackhole cluster */
	cl = malloc(sizeof(cluster));
	cl->name = strdup("blackhole");
	cl->type = BLACKHOLE;
	cl->members.forward = NULL;
	cl->next = NULL;
	clusters = cl;

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
			for (; *p != '\0' && isspace(*p); p++)
				;
			if ((strncmp(p, "carbon_ch", 9) == 0 && isspace(*(p + 9))) ||
					(strncmp(p, "fnv1a_ch", 8) == 0 && isspace(*(p + 8))))
			{
				int replcnt = 1;
				enum clusttype chtype = *p == 'c' ? CARBON_CH : FNV1A_CH;

				p += chtype == CARBON_CH ? 10 : 9;
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

				if ((cl = cl->next = malloc(sizeof(cluster))) == NULL) {
					fprintf(stderr, "malloc failed in cluster %s\n", name);
					free(buf);
					return 0;
				}
				cl->type = chtype;
				cl->members.ch = malloc(sizeof(chashring));
				cl->members.ch->repl_factor = (unsigned char)replcnt;
				cl->members.ch->ring =
					ch_new(chtype == CARBON_CH ? CARBON : FNV1a);
			} else if (strncmp(p, "forward", 7) == 0 && isspace(*(p + 7))) {
				p += 8;

				if ((cl = cl->next = malloc(sizeof(cluster))) == NULL) {
					fprintf(stderr, "malloc failed in cluster forward\n");
					free(buf);
					return 0;
				}
				cl->type = FORWARD;
				cl->members.forward = NULL;
			} else if (strncmp(p, "any_of", 6) == 0 && isspace(*(p + 6))) {
				p += 7;

				if ((cl = cl->next = malloc(sizeof(cluster))) == NULL) {
					fprintf(stderr, "malloc failed in cluster any_of\n");
					free(buf);
					return 0;
				}
				cl->type = ANYOF;
				cl->members.anyof = NULL;
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

			/* parse ips */
			for (; *p != '\0' && isspace(*p); p++)
				;
			w = NULL;
			do {
				char termchr;
				char *lastcolon = NULL;
				char *ip = p;
				char *proto = "tcp";
				int port = 2003;
				server *newserver = NULL;

				for (; *p != '\0' && !isspace(*p) && *p != ';'; p++)
					if (*p == ':')
						lastcolon = p;

				if (*p == '\0') {
					fprintf(stderr, "unexpected end of file at '%s' "
							"for cluster %s\n", ip, name);
					free(cl);
					free(buf);
					return 0;
				}

				termchr = *p;
				*p = '\0';

				if (*(p - 1) == ']')
					lastcolon = NULL;
				if (lastcolon != NULL) {
					*lastcolon = '\0';
					port = atoi(lastcolon + 1);
				}
				if (*ip == '[') {
					ip++;
					if (lastcolon != NULL && *(lastcolon - 1) == ']') {
						*(lastcolon - 1) = '\0';
					} else if (lastcolon == NULL && *(p - 1) == ']') {
						*(p - 1) = '\0';
					} else {
						fprintf(stderr, "expected ']' at '%s' "
								"for cluster %s\n", ip, name);
						free(cl);
						free(buf);
						return 0;
					}
				}

				if (isspace(termchr)) {
					p++;
					for (; *p != '\0' && isspace(*p); p++)
						;
					if (strncmp(p, "proto", 5) == 0 && isspace(*(p + 5))) {
						p += 6;

						for (; *p != '\0' && isspace(*p); p++)
							;
						proto = p;
						for (; *p != '\0' && !isspace(*p) && *p != ';'; p++)
							;
						termchr = *p;
						*p = '\0';

						if (strcmp(proto, "tcp") != 0
								&& strcmp(proto, "udp") != 0)
						{
							fprintf(stderr, "expected 'udp' or 'tcp' after "
									"'proto' at '%s' for cluster %s\n",
									proto, name);
							free(cl);
							free(buf);
							return 0;
						}
					} else {
						termchr = *p;
					}
				}

				newserver = server_new(ip, (unsigned short)port,
						*proto == 'u' ? CON_UDP : CON_TCP,
						queuesize, batchsize);
				if (newserver == NULL) {
					fprintf(stderr, "failed to add server %s:%d (%s) "
							"to cluster %s: %s\n", ip, port, proto,
							name, strerror(errno));
					free(cl);
					free(buf);
					return 0;
				}

				if (cl->type == CARBON_CH || cl->type == FNV1A_CH) {
					if (w == NULL) {
						cl->members.ch->servers = w =
							malloc(sizeof(servers));
					} else {
						w = w->next = malloc(sizeof(servers));
					}
					if (w == NULL) {
						fprintf(stderr, "malloc failed for %s_ch %s\n",
								cl->type == CARBON_CH ? "carbon" : "fnv1a", ip);
						free(cl);
						free(buf);
						return 0;
					}
					w->next = NULL;
					w->server = newserver;
					cl->members.ch->ring = ch_addnode(
							cl->members.ch->ring,
							w->server);
					if (cl->members.ch->ring == NULL) {
						fprintf(stderr, "failed to add server %s:%d "
								"to ring for cluster %s: out of memory\n",
								ip, port, name);
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
						fprintf(stderr, "malloc failed for %s %s\n",
								cl->type == FORWARD ? "forward" : "any_of", ip);
						free(cl);
						free(buf);
						return 0;
					}
					w->next = NULL;
					w->server = newserver;
					if (cl->type == FORWARD && cl->members.forward == NULL)
						cl->members.forward = w;
					if (cl->type == ANYOF && cl->members.anyof == NULL) {
						cl->members.anyof = malloc(sizeof(serverlist));
						cl->members.anyof->count = 0;
						cl->members.anyof->servers = NULL;
						cl->members.anyof->list = w;
					}
					if (cl->type == ANYOF)
						cl->members.anyof->count++;
				}

				*p = termchr;
				for (; *p != '\0' && isspace(*p); p++)
					;
			} while (*p != ';');
			p++; /* skip over ';' */
			if (cl->type == ANYOF) {
				size_t i = 0;
				cl->members.anyof->servers =
					malloc(sizeof(server *) * cl->members.anyof->count);
				for (w = cl->members.anyof->list; w != NULL; w = w->next)
					cl->members.anyof->servers[i++] = w->server;
				for (w = cl->members.anyof->list; w != NULL; w = w->next)
					server_add_secondaries(w->server,
							cl->members.anyof->servers,
							cl->members.anyof->count);
			}
			cl->name = strdup(name);
			cl->next = NULL;
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
				if (w->type != GROUP &&
						w->type != AGGREGATION &&
						w->type != REWRITE &&
						strcmp(w->name, dest) == 0)
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
				r->pattern = NULL;
				r->strmatch = NULL;
				r->matchtype = MATCHALL;
			} else {
				int err = determine_if_regex(r, pat, REG_EXTENDED | REG_NOSUB);
				if (err != 0) {
					char ebuf[512];
					regerror(err, &r->rule, ebuf, sizeof(ebuf));
					fprintf(stderr, "invalid expression '%s' for match: %s\n",
							pat, ebuf);
					free(r);
					free(buf);
					return 0;
				}
			}
			r->stop = w->type == BLACKHOLE ? 1 : stop;
			r->next = NULL;
		} else if (strncmp(p, "aggregate", 9) == 0 && isspace(*(p + 9))) {
			/* aggregation rule */
			char *type;
			char *pat;
			char *interval;
			char *expire;
			cluster *w;
			int err;

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
				err = determine_if_regex(r, pat, REG_EXTENDED);
				if (err != 0) {
					char ebuf[512];
					regerror(err, &r->rule, ebuf, sizeof(ebuf));
					fprintf(stderr, "invalid expression '%s' "
							"for aggregation: %s\n", pat, ebuf);
					free(r);
					free(buf);
					return 0;
				}
				r->dest = w;
				r->stop = 0;
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

			w->members.aggregation =
				aggregator_new(atoi(interval), atoi(expire));
			if (w->members.aggregation == NULL) {
				fprintf(stderr, "invalid interval (%s) or expire (%s)\n",
						interval, expire);
				free(buf);
				return 0;
			}
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
				type = pat;
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

				if (aggregator_add_compute(w->members.aggregation, pat, type) != 0) {
					fprintf(stderr, "expected sum, count, max, min or average "
							"after 'compute', got '%s'\n", type);
					free(buf);
					return 0;
				}

				for (; *p != '\0' && isspace(*p); p++)
					;
			} while (*p != ';');
			p++;
		} else if (strncmp(p, "rewrite", 7) == 0 && isspace(*(p + 7))) {
			/* rewrite rule */
			char *pat;
			char *replacement;
			int err;

			p += 8;
			for (; *p != '\0' && isspace(*p); p++)
				;
			pat = p;
			for (; *p != '\0' && !isspace(*p); p++)
				;
			if (*p == '\0') {
				fprintf(stderr, "unexpected end of file after 'rewrite'\n");
				free(buf);
				return 0;
			}
			*p++ = '\0';
			for (; *p != '\0' && isspace(*p); p++)
				;
			if (strncmp(p, "into", 4) != 0 || !isspace(*(p + 4))) {
				fprintf(stderr, "expected 'into' after rewrite %s\n", pat);
				free(buf);
				return 0;
			}
			p += 5;
			for (; *p != '\0' && isspace(*p); p++)
				;
			replacement = p;
			for (; *p != '\0' && !isspace(*p) && *p != ';'; p++)
				;
			if (*p == '\0') {
				fprintf(stderr, "unexpected end of file after 'into %s'\n",
						replacement);
				free(buf);
				return 0;
			} else if (*p == ';') {
				*p++ = '\0';
			} else {
				*p++ = '\0';
				for (; *p != '\0' && isspace(*p) && *p != ';'; p++)
					;
				if (*p != ';') {
					fprintf(stderr, "expected ';' after %s\n", replacement);
					free(buf);
					return 0;
				}
				p++;
			}

			if (r == NULL) {
				routes = r = malloc(sizeof(route));
			} else {
				r = r->next = malloc(sizeof(route));
			}
			err = determine_if_regex(r, pat, REG_EXTENDED | REG_FORCE);
			if (err != 0) {
				char ebuf[512];
				regerror(err, &r->rule, ebuf, sizeof(ebuf));
				fprintf(stderr, "invalid expression '%s' for rewrite: %s\n",
						pat, ebuf);
				free(r);
				free(buf);
				return 0;
			}

			if ((cl = cl->next = malloc(sizeof(cluster))) == NULL) {
				fprintf(stderr, "malloc failed for rewrite destination\n");
				free(r);
				free(buf);
				return 0;
			}
			cl->type = REWRITE;
			cl->members.replacement = strdup(replacement);
			cl->next = NULL;

			r->dest = cl;
			r->stop = 0;
			r->next = NULL;
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
	 * before determining that an input metric cannot match any
	 * expression we have defined. */
	seq = 0;
	blast = bstart = blocks = malloc(sizeof(block));
	blocks->refcnt = 0;
	blocks->seqnr = seq++;
	blocks->prev = NULL;
	blocks->next = NULL;
	for (rwalk = routes; rwalk != NULL; rwalk = rnext) {
		/* matchall rules cannot be in a group */
		if (rwalk->matchtype == MATCHALL) {
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
			rwalk->matchtype = CONTAINS;
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

#define PPROTO \
	server_ctype(s->server) == CON_UDP ? " proto udp" : ""

	for (c = clusters; c != NULL; c = c->next) {
		if (c->type == BLACKHOLE || c->type == REWRITE)
			continue;
		fprintf(f, "cluster %s\n", c->name);
		if (c->type == FORWARD) {
			fprintf(f, "    forward\n");
			for (s = c->members.forward; s != NULL; s = s->next)
				fprintf(f, "        %s:%d%s\n",
						server_ip(s->server), server_port(s->server), PPROTO);
		} else if (c->type == ANYOF) {
			fprintf(f, "    any_of\n");
			for (s = c->members.anyof->list; s != NULL; s = s->next)
				fprintf(f, "        %s:%d%s\n",
						server_ip(s->server), server_port(s->server), PPROTO);
		} else if (c->type == CARBON_CH || c->type == FNV1A_CH) {
			fprintf(f, "    %s_ch replication %d\n",
					c->type == CARBON_CH ? "carbon" : "fnv1a",
					c->members.ch->repl_factor);
			for (s = c->members.ch->servers; s != NULL; s = s->next)
				fprintf(f, "        %s:%d%s\n",
						server_ip(s->server), server_port(s->server), PPROTO);
		}
		fprintf(f, "    ;\n");
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
				fprintf(f, "        %s\n", r->pattern);
			} while (r->next != NULL && r->next->dest == aggr
					&& (r = r->next) != NULL);
			fprintf(f, "    every %u seconds\n"
					"    expire after %u seconds\n",
					aggr->members.aggregation->interval,
					aggr->members.aggregation->expire);
			for (ac = aggr->members.aggregation->computes; ac != NULL; ac = ac->next)
				fprintf(f, "    compute %s write to\n"
						"        %s\n",
						ac->type == SUM ? "sum" : ac->type == CNT ? "count" :
						ac->type == MAX ? "max" : ac->type == MIN ? "min" :
						ac->type == AVG ? "average" : "<unknown>", ac->metric);
			fprintf(f, "    ;\n");
		} else if (r->dest->type == REWRITE) {
			fprintf(f, "rewrite %s\n    into %s\n    ;\n",
					r->pattern, r->dest->members.replacement);
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
			fprintf(f, "# common pattern group '%s' "
					"contains %zd aggregations/matches\n",
					++b, cnt);
		} else {
			fprintf(f, "match %s\n    send to %s%s\n    ;\n",
					r->matchtype == MATCHALL ? "*" : r->pattern,
					r->dest->name,
					r->stop ? "\n    stop" : "");
		}
	}
	fflush(f);
}

inline static char
router_metric_matches(
		const route *r,
		char *metric,
		char *firstspace,
		regmatch_t *pmatch)
{
	char ret = 0;

	switch (r->matchtype) {
		case MATCHALL:
			ret = 1;
			break;
		case REGEX:
			*firstspace = '\0';
			ret = regexec(&r->rule, metric, r->nmatch, pmatch, 0) == 0;
			*firstspace = ' ';
			break;
		case CONTAINS:
			*firstspace = '\0';
			ret = strstr(metric, r->strmatch) != NULL;
			*firstspace = ' ';
			break;
		case STARTS_WITH:
			ret = strncmp(metric, r->strmatch, strlen(r->strmatch)) == 0;
			break;
		case ENDS_WITH:
			*firstspace = '\0';
			ret = strcmp(
					firstspace - strlen(r->strmatch),
					r->strmatch) == 0;
			*firstspace = ' ';
			break;
		case MATCHES:
			*firstspace = '\0';
			ret = strcmp(metric, r->strmatch) == 0;
			*firstspace = ' ';
			break;
		default:
			ret = 0;
			break;
	}

	return ret;
}

inline size_t
router_rewrite_metric(
		char (*newmetric)[METRIC_BUFSIZ],
		char **newfirstspace,
		const char *metric,
		const char *firstspace,
		const char *replacement,
		const size_t nmatch,
		const regmatch_t *pmatch)
{
	char escape = 0;
	int ref = 0;
	char *s = *newmetric;
	const char *p;
	const char *q;
	const char *t;

	/* insert leading part */
	q = metric;
	t = metric + pmatch[0].rm_so;
	if (s - *newmetric + t - q < sizeof(*newmetric)) {
		while (q < t)
			*s++ = *q++;
	} else {
		return 0;  /* won't fit, don't try further */
	}

	for (p = replacement; ; p++) {
		switch (*p) {
			case '\\':
				if (!escape) {
					escape = 1;
					break;
				}
				/* fall through so we handle \1\2 */
			default:
				if (escape && *p >= '0' && *p <= '9') {
					ref *= 10;
					ref += *p - '0';
				} else {
					if (escape) {
						if (ref > 0 && ref <= nmatch
								&& pmatch[ref].rm_so >= 0)
						{
							/* insert match part */
							q = metric + pmatch[ref].rm_so;
							t = metric + pmatch[ref].rm_eo;
							if (s - *newmetric + t - q < sizeof(*newmetric))
								while (q < t)
									*s++ = *q++;
						}
						ref = 0;
					}
					if (*p != '\\') { /* \1\2 case */
						escape = 0;
						if (s - *newmetric + 1 < sizeof(*newmetric))
							*s++ = *p;
					}
				}
				break;
		}
		if (*p == '\0')
			break;
	}
	/* undo trailing \0 */
	s--;

	/* insert remaining part */
	q = metric + pmatch[0].rm_eo;
	t = firstspace;
	if (s - *newmetric + t - q < sizeof(*newmetric)) {
		while (q < t)
			*s++ = *q++;
	} else {
		return 0;  /* won't fit, don't try further */
	}

	/* record new position of firstspace */
	*newfirstspace = s;

	/* copy data part */
	if (s - *newmetric + strlen(firstspace) < sizeof(*newmetric))
	{
		for (p = firstspace; *p != '\0'; p++)
			*s++ = *p;
		*s++ = '\0';

		return s - *newmetric;
	}

	return 0;  /* we couldn't copy everything */
}

static char
router_route_intern(
		destination ret[],
		size_t *curlen,
		size_t retsize,
		char *metric,
		char *firstspace,
		const route *r)
{
	const route *w;
	char stop = 0;
	const char *p;
	const char *q = NULL;  /* pacify compiler, won't happen in reality */
	const char *t;
	char newmetric[METRIC_BUFSIZ];
	char *newfirstspace = NULL;
	size_t len;
	regmatch_t pmatch[RE_MAX_MATCHES];

#define failif(RETLEN, WANTLEN) \
	if (WANTLEN > RETLEN) { \
		fprintf(stderr, "router_route: out of destination slots, " \
				"increase CONN_DESTS_SIZE in dispatcher.c\n"); \
		return 1; \
	}


	for (w = r; w != NULL && keep_running; w = w->next) {
		if (w->dest->type == GROUP) {
			/* strrstr doesn't exist, grrr
			 * therefore the pattern in the group is stored in reverse,
			 * such that we can start matching the tail easily without
			 * having to calculate the end of the pattern string all the
			 * time */
			for (p = firstspace - 1; p >= metric; p--) {
				for (q = w->dest->name, t = p; *q != '\0' && t >= metric; q++, t--) {
					if (*q != *t)
						break;
				}
				if (*q == '\0')
					break;
			}
			/* indirection */
			if (*q == '\0')
				stop = router_route_intern(
						ret,
						curlen,
						retsize,
						metric,
						firstspace,
						w->dest->members.routes);
		} else if (router_metric_matches(w, metric, firstspace, pmatch)) {
			stop = w->stop;
			/* rule matches, send to destination(s) */
			switch (w->dest->type) {
				case BLACKHOLE: {
					/* maybe just record we're dropping this metric? */
				}	break;
				case FORWARD: {
					/* simple case, no logic necessary */
					servers *s;
					for (s = w->dest->members.forward; s != NULL; s = s->next)
					{
						failif(retsize, *curlen + 1);
						ret[*curlen].dest = s->server;
						ret[(*curlen)++].metric = strdup(metric);
					}
				}	break;
				case ANYOF: {
					/* we queue the same metrics at the same server */
					unsigned int hash = 2166136261UL;  /* FNV1a */

					for (p = metric; p < firstspace; p++)
						hash = (hash ^ (unsigned int)*p) * 16777619;
					/* We could use the retry approach here, but since
					 * our c is very small compared to MAX_INT, the bias
					 * we introduce for the last few of the range
					 * (MAX_INT % c) can be considered neglicible given
					 * the number of occurances of c in the range of
					 * MAX_INT, therefore we stick with a simple mod. */
					hash %= w->dest->members.anyof->count;
					failif(retsize, *curlen + 1);
					ret[*curlen].dest =
						w->dest->members.anyof->servers[hash];
					ret[(*curlen)++].metric = strdup(metric);
				}	break;
				case CARBON_CH:
				case FNV1A_CH: {
					/* let the ring(bearer) decide */
					failif(retsize,
							*curlen + w->dest->members.ch->repl_factor);
					ch_get_nodes(
							&ret[*curlen],
							w->dest->members.ch->ring,
							w->dest->members.ch->repl_factor,
							metric,
							firstspace);
					*curlen += w->dest->members.ch->repl_factor;
				}	break;
				case AGGREGATION: {
					/* aggregation rule */
					aggregator_putmetric(
							w->dest->members.aggregation,
							metric,
							firstspace,
							w->nmatch, pmatch);
				}	break;
				case REWRITE: {
					/* rewrite metric name */
					if ((len = router_rewrite_metric(
								&newmetric, &newfirstspace,
								metric, firstspace,
								w->dest->members.replacement,
								w->nmatch, pmatch)) == 0)
					{
						fprintf(stderr, "router_route: failed to rewrite "
								"metric: newmetric size too small to hold "
								"replacement (%s -> %s)\n",
								metric, w->dest->members.replacement);
						break;
					};

					/* scary! write back the rewritten metric */
					memcpy(metric, newmetric, len);
					firstspace = metric + (newfirstspace - newmetric);
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
 * returns the list of servers in ret, the number of servers is
 * returned.
 */
inline size_t
router_route(destination ret[], size_t retsize, char *metric, char *firstspace)
{
	size_t curlen = 0;

	(void)router_route_intern(ret, &curlen, retsize,
			metric, firstspace, routes);

	return curlen;
}

/**
 * Prints for metric_path which rules and/or aggregations would be
 * triggered.  Useful for testing regular expressions.
 */
char
router_test_intern(char *metric, char *firstspace, route *routes)
{
	route *w;
	char gotmatch = 0;
	char newmetric[METRIC_BUFSIZ];
	char *newfirstspace = NULL;
	size_t len;
	regmatch_t pmatch[RE_MAX_MATCHES];

	for (w = routes; w != NULL; w = w->next) {
		if (w->dest->type == GROUP) {
			/* just recurse, in test mode performance shouldn't be an
			 * issue at all */
			gotmatch |= router_test_intern(
					metric,
					firstspace,
					w->dest->members.routes);
			if (gotmatch & 2)
				break;
		} else if (router_metric_matches(w, metric, firstspace, pmatch)) {
			gotmatch = 1;
			switch (w->dest->type) {
				case AGGREGATION:
					fprintf(stdout, "aggregation\n");
					break;
				case REWRITE:
					fprintf(stdout, "rewrite\n");
					break;
				default:
					fprintf(stdout, "match\n");
					break;
			}
			*firstspace = '\0';
			switch (w->matchtype) {
				case MATCHALL:
					fprintf(stdout, "    * -> %s\n", metric);
					break;
				case REGEX:
					fprintf(stdout, "    %s (regex) -> %s\n",
							w->pattern, metric);
					break;
				default: {
					char *x;
					switch (w->matchtype) {
						case CONTAINS:
							x = "strstr";
							break;
						case STARTS_WITH:
							x = "strncmp";
							break;
						case ENDS_WITH:
							x = "tailcmp";
							break;
						case MATCHES:
							x = "strcmp";
							break;
						default:
							x = "!impossible?";
							break;
					}
					fprintf(stdout, "    %s [%s: %s]\n    -> %s\n",
							w->pattern, x, w->strmatch, metric);
				}	break;
			}
			*firstspace = ' ';
			switch (w->dest->type) {
				case AGGREGATION: {
					struct _aggr_computes *ac;
					char newmetric[METRIC_BUFSIZ];
					char *newfirstspace = NULL;

					for (ac = w->dest->members.aggregation->computes; ac != NULL; ac = ac->next)
					{
						if (w->nmatch == 0 || (len = router_rewrite_metric(
										&newmetric, &newfirstspace,
										metric, firstspace,
										ac->metric,
										w->nmatch, pmatch)) == 0)
						{
							if (w->nmatch > 0) {
								fprintf(stderr, "router_test: failed to "
										"rewrite metric: newmetric size too "
										"small to hold replacement "
										"(%s -> %s)\n",
										metric, ac->metric);
								break;
							}
							snprintf(newmetric, sizeof(newmetric),
									"%s", ac->metric);
						}

						fprintf(stdout, "    %s%s%s%s -> %s\n",
								ac->type == SUM ? "sum" : ac->type == CNT ? "count" :
								ac->type == MAX ? "max" : ac->type == MIN ? "min" :
								ac->type == AVG ? "average" : "<unknown>",
								w->nmatch > 0 ? "(" : "",
								w->nmatch > 0 ? ac->metric : "",
								w->nmatch > 0 ? ")" : "",
								newmetric);
					}
				}	break;
				case BLACKHOLE: {
					fprintf(stdout, "    blackholed\n");
				}	break;
				case REWRITE: {
					/* rewrite metric name */
					if ((len = router_rewrite_metric(
								&newmetric, &newfirstspace,
								metric, firstspace,
								w->dest->members.replacement,
								w->nmatch, pmatch)) == 0)
					{
						fprintf(stderr, "router_test: failed to rewrite "
								"metric: newmetric size too small to hold "
								"replacement (%s -> %s)\n",
								metric, w->dest->members.replacement);
						break;
					};

					/* scary! write back the rewritten metric */
					memcpy(metric, newmetric, len);
					firstspace = metric + (newfirstspace - newmetric);
					*firstspace = '\0';
					fprintf(stdout, "    into(%s) -> %s\n",
							w->dest->members.replacement, metric);
					*firstspace = ' ';
				}	break;
				default: {
					fprintf(stdout, "    cluster(%s)\n", w->dest->name);
				}	break;
			}
			if (w->stop) {
				gotmatch = 3;
				fprintf(stdout, "    stop\n");
				break;
			}
		}
	}

	return gotmatch;
}

void
router_test(char *metric)
{
	char *firstspace;

	for (firstspace = metric; *firstspace != '\0'; firstspace++)
		if (*firstspace == ' ')
			break;
	if (!router_test_intern(metric, firstspace, routes)) {
		*firstspace = '\0';
		fprintf(stdout, "nothing matched %s\n", metric);
	}
	fflush(stdout);
}

void
router_shutdown(void)
{
	keep_running = 0;
}
