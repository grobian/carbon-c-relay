/*
 * Copyright 2013-2016 Fabian Groffen
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
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <errno.h>
#include <assert.h>

#include "fnv1a.h"
#include "consistent-hash.h"
#include "server.h"
#include "queue.h"
#include "aggregator.h"
#include "relay.h"
#include "router.h"

enum clusttype {
	BLACKHOLE,  /* /dev/null-like destination */
	GROUP,      /* pseudo type to create a matching tree */
	AGGRSTUB,   /* pseudo type to have stub matches for aggregation returns */
	STATSTUB,   /* pseudo type to have stub matches for collector returns */
	FORWARD,
	FILELOG,    /* like forward, write metric to file */
	FILELOGIP,  /* like forward, write ip metric to file */
	CARBON_CH,  /* original carbon-relay.py consistent-hash */
	FNV1A_CH,   /* FNV1a-based consistent-hash */
	JUMP_CH,    /* jump consistent hash with fnv1a input */
	ANYOF,      /* FNV1a-based hash, but with backup by others */
	FAILOVER,   /* ordered attempt delivery list */
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

typedef struct _destinations {
	cluster *cl;
	struct _destinations *next;
} destinations;

typedef struct _route {
	char *pattern;    /* original regex input, used for printing only */
	regex_t rule;     /* regex on metric, only if type == REGEX */
	size_t nmatch;    /* number of match groups */
	char *strmatch;   /* string to search for if type not REGEX or MATCHALL */
	destinations *dests; /* where matches should go */
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

struct _router {
	cluster *clusters;
	route *routes;
	aggregator *aggregators;
	char *collector_stub;
};

/* custom constant, meant to force regex mode matching */
#define REG_FORCE   01000000

/* catch string used for aggrgator destinations */
#define STUB_AGGR   "_aggregator_stub_"
/* catch string used for collector output */
#define STUB_STATS  "_collector_stub_"

/**
 * Frees routes and clusters.
 */
static void
router_free_intern(cluster *clusters, route *routes)
{
	cluster *c;
	route *r;
	servers *s;
	destinations *d;

	while (routes != NULL) {
		if (routes->pattern)
			free(routes->pattern);
		if (routes->strmatch)
			free(routes->strmatch);
		if (routes->matchtype == REGEX)
			regfree(&routes->rule);

		if (routes->next == NULL || routes->next->dests != routes->dests) {
			while (routes->dests != NULL) {
				if (routes->dests->cl->type == GROUP ||
						routes->dests->cl->type == AGGRSTUB ||
						routes->dests->cl->type == STATSTUB)
					router_free_intern(NULL, routes->dests->cl->members.routes);

				/* avoid freeing pointer also in use by stub */
				if (routes->dests->cl->type == AGGREGATION) {
					d = NULL;
				} else {
					d = routes->dests->next;
				}
				free(routes->dests);
				routes->dests = d;
			}
		}

		r = routes->next;
		free(routes);
		routes = r;
	}

	while (clusters != NULL) {
		switch (clusters->type) {
			case CARBON_CH:
			case FNV1A_CH:
			case JUMP_CH:
				assert(clusters->members.ch != NULL);
				ch_free(clusters->members.ch->ring);
				while (clusters->members.ch->servers) {
					s = clusters->members.ch->servers->next;
					server_shutdown(clusters->members.ch->servers->server);
					server_free(clusters->members.ch->servers->server);
					free(clusters->members.ch->servers);
					clusters->members.ch->servers = s;
				}
				free(clusters->members.ch);
				break;
			case FORWARD:
			case FILELOG:
			case FILELOGIP:
			case BLACKHOLE:
				while (clusters->members.forward) {
					server_shutdown(clusters->members.forward->server);
					server_free(clusters->members.forward->server);

					s = clusters->members.forward->next;
					free(clusters->members.forward);
					clusters->members.forward = s;
				}
				break;
			case ANYOF:
			case FAILOVER:
				/* in case of secondaries, make sure nothing references
				 * the servers anymore */
				for (s = clusters->members.anyof->list; s != NULL; s = s->next)
					server_shutdown(s->server);
				while (clusters->members.anyof->list) {
					server_free(clusters->members.anyof->list->server);

					s = clusters->members.anyof->list->next;
					free(clusters->members.anyof->list);
					clusters->members.anyof->list = s;
				}
				free(clusters->members.anyof->servers);
				free(clusters->members.anyof);
				break;
			case GROUP:
			case AGGRSTUB:
			case STATSTUB:
				/* handled at the routes above */
				break;
			case AGGREGATION:
				/* aggregators starve when they get no more input */
				break;
			case REWRITE:
				if (clusters->members.replacement)
					free(clusters->members.replacement);
				break;
		}
		if (clusters->name)
			free(clusters->name);

		c = clusters->next;
		free(clusters);
		clusters = c;
	}
}

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
				logerr("determine_if_regex: too many match groups, "
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
 *     (forward | any_of [useall] | failover | (carbon|fnv1a|jump_fnv1a)_ch [replication (count)])
 *         (ip:port[=instance] [proto (tcp | udp)] ...)
 *     ;
 * cluster (name)
 *     file [ip]
 *         (/path/to/file ...)
 *     ;
 * match
 *         (* | regex[ regex ...])
 *     send to (cluster ... | blackhole)
 *     [stop]
 *     ;
 * rewrite (regex)
 *     into (replacement)
 *     ;
 * aggregate
 *         (regex[ regex ...])
 *     every (interval) seconds
 *     expire after (expiration) seconds
 *     [timestamp at (start | middle | end) of bucket]
 *     compute (sum | count | max | min | average |
 *              median | percentile<%> | variance | stddev) write to
 *         (metric)
 *     [compute ... write to ...]
 *     [send to (cluster ...)]
 *     [stop]
 *     ;
 * send statistics to (cluster ...) [stop];
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
router *
router_readconfig(const char *path, size_t queuesize, size_t batchsize,
		unsigned short iotimeout)
{
	FILE *cnf;
	char *buf;
	size_t len = 0;
	char *p;
	cluster *cl;
	cluster *topcl;
	struct stat st;
	route *r = NULL;
	route *topr = NULL;
	aggregator *a = NULL;
	aggregator *topa = NULL;
	struct addrinfo *saddrs;
	char matchcatchallfound = 0;
	router *ret = NULL;

	if (stat(path, &st) == -1)
		return NULL;
	if ((buf = malloc(st.st_size + 1)) == NULL)
		return NULL;
	if ((cnf = fopen(path, "r")) == NULL)
		return NULL;
	while ((len = fread(buf + len, 1, st.st_size - len, cnf)) != 0)
		;
	buf[st.st_size] = '\0';
	fclose(cnf);

	if ((ret = malloc(sizeof(router))) == NULL)
		return NULL;
	ret->collector_stub = NULL;

	/* create virtual blackhole cluster */
	cl = malloc(sizeof(cluster));
	cl->name = strdup("blackhole");
	cl->type = BLACKHOLE;
	cl->members.forward = NULL;
	cl->next = NULL;
	topcl = cl;

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
			char useall = 0;
			p += 8;
			for (; *p != '\0' && isspace(*p); p++)
				;
			name = p;
			for (; *p != '\0' && !isspace(*p); p++)
				;
			if (*p == '\0') {
				logerr("unexpected end of file after 'cluster'\n");
				free(buf);
				free(ret);
				return NULL;
			}
			*p++ = '\0';
			for (; *p != '\0' && isspace(*p); p++)
				;
			if (
					(strncmp(p, "carbon_ch", 9) == 0 && isspace(*(p + 9))) ||
					(strncmp(p, "fnv1a_ch", 8) == 0 && isspace(*(p + 8))) ||
					(strncmp(p, "jump_fnv1a_ch", 13) == 0 && isspace(*(p + 13)))
			   )
			{
				int replcnt = 1;
				enum clusttype chtype =
					*p == 'c' ? CARBON_CH : *p == 'f' ? FNV1A_CH : JUMP_CH;

				p += chtype == CARBON_CH ? 10 : chtype == FNV1A_CH ? 9 : 14;
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
						logerr("unexpected end of file after "
								"'replication %s' for cluster %s\n",
								repl, name);
						free(buf);
						free(ret);
						return NULL;
					}
					*p++ = '\0';
					if ((replcnt = atoi(repl)) == 0)
						replcnt = 1;
				}

				if ((cl = cl->next = malloc(sizeof(cluster))) == NULL) {
					logerr("malloc failed in cluster %s\n", name);
					free(buf);
					free(ret);
					return NULL;
				}
				cl->type = chtype;
				cl->members.ch = malloc(sizeof(chashring));
				cl->members.ch->repl_factor = (unsigned char)replcnt;
				cl->members.ch->ring =
					ch_new(chtype == CARBON_CH ? CARBON :
							chtype == FNV1A_CH ? FNV1a : JUMP_FNV1a);
			} else if (strncmp(p, "forward", 7) == 0 && isspace(*(p + 7))) {
				p += 8;

				if ((cl = cl->next = malloc(sizeof(cluster))) == NULL) {
					logerr("malloc failed in cluster forward\n");
					free(buf);
					free(ret);
					return NULL;
				}
				cl->type = FORWARD;
				cl->members.forward = NULL;
			} else if (strncmp(p, "any_of", 6) == 0 && isspace(*(p + 6))) {
				p += 7;

				for (; *p != '\0' && isspace(*p); p++)
					;
				if (strncmp(p, "useall", 6) == 0 && isspace(*(p + 6))) {
					p += 7;
					useall = 1;
				}

				if ((cl = cl->next = malloc(sizeof(cluster))) == NULL) {
					logerr("malloc failed in cluster any_of\n");
					free(buf);
					free(ret);
					return NULL;
				}
				cl->type = ANYOF;
				cl->members.anyof = NULL;
			} else if (strncmp(p, "failover", 8) == 0 && isspace(*(p + 8))) {
				p += 9;

				for (; *p != '\0' && isspace(*p); p++)
					;
				if ((cl = cl->next = malloc(sizeof(cluster))) == NULL) {
					logerr("malloc failed in cluster failover\n");
					free(buf);
					free(ret);
					return NULL;
				}
				cl->type = FAILOVER;
				cl->members.anyof = NULL;
			} else if (strncmp(p, "file", 4) == 0 && isspace(*(p + 4))) {
				p += 5;

				for (; *p != '\0' && isspace(*p); p++)
					;
				if ((cl = cl->next = malloc(sizeof(cluster))) == NULL) {
					logerr("malloc failed in cluster file\n");
					free(buf);
					free(ret);
					return NULL;
				}
				if (strncmp(p, "ip", 2) == 0 && isspace(*(p + 2))) {
					p += 3;
					for (; *p != '\0' && isspace(*p); p++)
						;
					cl->type = FILELOGIP;
				} else {
					cl->type = FILELOG;
				}
				cl->members.forward = NULL;
			} else {
				char *type = p;
				for (; *p != '\0' && !isspace(*p); p++)
					;
				*p = 0;
				logerr("unknown cluster type '%s' for cluster %s\n",
						type, name);
				free(buf);
				free(ret);
				return NULL;
			}

			/* parse ips */
			for (; *p != '\0' && isspace(*p); p++)
				;
			w = NULL;
			do {
				char termchr;
				char *lastcolon = NULL;
				char *ip = p;
				char *inst = NULL;
				char *proto = "tcp";
				int port = 2003;
				server *newserver = NULL;
				struct addrinfo hint;
				char sport[8];
				int err;
				struct addrinfo *walk = NULL;
				struct addrinfo *next = NULL;
				char hnbuf[256];

				for (; *p != '\0' && !isspace(*p) && *p != ';'; p++) {
					if (*p == ':' && inst == NULL)
						lastcolon = p;
					if (*p == '=' && inst == NULL)
						inst = p;
				}

				if (*p == '\0') {
					logerr("unexpected end of file at '%s' "
							"for cluster %s\n", ip, name);
					free(cl);
					free(buf);
					free(ret);
					return NULL;
				}

				termchr = *p;
				*p = '\0';

				if (cl->type == CARBON_CH ||
						cl->type == FNV1A_CH ||
						cl->type == JUMP_CH)
				{
					if (inst != NULL) {
						*inst = '\0';
						p = inst++;
					}
					if (inst == ip)
						inst = NULL;
				}

				if (*(p - 1) == ']')
					lastcolon = NULL;
				if (lastcolon != NULL) {
					char *endp = NULL;
					*lastcolon = '\0';
					port = (int)strtol(lastcolon + 1, &endp, 10);
					if (port == 0 || endp != p) {
						logerr("expected port, or unexpected data at "
								"'%s' for cluster %s\n", lastcolon + 1, name);
						free(cl);
						free(buf);
						free(ret);
						return NULL;
					}
				}
				if (*ip == '[') {
					ip++;
					if (lastcolon != NULL && *(lastcolon - 1) == ']') {
						*(lastcolon - 1) = '\0';
					} else if (lastcolon == NULL && *(p - 1) == ']') {
						*(p - 1) = '\0';
					} else {
						logerr("expected ']' at '%s' "
								"for cluster %s\n", ip, name);
						free(cl);
						free(buf);
						free(ret);
						return NULL;
					}
				}

				if (inst != NULL)
					p = inst + strlen(inst);

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
							logerr("expected 'udp' or 'tcp' after "
									"'proto' at '%s' for cluster %s\n",
									proto, name);
							free(cl);
							free(buf);
							free(ret);
							return NULL;
						}
					} else {
						termchr = *p;
					}
				}

				if (cl->type != FILELOG && cl->type != FILELOGIP) {
					/* resolve host/IP */
					memset(&hint, 0, sizeof(hint));

					hint.ai_family = PF_UNSPEC;
					hint.ai_socktype = *proto == 'u' ? SOCK_DGRAM : SOCK_STREAM;
					hint.ai_protocol = *proto == 'u' ? IPPROTO_UDP : IPPROTO_TCP;
					hint.ai_flags = AI_NUMERICSERV;
					snprintf(sport, sizeof(sport), "%u", port);  /* for default */

					if ((err = getaddrinfo(ip, sport, &hint, &saddrs)) != 0) {
						logerr("failed to resolve server %s:%s (%s) "
								"for cluster %s: %s\n",
								ip, sport, proto, name, gai_strerror(err));
						free(cl);
						free(buf);
						free(ret);
						return NULL;
					}

					if (!useall && saddrs->ai_next != NULL) {
						/* take first result only */
						freeaddrinfo(saddrs->ai_next);
						saddrs->ai_next = NULL;
					}
				} else {
					/* TODO: try to create/append to file */

					proto = "file";
					saddrs = (void *)1;
				}

				walk = saddrs;
				while (walk != NULL) {
					/* disconnect from the rest to avoid double
					 * frees by freeaddrinfo() in server_destroy() */
					if (walk != (void *)1) {
						next = walk->ai_next;
						walk->ai_next = NULL;
					}

					if (useall) {
						/* unfold whatever we resolved, for human
						 * readability issues */
						if (walk->ai_family == AF_INET) {
							if (inet_ntop(walk->ai_family,
									&((struct sockaddr_in *)walk->ai_addr)->sin_addr,
									hnbuf, sizeof(hnbuf)) != NULL)
								ip = hnbuf;
						} else if (walk->ai_family == AF_INET6) {
							if (inet_ntop(walk->ai_family,
									&((struct sockaddr_in6 *)walk->ai_addr)->sin6_addr,
									hnbuf + 1, sizeof(hnbuf) - 2) != NULL)
							{
								hnbuf[0] = '[';
								/* space is reserved above */
								strcat(hnbuf, "]");
								ip = hnbuf;
							}
						}
					}

					newserver = server_new(ip, (unsigned short)port,
							*proto == 'f' ? CON_FILE :
							*proto == 'u' ? CON_UDP : CON_TCP,
							walk == (void *)1 ? NULL : walk,
							queuesize, batchsize, iotimeout);
					if (newserver == NULL) {
						logerr("failed to add server %s:%d (%s) "
								"to cluster %s: %s\n", ip, port, proto,
								name, strerror(errno));
						free(cl);
						free(buf);
						free(ret);
						return NULL;
					}

					if (cl->type == CARBON_CH ||
							cl->type == FNV1A_CH ||
							cl->type == JUMP_CH)
					{
						if (w == NULL) {
							cl->members.ch->servers = w =
								malloc(sizeof(servers));
						} else {
							w = w->next = malloc(sizeof(servers));
						}
						if (w == NULL) {
							logerr("malloc failed for %s_ch %s\n",
									cl->type == CARBON_CH ? "carbon" :
									cl->type == FNV1A_CH ? "fnv1a" :
									"jump_fnv1a", ip);
							free(cl);
							free(buf);
							free(ret);
							return NULL;
						}
						w->next = NULL;
						if (inst != NULL)
							server_set_instance(newserver, inst);
						w->server = newserver;
						cl->members.ch->ring = ch_addnode(
								cl->members.ch->ring,
								w->server);
						if (cl->members.ch->ring == NULL) {
							logerr("failed to add server %s:%d "
									"to ring for cluster %s: out of memory\n",
									ip, port, name);
							free(cl);
							free(buf);
							free(ret);
							return NULL;
						}
					} else if (cl->type == FORWARD ||
							cl->type == ANYOF ||
							cl->type == FAILOVER ||
							cl->type == FILELOG ||
							cl->type == FILELOGIP)
					{
						if (w == NULL) {
							w = malloc(sizeof(servers));
						} else {
							w = w->next = malloc(sizeof(servers));
						}
						if (w == NULL) {
							logerr("malloc failed for %s %s\n",
									cl->type == FORWARD ? "forward" :
									cl->type == ANYOF ? "any_of" :
									cl->type == FAILOVER ? "failover" :
									"file",
									ip);
							free(cl);
							free(buf);
							free(ret);
							return NULL;
						}
						w->next = NULL;
						w->server = newserver;
						if ((cl->type == FORWARD ||
							 cl->type == FILELOG || cl->type == FILELOGIP)
									&& cl->members.forward == NULL)
							cl->members.forward = w;
						if (cl->type == ANYOF || cl->type == FAILOVER) {
							if (cl->members.anyof == NULL) {
								cl->members.anyof = malloc(sizeof(serverlist));
								cl->members.anyof->count = 1;
								cl->members.anyof->servers = NULL;
								cl->members.anyof->list = w;
							} else {
								cl->members.anyof->count++;
							}
						}
					}

					walk = next;
				}

				*p = termchr;
				for (; *p != '\0' && isspace(*p); p++)
					;
			} while (*p != ';');
			p++; /* skip over ';' */
			if (cl->type == ANYOF || cl->type == FAILOVER) {
				size_t i = 0;
				cl->members.anyof->servers =
					malloc(sizeof(server *) * cl->members.anyof->count);
				for (w = cl->members.anyof->list; w != NULL; w = w->next)
					cl->members.anyof->servers[i++] = w->server;
				for (w = cl->members.anyof->list; w != NULL; w = w->next) {
					server_add_secondaries(w->server,
							cl->members.anyof->servers,
							cl->members.anyof->count);
					if (cl->type == FAILOVER)
						server_set_failover(w->server);
				}
			} else if (cl->type == CARBON_CH ||
					cl->type == FNV1A_CH ||
					cl->type == JUMP_CH)
			{
				/* check that replication count is actually <= the
				 * number of servers */
				size_t i = 0;
				for (w = cl->members.ch->servers; w != NULL; w = w->next)
					i++;
				if (i < cl->members.ch->repl_factor) {
					logerr("invalid cluster '%s': replication count (%zu) is "
							"larger than the number of servers (%zu)\n",
							name, cl->members.ch->repl_factor, i);
					free(cl);
					free(buf);
					free(ret);
					return NULL;
				}
			}
			cl->name = strdup(name);
			cl->next = NULL;
		} else if (strncmp(p, "match", 5) == 0 && isspace(*(p + 5))) {
			/* match rule */
			char *pat;
			char *dest;
			char stop = -1;
			cluster *w;
			route *m = NULL;
			destinations *d = NULL;
			destinations *dw = NULL;

#define FREE_R \
			{ \
				route *lm; \
				do { \
					lm = m->next; \
					free(m); \
				} while (m != r && (m = lm) != NULL); \
			}
#define FREE_D \
			{ \
				destinations *ld; \
				while (dw != NULL) { \
					ld = dw->next; \
					free(dw); \
					dw = ld; \
				} \
			}

			p += 6;
			for (; *p != '\0' && isspace(*p); p++)
				;

			do {
				pat = p;
				for (; *p != '\0' && !isspace(*p); p++)
					;
				if (*p == '\0') {
					logerr("unexpected end of file after 'match'\n");
					FREE_R;
					free(buf);
					free(ret);
					return NULL;
				}
				*p++ = '\0';
				for (; *p != '\0' && isspace(*p); p++)
					;

				if (r == NULL) {
					topr = r = malloc(sizeof(route));
				} else {
					r = r->next = malloc(sizeof(route));
				}
				if (m == NULL)
					m = r;
				if (strcmp(pat, "*") == 0) {
					r->pattern = NULL;
					r->strmatch = NULL;
					r->matchtype = MATCHALL;
				} else {
					int err = determine_if_regex(r, pat,
							REG_EXTENDED | REG_NOSUB);
					if (err != 0) {
						char ebuf[512];
						regerror(err, &r->rule, ebuf, sizeof(ebuf));
						logerr("invalid expression '%s' for match: %s\n",
								pat, ebuf);
						FREE_R;
						free(buf);
						free(ret);
						return NULL;
					}
				}
				r->next = NULL;
			} while (strncmp(p, "send", 4) != 0 || !isspace(*(p + 4)));
			p += 5;
			for (; *p != '\0' && isspace(*p); p++)
				;
			if (strncmp(p, "to", 2) != 0 || !isspace(*(p + 2))) {
				logerr("expected 'send to' after match %s\n", pat);
				FREE_R;
				free(buf);
				free(ret);
				return NULL;
			}
			p += 3;
			for (; *p != '\0' && isspace(*p); p++)
				;
			do {
				char save;
				dest = p;
				for (; *p != '\0' && !isspace(*p) && *p != ';'; p++)
					;
				if (*p == '\0')
					break;
				save = *p;
				*p = '\0';

				/* lookup dest */
				for (w = topcl; w != NULL; w = w->next) {
					if (w->type != GROUP &&
							w->type != AGGRSTUB &&
							w->type != STATSTUB &&
							w->type != AGGREGATION &&
							w->type != REWRITE &&
							strcmp(w->name, dest) == 0)
						break;
				}
				if (w == NULL) {
					logerr("no such cluster '%s' for 'match %s'\n", dest, pat);
					FREE_R;
					FREE_D;
					free(buf);
					free(ret);
					return NULL;
				}

				if (dw == NULL) {
					dw = d = malloc(sizeof(destinations));
				} else {
					d = d->next = malloc(sizeof(destinations));
				}
				if (d == NULL) {
					logerr("out of memory allocating new destination '%s' "
							"for 'match %s'\n", dest, pat);
					FREE_R;
					FREE_D;
					free(buf);
					free(ret);
					return NULL;
				}
				d->cl = w;
				d->next = NULL;

				*p = save;
				for (; *p != '\0' && isspace(*p); p++)
					;
			} while (*p != ';' &&
					!(strncmp(p, "stop", 4) == 0 &&
						(isspace(*(p + 4)) || *(p + 4) == ';')));

			if (*p == '\0') {
				logerr("unexpected end of file after 'send to %s'\n",
						dest);
				FREE_R;
				FREE_D;
				free(buf);
				free(ret);
				return NULL;
			} else if (*p == ';') {
				stop = 0;
			} else { /* due to strncmp above, this must be stop */
				p += 4;
				stop = 1;
			}

			/* find the closing ';' */
			for (; *p != '\0' && isspace(*p); p++)
				;
			if (*p != ';') {
				logerr("expected ';' after match %s\n", pat);
				FREE_R;
				FREE_D;
				free(buf);
				free(ret);
				return NULL;
			}
			p++;

			/* fill in the destinations for all the routes */
			do {
				m->dests = dw;
				m->stop = w->type == BLACKHOLE ? 1 : stop;
			} while (m != r && (m = m->next) != NULL);

			if (matchcatchallfound) {
				logerr("warning: match %s will never be matched "
						"due to preceding match * ... stop\n",
						r->pattern == NULL ? "*" : r->pattern);
			}
			if (r->matchtype == MATCHALL && r->stop)
				matchcatchallfound = 1;
		} else if (strncmp(p, "aggregate", 9) == 0 && isspace(*(p + 9))) {
			/* aggregation rule */
			char *type;
			char *pat;
			char *num;
			enum _aggr_timestamp tswhen = TS_END;
			cluster *w;
			int err;
			int intv;
			int exp;
			char stop = 0;
			route *m = NULL;
			destinations *d = NULL;
			destinations *dw = NULL;

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
					logerr("unexpected end of file after 'aggregate'\n");
					free(buf);
					free(ret);
					return NULL;
				}
				*p++ = '\0';
				for (; *p != '\0' && isspace(*p); p++)
					;

				if (r == NULL) {
					topr = r = malloc(sizeof(route));
				} else {
					r = r->next = malloc(sizeof(route));
				}
				if (m == NULL)
					m = r;
				err = determine_if_regex(r, pat, REG_EXTENDED);
				if (err != 0) {
					char ebuf[512];
					regerror(err, &r->rule, ebuf, sizeof(ebuf));
					logerr("invalid expression '%s' "
							"for aggregation: %s\n", pat, ebuf);
					FREE_R;
					free(w);
					free(buf);
					free(ret);
					return NULL;
				}
				r->next = NULL;
			} while (strncmp(p, "every", 5) != 0 || !isspace(*(p + 5)));
			p += 6;
			for (; *p != '\0' && isspace(*p); p++)
				;
			num = p;
			for (; *p != '\0' && isdigit(*p); p++)
				;
			if (*p == '\0') {
				logerr("unexpected end of file after 'every %s'\n", num);
				FREE_R;
				free(w);
				free(buf);
				free(ret);
				return NULL;
			}
			if (!isspace(*p)) {
				logerr("unexpected character '%c', "
						"expected number after 'every'\n", *p);
				FREE_R;
				free(w);
				free(buf);
				free(ret);
				return NULL;
			}
			*p++ = '\0';
			for (; *p != '\0' && isspace(*p); p++)
				;
			if (strncmp(p, "seconds", 7) != 0 || !isspace(*(p + 7))) {
				logerr("expected 'seconds' after 'every %s'\n", num);
				FREE_R;
				free(w);
				free(buf);
				free(ret);
				return NULL;
			}
			p += 8;
			intv = atoi(num);
			if (intv == 0) {
				logerr("interval must be non-zero\n");
				FREE_R;
				free(w);
				free(buf);
				free(ret);
				return NULL;
			}
			for (; *p != '\0' && isspace(*p); p++)
				;
			if (strncmp(p, "expire", 6) != 0 || !isspace(*(p + 6))) {
				logerr("expected 'expire after' after 'every %s seconds\n",
						num);
				FREE_R;
				free(w);
				free(buf);
				free(ret);
				return NULL;
			}
			p += 7;
			for (; *p != '\0' && isspace(*p); p++)
				;
			if (strncmp(p, "after", 5) != 0 || !isspace(*(p + 5))) {
				logerr("expected 'after' after 'expire'\n");
				FREE_R;
				free(w);
				free(buf);
				free(ret);
				return NULL;
			}
			p += 6;
			for (; *p != '\0' && isspace(*p); p++)
				;
			num = p;
			for (; *p != '\0' && isdigit(*p); p++)
				;
			if (*p == '\0') {
				logerr("unexpected end of file after 'expire after %s'\n",
						num);
				FREE_R;
				free(w);
				free(buf);
				free(ret);
				return NULL;
			}
			if (!isspace(*p)) {
				logerr("unexpected character '%c', "
						"expected number after 'expire after'\n", *p);
				FREE_R;
				free(w);
				free(buf);
				free(ret);
				return NULL;
			}
			*p++ = '\0';
			for (; *p != '\0' && isspace(*p); p++)
				;
			if (strncmp(p, "seconds", 7) != 0 || !isspace(*(p + 7))) {
				logerr("expected 'seconds' after 'expire after %s'\n", num);
				FREE_R;
				free(w);
				free(buf);
				free(ret);
				return NULL;
			}
			p += 8;
			exp = atoi(num);
			if (exp == 0) {
				logerr("expire must be non-zero\n");
				FREE_R;
				free(w);
				free(buf);
				free(ret);
				return NULL;
			}
			if (exp < intv) {
				logerr("expire (%d) must be greater than interval (%d)\n",
						exp, intv);
				FREE_R;
				free(w);
				free(buf);
				free(ret);
				return NULL;
			}
			for (; *p != '\0' && isspace(*p); p++)
				;

			/* optional timestamp bit */
			if (strncmp(p, "timestamp", 9) == 0 && isspace(*(p + 9))) {
				p += 10;
				for (; *p != '\0' && isspace(*p); p++)
					;
				if (strncmp(p, "at", 2) != 0 || !isspace(*(p + 2))) {
					logerr("expected 'at' after 'timestamp'\n");
					FREE_R;
					free(w);
					free(buf);
					free(ret);
					return NULL;
				}
				p += 3;
				for (; *p != '\0' && isspace(*p); p++)
					;
				if (strncmp(p, "start", 5) == 0 && isspace(*(p + 5))) {
					p += 6;
					tswhen = TS_START;
				} else if (strncmp(p, "middle", 6) == 0 && isspace(*(p + 6))) {
					p += 7;
					tswhen = TS_MIDDLE;
				} else if (strncmp(p, "end", 3) == 0 && isspace(*(p + 3))) {
					p += 4;
					tswhen = TS_END;
				} else {
					logerr("expected 'start', 'middle' or 'end' "
							"after 'timestamp at'\n");
					FREE_R;
					free(w);
					free(buf);
					free(ret);
					return NULL;
				}
				for (; *p != '\0' && isspace(*p); p++)
					;
				if (strncmp(p, "of", 2) != 0 || !isspace(*(p + 2))) {
					logerr("expected 'of' after 'timestamp at ...'\n");
					FREE_R;
					free(w);
					free(buf);
					free(ret);
					return NULL;
				}
				p += 3;
				for (; *p != '\0' && isspace(*p); p++)
					;
				if (strncmp(p, "bucket", 6) != 0 || !isspace(*(p + 6))) {
					logerr("expected 'bucket' after 'timestamp at ... of'\n");
					FREE_R;
					free(w);
					free(buf);
					free(ret);
					return NULL;
				}
				p += 7;
				for (; *p != '\0' && isspace(*p); p++)
					;
			}

			w->members.aggregation = aggregator_new(intv, exp, tswhen);
			if (w->members.aggregation == NULL) {
				logerr("out of memory while allocating new aggregator\n");
				FREE_R;
				free(w);
				free(buf);
				free(ret);
				return NULL;
			}
			if (a == NULL) {
				topa = a = w->members.aggregation;
			} else {
				a = a->next = w->members.aggregation;
			}
			do {
				if (strncmp(p, "compute", 7) != 0 || !isspace(*(p + 7))) {
					logerr("expected 'compute' at: %.20s\n", p);
					FREE_R;
					free(w);
					free(buf);
					free(ret);
					return NULL;
				}
				p += 8;
				pat = p;
				for (; *p != '\0' && !isspace(*p); p++)
					;
				if (*p == '\0') {
					logerr("unexpected end of file after 'compute'\n");
					FREE_R;
					free(w);
					free(buf);
					free(ret);
					return NULL;
				}
				*p++ = '\0';
				type = pat;
				for (; *p != '\0' && isspace(*p); p++)
					;
				if (strncmp(p, "write", 5) != 0 || !isspace(*(p + 5))) {
					logerr("expected 'write to' after 'compute %s'\n", pat);
					FREE_R;
					free(w);
					free(buf);
					free(ret);
					return NULL;
				}
				p += 6;
				for (; *p != '\0' && isspace(*p); p++)
					;
				if (strncmp(p, "to", 2) != 0 || !isspace(*(p + 2))) {
					logerr("expected 'write to' after 'compute %s'\n", pat);
					FREE_R;
					free(w);
					free(buf);
					free(ret);
					return NULL;
				}
				p += 3;
				for (; *p != '\0' && isspace(*p); p++)
					;
				pat = p;
				for (; *p != '\0' && !isspace(*p); p++)
					;
				if (*p == '\0') {
					logerr("unexpected end of file after 'write to'\n");
					FREE_R;
					free(w);
					free(buf);
					free(ret);
					return NULL;
				}
			 	*p++ = '\0';

				if (aggregator_add_compute(w->members.aggregation, pat, type) != 0) {
					logerr("expected sum, count, max, min, average, "
							"median, percentile<%>, variance or stddev "
							"after 'compute', got '%s'\n", type);
					FREE_R;
					free(w);
					free(buf);
					free(ret);
					return NULL;
				}

				for (; *p != '\0' && isspace(*p); p++)
					;
			} while (*p != ';' &&
					!(strncmp(p, "send", 4) == 0 && isspace(*(p + 4))) &&
					!(strncmp(p, "stop", 4) == 0 &&
						(isspace(*(p + 4)) || *(p + 4) == ';')));

			if (strncmp(p, "send", 4) == 0 && isspace(*(p + 4))) {
				cluster *cw;
				p += 5;
				for (; *p != '\0' && isspace(*p); p++)
					;
				if (strncmp(p, "to", 2) != 0 || !isspace(*(p + 2))) {
					logerr("expected 'to' after 'send' for aggregate\n");
					free(buf);
					free(ret);
					return NULL;
				}
				p += 3;
				for (; *p != '\0' && isspace(*p); p++)
					;
				do {
					char save;
					char *dest = p;
					for (; *p != '\0' && !isspace(*p) && *p != ';'; p++)
						;
					if (*p == '\0')
						break;
					save = *p;
					*p = '\0';

					/* lookup dest */
					for (cw = topcl; cw != NULL; cw = cw->next) {
						if (cw->type != GROUP &&
								cw->type != AGGRSTUB &&
								cw->type != STATSTUB &&
								cw->type != AGGREGATION &&
								cw->type != REWRITE &&
								strcmp(cw->name, dest) == 0)
							break;
					}
					if (cw == NULL) {
						logerr("no such cluster '%s' for aggregate\n", dest);
						FREE_D;
						FREE_R;
						free(w);
						free(buf);
						free(ret);
						return NULL;
					}

					if (dw == NULL) {
						dw = d = malloc(sizeof(destinations));
					} else {
						d = d->next = malloc(sizeof(destinations));
					}
					if (d == NULL) {
						logerr("out of memory allocating new destination '%s' "
								"for aggregation\n", dest);
						FREE_D;
						FREE_R;
						free(w);
						free(buf);
						free(ret);
						return NULL;
					}
					d->cl = cw;
					d->next = NULL;

					*p = save;
					for (; *p != '\0' && isspace(*p); p++)
						;
				} while (*p != ';' &&
						!(strncmp(p, "stop", 4) == 0 &&
							(isspace(*(p + 4)) || *(p + 4) == ';')));
			}

			if (strncmp(p, "stop", 4) == 0 &&
					(isspace(*(p + 4)) || *(p + 4) == ';'))
			{
				p += 4;
				stop = 1;
				for (; *p != '\0' && isspace(*p); p++)
					;
			}

			if (*p == '\0') {
				logerr("unexpected end of file after 'aggregate %s'\n",
						r->pattern);
				FREE_D;
				FREE_R;
				free(w);
				free(buf);
				free(ret);
				return NULL;
			}
			if (*p != ';') {
				logerr("expected ';' after aggregate %s\n", r->pattern);
				FREE_D;
				FREE_R;
				free(w);
				free(buf);
				free(ret);
				return NULL;
			}
			p++;

			/* add cluster to the list of clusters */
			cl = cl->next = w;

			/* fill in the destinations for all the routes, this is for
			 * printing and free-ing */
			do {
				m->dests = malloc(sizeof(destinations));
				m->dests->cl = w;
				m->dests->next = dw;
				m->stop = stop;
			} while (m != r && (m = m->next) != NULL);

			if (dw != NULL) {
				char stubname[48];

				m = malloc(sizeof(route));
				m->pattern = NULL;
				m->strmatch = NULL;
				m->dests = dw;
				m->stop = 1;
				m->matchtype = MATCHALL;
				m->next = NULL;

				/* inject stub route for dests */
				d = malloc(sizeof(destinations));
				cl = cl->next = d->cl = malloc(sizeof(cluster));
				cl->name = NULL;
				cl->type = AGGRSTUB;
				cl->members.routes = m;
				cl->next = NULL;
				d->next = NULL;

				snprintf(stubname, sizeof(stubname),
						STUB_AGGR "%p__", w->members.aggregation);
				m = malloc(sizeof(route));
				m->pattern = strdup(stubname);
				m->strmatch = strdup(stubname);
				m->dests = d;
				m->stop = 1;
				m->matchtype = STARTS_WITH;
				m->next = topr;
				/* enforce first match to avoid interference */
				topr = m;

				aggregator_set_stub(w->members.aggregation, stubname);
			}

			if (matchcatchallfound) {
				logerr("warning: aggregate %s will never be matched "
						"due to preceeding match * ... stop\n",
						r->pattern);
			}
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
				logerr("unexpected end of file after 'rewrite'\n");
				free(buf);
				free(ret);
				return NULL;
			}
			*p++ = '\0';
			for (; *p != '\0' && isspace(*p); p++)
				;
			if (strncmp(p, "into", 4) != 0 || !isspace(*(p + 4))) {
				logerr("expected 'into' after rewrite %s\n", pat);
				free(buf);
				free(ret);
				return NULL;
			}
			p += 5;
			for (; *p != '\0' && isspace(*p); p++)
				;
			replacement = p;
			for (; *p != '\0' && !isspace(*p) && *p != ';'; p++)
				;
			if (*p == '\0') {
				logerr("unexpected end of file after 'into %s'\n",
						replacement);
				free(buf);
				free(ret);
				return NULL;
			} else if (*p == ';') {
				*p++ = '\0';
			} else {
				*p++ = '\0';
				for (; *p != '\0' && isspace(*p) && *p != ';'; p++)
					;
				if (*p != ';') {
					logerr("expected ';' after %s\n", replacement);
					free(buf);
					free(ret);
					return NULL;
				}
				p++;
			}

			if (r == NULL) {
				topr = r = malloc(sizeof(route));
			} else {
				r = r->next = malloc(sizeof(route));
			}
			err = determine_if_regex(r, pat, REG_EXTENDED | REG_FORCE);
			if (err != 0) {
				char ebuf[512];
				regerror(err, &r->rule, ebuf, sizeof(ebuf));
				logerr("invalid expression '%s' for rewrite: %s\n", pat, ebuf);
				free(r);
				free(buf);
				free(ret);
				return NULL;
			}

			if ((cl = cl->next = malloc(sizeof(cluster))) == NULL) {
				logerr("malloc failed for rewrite destination\n");
				free(r);
				free(buf);
				free(ret);
				return NULL;
			}
			cl->type = REWRITE;
			cl->name = NULL;
			cl->members.replacement = strdup(replacement);
			cl->next = NULL;

			r->dests = malloc(sizeof(destinations));
			r->dests->cl = cl;
			r->dests->next = NULL;
			r->stop = 0;
			r->next = NULL;

			if (matchcatchallfound) {
				logerr("warning: rewrite %s will never be matched "
						"due to preceeding match * ... stop\n",
						r->pattern == NULL ? "*" : r->pattern);
			}
		} else if (strncmp(p, "send", 4) == 0 && isspace(*(p + 4))) {
			cluster *cw;
			destinations *d = NULL;
			destinations *dw = NULL;
			route *m;
			char stubname[48];

			p += 4;
			for (; *p != '\0' && isspace(*p); p++)
				;
			if (strncmp(p, "statistics", 10) != 0 || !isspace(*(p + 10))) {
				logerr("expected 'statistics' after 'send'\n");
				free(buf);
				router_free_intern(topcl, topr);
				free(ret);
				return NULL;
			}
			p += 10;
			for (; *p != '\0' && isspace(*p); p++)
				;
			if (strncmp(p, "to", 2) != 0 || !isspace(*(p + 2))) {
				logerr("expected 'to' after 'send statistics'\n");
				free(buf);
				router_free_intern(topcl, topr);
				free(ret);
				return NULL;
			}
			p += 2;
			if (ret->collector_stub != NULL) {
				logerr("duplicate 'send statistics to' not allowed, "
						"use multiple destinations instead\n");
				free(buf);
				router_free_intern(topcl, topr);
				free(ret);
				return NULL;
			}
			for (; *p != '\0' && isspace(*p); p++)
				;
			do {
				char save;
				char *dest = p;
				for (; *p != '\0' && !isspace(*p) && *p != ';'; p++)
					;
				if (*p == '\0' || *p == ';')
					break;
				save = *p;
				*p = '\0';

				/* lookup dest */
				for (cw = topcl; cw != NULL; cw = cw->next) {
					if (cw->type != GROUP &&
							cw->type != AGGRSTUB &&
							cw->type != STATSTUB &&
							cw->type != AGGREGATION &&
							cw->type != REWRITE &&
							strcmp(cw->name, dest) == 0)
						break;
				}
				if (cw == NULL) {
					logerr("no such cluster '%s' for send statistics to\n",
							dest);
					FREE_D;
					free(buf);
					free(ret);
					return NULL;
				}

				if (dw == NULL) {
					dw = d = malloc(sizeof(destinations));
				} else {
					d = d->next = malloc(sizeof(destinations));
				}
				if (d == NULL) {
					logerr("out of memory allocating new destination '%s' "
							"for 'send statistics to'\n", dest);
					FREE_D;
					free(buf);
					free(ret);
					return NULL;
				}
				d->cl = cw;
				d->next = NULL;

				*p = save;
				for (; *p != '\0' && isspace(*p); p++)
					;
			} while (*p != ';' &&
					!(strncmp(p, "stop", 4) == 0 &&
						(isspace(*(p + 4)) || *(p + 4) == ';')));
			if (strncmp(p, "stop", 4) == 0 &&
					(isspace(*(p + 4)) || *(p + 4) == ';'))
			{
				p += 4;
				for (; *p != '\0' && isspace(*p); p++)
					;
			}
			if (*p == ';')
				p++;

			if (dw == NULL) {
				logerr("expected cluster name after 'send statistics to'\n");
				FREE_D;
				free(buf);
				free(ret);
				return NULL;
			}

			/* create a top-rule with match for unique prefix and send
			 * that to the defined targets */
			m = malloc(sizeof(route));
			m->pattern = NULL;
			m->strmatch = NULL;
			m->dests = dw;
			m->stop = 1;
			m->matchtype = MATCHALL;
			m->next = NULL;

			/* inject stub route for dests */
			d = malloc(sizeof(destinations));
			cl = cl->next = d->cl = malloc(sizeof(cluster));
			cl->name = NULL;
			cl->type = STATSTUB;
			cl->members.routes = m;
			cl->next = NULL;
			d->next = NULL;

			snprintf(stubname, sizeof(stubname),
					STUB_STATS "%p__", ret);
			m = malloc(sizeof(route));
			m->pattern = strdup(stubname);
			m->strmatch = strdup(stubname);
			m->dests = d;
			m->stop = 1;
			m->matchtype = STARTS_WITH;
			m->next = topr;
			/* enforce first match to avoid interference */
			topr = m;
			if (r == NULL)
				r = topr;

			ret->collector_stub = m->pattern;
		} else {
			/* garbage? */
			logerr("garbage in config: %s\n", p);
			free(buf);
			router_free_intern(topcl, topr);
			free(ret);
			return NULL;
		}
	} while (*p != '\0');

	free(buf);

	ret->clusters = topcl;
	ret->routes = topr;
	ret->aggregators = topa;
	return ret;
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
router_optimise(router *r)
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
	for (rwalk = r->routes; rwalk != NULL && seq < 50; rwalk = rwalk->next)
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
	for (rwalk = r->routes; rwalk != NULL; rwalk = rnext) {
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

	rwalk = r->routes = NULL;
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
			if (r->routes == NULL) {
				r->routes = bwalk->firstroute;
			} else {
				rwalk->next = bwalk->firstroute;
			}
			rwalk = bwalk->lastroute;
			blast = bwalk->next;
			free(bwalk->pattern);
			free(bwalk);
		} else {
			if (r->routes == NULL) {
				rwalk = r->routes = malloc(sizeof(route));
			} else {
				rwalk = rwalk->next = malloc(sizeof(route));
			}
			rwalk->pattern = NULL;
			rwalk->stop = 0;
			rwalk->matchtype = CONTAINS;
			rwalk->dests = malloc(sizeof(destinations));
			rwalk->dests->cl = malloc(sizeof(cluster));
			rwalk->dests->cl->name = bwalk->pattern;
			rwalk->dests->cl->type = GROUP;
			rwalk->dests->cl->members.routes = bwalk->firstroute;
			rwalk->dests->cl->next = NULL;
			rwalk->dests->next = NULL;
			rwalk->next = NULL;
			blast = bwalk->next;
			free(bwalk);
		}
		if (bwalk == bstart)
			bstart = NULL;
	}
}

/**
 * Returns all (unique) servers from the cluster-configuration.
 */
server **
router_getservers(router *r)
{
#define SERVSZ  511
	server **ret = malloc(sizeof(server *) * SERVSZ + 1);
	cluster *c;
	servers *s;
	int i;

	*ret = NULL;

#define add_server(X) { \
	for (i = 0; i < SERVSZ && ret[i] != NULL; i++) \
		if (ret[i] == X) \
			break; \
	if (i < SERVSZ && ret[i] == NULL) { \
		ret[i] = X; \
		ret[i + 1] = NULL; \
	} \
}

	for (c = r->clusters; c != NULL; c = c->next) {
		if (c->type == FORWARD ||
				c->type == FILELOG || c->type == FILELOGIP)
		{
			for (s = c->members.forward; s != NULL; s = s->next)
				add_server(s->server);
		} else if (c->type == ANYOF || c->type == FAILOVER) {
			for (s = c->members.anyof->list; s != NULL; s = s->next)
				add_server(s->server);
		} else if (c->type == CARBON_CH ||
				c->type == FNV1A_CH ||
				c->type == JUMP_CH)
		{
			for (s = c->members.ch->servers; s != NULL; s = s->next)
				add_server(s->server);
		}
	}

	return ret;
}

/**
 * Returns all aggregators from this router configuration
 */
inline aggregator *
router_getaggregators(router *rtr)
{
	return rtr->aggregators;
}

/**
 * Returns the stub prefix to use for collector stats, or NULL if no
 * such thing should be employed.
 */
inline char *
router_getcollectorstub(router *rtr)
{
	return rtr->collector_stub;
}

/**
 * Mere debugging function to check if the configuration is picked up
 * alright.  If all is set to false, aggregation rules won't be printed.
 * This comes in handy because aggregations usually come in the order of
 * thousands.
 */
void
router_printconfig(router *rtr, FILE *f, char pmode)
{
	cluster *c;
	route *r;
	servers *s;

#define PPROTO \
	server_ctype(s->server) == CON_UDP ? " proto udp" : ""

	for (c = rtr->clusters; c != NULL; c = c->next) {
		if (c->type == BLACKHOLE || c->type == REWRITE ||
				c->type == GROUP || c->type == AGGREGATION ||
				c->type == AGGRSTUB || c->type == STATSTUB)
			continue;
		fprintf(f, "cluster %s\n", c->name);
		if (c->type == FORWARD) {
			fprintf(f, "    forward\n");
			for (s = c->members.forward; s != NULL; s = s->next)
				fprintf(f, "        %s:%d%s\n",
						server_ip(s->server), server_port(s->server), PPROTO);
		} else if (c->type == FILELOG || c->type == FILELOGIP) {
			fprintf(f, "    file%s\n", c->type == FILELOGIP ? " ip" : "");
			for (s = c->members.forward; s != NULL; s = s->next)
				fprintf(f, "        %s\n",
						server_ip(s->server));
		} else if (c->type == ANYOF || c->type == FAILOVER) {
			fprintf(f, "    %s\n", c->type == ANYOF ? "any_of" : "failover");
			for (s = c->members.anyof->list; s != NULL; s = s->next)
				fprintf(f, "        %s:%d%s\n",
						server_ip(s->server), server_port(s->server), PPROTO);
		} else if (c->type == CARBON_CH ||
				c->type == FNV1A_CH ||
				c->type == JUMP_CH)
		{
			fprintf(f, "    %s_ch replication %d\n",
					c->type == CARBON_CH ? "carbon" : 
					c->type == FNV1A_CH ? "fnv1a" : "jump_fnv1a",
					c->members.ch->repl_factor);
			for (s = c->members.ch->servers; s != NULL; s = s->next)
				fprintf(f, "        %s:%d%s%s%s\n",
						server_ip(s->server), server_port(s->server),
						server_instance(s->server) ? "=" : "",
						server_instance(s->server) ? server_instance(s->server) : "",
						PPROTO);
		}
		fprintf(f, "    ;\n");
		if (pmode & 2) {
			if (c->type == CARBON_CH ||
					c->type == FNV1A_CH ||
					c->type == JUMP_CH)
			{
				fprintf(f, "# hash ring for %s follows\n", c->name);
				ch_printhashring(c->members.ch->ring, f);
			}
		}
	}
	fprintf(f, "\n");
	for (r = rtr->routes; r != NULL; r = r->next) {
		if (r->dests->cl->type == AGGREGATION) {
			cluster *aggr = r->dests->cl;
			struct _aggr_computes *ac;
			char stubname[48];
			char percentile[16];

			if (!(pmode & 1))
				continue;

			if (pmode & 2 || r->dests->next == NULL) {
				stubname[0] = '\0';
			} else {
				snprintf(stubname, sizeof(stubname),
						STUB_AGGR "%p__", aggr->members.aggregation);
			}

			fprintf(f, "aggregate");
			if (r->next == NULL || r->next->dests->cl != aggr) {
				fprintf(f, " %s\n", r->pattern);
			} else {
				fprintf(f, "\n");
				do {
					fprintf(f, "        %s\n", r->pattern);
				} while (r->next != NULL && r->next->dests->cl == aggr
						&& (r = r->next) != NULL);
			}
			fprintf(f, "    every %u seconds\n"
					"    expire after %u seconds\n"
					"    timestamp at %s of bucket\n",
					aggr->members.aggregation->interval,
					aggr->members.aggregation->expire,
					aggr->members.aggregation->tswhen == TS_START ? "start" :
					aggr->members.aggregation->tswhen == TS_MIDDLE ? "middle" :
					aggr->members.aggregation->tswhen == TS_END ? "end" :
					"<unknown>");
			for (ac = aggr->members.aggregation->computes; ac != NULL; ac = ac->next) {
				snprintf(percentile, sizeof(percentile),
						"percentile%d", ac->percentile);
				fprintf(f, "    compute %s write to\n"
						"        %s\n",
						ac->type == SUM ? "sum" : ac->type == CNT ? "count" :
						ac->type == MAX ? "max" : ac->type == MIN ? "min" :
						ac->type == AVG ? "average" : 
						ac->type == MEDN ? "median" :
						ac->type == PCTL ? percentile :
						ac->type == VAR ? "variance" :
						ac->type == SDEV ? "stddev" :
						"<unknown>",
						ac->metric + strlen(stubname));
			}
			if (!(pmode & 2) && r->dests->next != NULL) {
				destinations *dn = r->dests->next;
				fprintf(f, "    send to");
				if (dn->next == NULL) {
					fprintf(f, " %s\n", dn->cl->name);
				} else {
					for (; dn != NULL; dn = dn->next)
						fprintf(f, "\n        %s", dn->cl->name);
					fprintf(f, "\n");
				}
			}
			fprintf(f, "%s    ;\n", r->stop ? "    stop\n" : "");
		} else if (r->dests->cl->type == REWRITE) {
			fprintf(f, "rewrite %s\n    into %s\n    ;\n",
					r->pattern, r->dests->cl->members.replacement);
		} else if (r->dests->cl->type == GROUP) {
			size_t cnt = 0;
			route *rwalk;
			char blockname[64];
			char *b = &blockname[sizeof(blockname) - 1];
			char *p;

			for (rwalk = r->dests->cl->members.routes; rwalk != NULL; rwalk = rwalk->next)
				cnt++;
			/* reverse the name, to make it human consumable */
			*b-- ='\0';
			for (p = r->dests->cl->name; *p != '\0' && b > blockname; p++)
				*b-- = *p;
			fprintf(f, "# common pattern group '%s' "
					"contains %zu aggregations/matches\n",
					++b, cnt);
		} else if (r->dests->cl->type == AGGRSTUB ||
				r->dests->cl->type == STATSTUB)
		{
			if (pmode & 2) {
				fprintf(f, "# stub match for aggregate/statistics rule "
						"with send to\n");
				fprintf(f, "match ^%s\n    send to", r->pattern);
				if (r->dests->cl->members.routes->dests->next == NULL) {
					fprintf(f, " %s",
							r->dests->cl->members.routes->dests->cl->name);
				} else {
					destinations *d = r->dests->cl->members.routes->dests;
					for (; d != NULL; d = d->next)
						fprintf(f, "\n        %s", d->cl->name);
				}
				fprintf(f, "%s\n    ;\n", r->stop ? "\n    stop" : "");
			} else if (r->dests->cl->type == STATSTUB) {
				destinations *d = r->dests->cl->members.routes->dests;
				fprintf(f, "send statistics to");
				if (d->next == NULL) {
					fprintf(f, " %s", d->cl->name);
				} else {
					for ( ; d != NULL; d = d->next)
						fprintf(f, "\n        %s", d->cl->name);
				}
				fprintf(f, "\n    stop\n    ;\n");
			}
		} else {
			route *or = r;
			fprintf(f, "match");
			if (r->next == NULL || r->next->dests != or->dests) {
				fprintf(f, " %s\n",
						r->matchtype == MATCHALL ? "*" : r->pattern);
			} else {
				fprintf(f, "\n");
				do {
					fprintf(f, "        %s\n",
							r->matchtype == MATCHALL ? "*" : r->pattern);
				} while (r->next != NULL && r->next->dests == or->dests
						&& (r = r->next) != NULL);
			}
			fprintf(f, "    send to");
			if (or->dests->next == NULL) {
				fprintf(f, " %s", or->dests->cl->name);
			} else {
				destinations *d;
				for (d = or->dests; d != NULL; d = d->next)
					fprintf(f, "\n        %s", d->cl->name);
			}
			fprintf(f, "\n%s    ;\n", or->stop ? "    stop\n" : "");
		}
	}
	fflush(f);
}

/**
 * Free the routes and all associated resources.
 */
void
router_free(router *rtr)
{
	router_free_intern(rtr->clusters, rtr->routes);

	free(rtr);
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
	enum rewrite_case { RETAIN, LOWER, UPPER } rcase = RETAIN;

	assert(pmatch != NULL);

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
					rcase = RETAIN;
					break;
				}
				/* fall through so we handle \1\2 */
			default:
				if (escape == 1 && rcase == RETAIN && *p == '_') {
					rcase = LOWER;
				} else if (escape == 1 && rcase == RETAIN && *p == '^') {
					rcase = UPPER;
				} else if (escape && *p >= '0' && *p <= '9') {
					escape = 2;
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
							if (s - *newmetric + t - q < sizeof(*newmetric)) {
								switch (rcase) {
									case RETAIN:
										while (q < t)
											*s++ = *q++;
										break;
									case LOWER:
										while (q < t)
											*s++ = (char)tolower(*q++);
										break;
									case UPPER:
										while (q < t)
											*s++ = (char)toupper(*q++);
										break;
								}
							}
						}
						ref = 0;
					}
					if (*p != '\\') { /* \1\2 case */
						escape = 0;
						rcase = RETAIN;
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
		char *blackholed,
		destination ret[],
		size_t *curlen,
		size_t retsize,
		char *srcaddr,
		char *metric,
		char *firstspace,
		const route *r)
{
	const route *w;
	destinations *d;
	char stop = 0;
	char wassent = 0;
	const char *p;
	const char *q = NULL;  /* pacify compiler, won't happen in reality */
	const char *t;
	char newmetric[METRIC_BUFSIZ];
	char *newfirstspace = NULL;
	size_t len;
	regmatch_t pmatch[RE_MAX_MATCHES];

#define failif(RETLEN, WANTLEN) \
	if (WANTLEN > RETLEN) { \
		logerr("router_route: out of destination slots, " \
				"increase CONN_DESTS_SIZE in router.h\n"); \
		return 1; \
	}


	for (w = r; w != NULL; w = w->next) {
		if (w->dests->cl->type == GROUP) {
			/* strrstr doesn't exist, grrr
			 * therefore the pattern in the group is stored in reverse,
			 * such that we can start matching the tail easily without
			 * having to calculate the end of the pattern string all the
			 * time */
			for (p = firstspace - 1; p >= metric; p--) {
				for (q = w->dests->cl->name, t = p; *q != '\0' && t >= metric; q++, t--) {
					if (*q != *t)
						break;
				}
				if (*q == '\0')
					break;
			}
			/* indirection */
			assert(q != NULL);
			if (*q == '\0')
				stop = router_route_intern(
						blackholed,
						ret,
						curlen,
						retsize,
						srcaddr,
						metric,
						firstspace,
						w->dests->cl->members.routes);
		} else if (router_metric_matches(w, metric, firstspace, pmatch)) {
			stop = w->stop;
			/* rule matches, send to destination(s) */
			for (d = w->dests; d != NULL; d = d->next) {
				switch (d->cl->type) {
					case BLACKHOLE:
						*blackholed = 1;
						break;
					case FILELOGIP: {
						servers *s;
						snprintf(newmetric, sizeof(newmetric), "%s %s",
								srcaddr, metric);
						for (s = d->cl->members.forward; s != NULL; s = s->next)
						{
							failif(retsize, *curlen + 1);
							ret[*curlen].dest = s->server;
							ret[(*curlen)++].metric = strdup(newmetric);
						}
						wassent = 1;
					}	break;
					case FILELOG:
					case FORWARD: {
						/* simple case, no logic necessary */
						servers *s;
						for (s = d->cl->members.forward; s != NULL; s = s->next)
						{
							failif(retsize, *curlen + 1);
							ret[*curlen].dest = s->server;
							ret[(*curlen)++].metric = strdup(metric);
						}
						wassent = 1;
					}	break;
					case ANYOF: {
						/* we queue the same metrics at the same server */
						unsigned int hash;
						
						fnv1a_32(hash, p, metric, firstspace);

						/* We could use the retry approach here, but since
						 * our c is very small compared to MAX_INT, the bias
						 * we introduce for the last few of the range
						 * (MAX_INT % c) can be considered neglicible given
						 * the number of occurances of c in the range of
						 * MAX_INT, therefore we stick with a simple mod. */
						hash %= d->cl->members.anyof->count;
						failif(retsize, *curlen + 1);
						ret[*curlen].dest =
							d->cl->members.anyof->servers[hash];
						ret[(*curlen)++].metric = strdup(metric);
						wassent = 1;
					}	break;
					case FAILOVER: {
						/* queue at the first non-failing server */
						unsigned short i;

						failif(retsize, *curlen + 1);
						ret[*curlen].dest = NULL;
						for (i = 0; i < d->cl->members.anyof->count; i++) {
							server *s = d->cl->members.anyof->servers[i];
							if (server_failed(s))
								continue;
							ret[*curlen].dest = s;
							break;
						}
						if (ret[*curlen].dest == NULL)
							/* all failed, take first server */
							ret[*curlen].dest =
								d->cl->members.anyof->servers[0];
						ret[(*curlen)++].metric = strdup(metric);
						wassent = 1;
					}	break;
					case CARBON_CH:
					case FNV1A_CH:
					case JUMP_CH: {
						/* let the ring(bearer) decide */
						failif(retsize,
								*curlen + d->cl->members.ch->repl_factor);
						ch_get_nodes(
								&ret[*curlen],
								d->cl->members.ch->ring,
								d->cl->members.ch->repl_factor,
								metric,
								firstspace);
						*curlen += d->cl->members.ch->repl_factor;
						wassent = 1;
					}	break;
					case AGGREGATION: {
						/* aggregation rule */
						aggregator_putmetric(
								d->cl->members.aggregation,
								metric,
								firstspace,
								w->nmatch, pmatch);
						wassent = 1;
						/* we need to break out of the inner loop. since
						 * the rest of dests are meant for the stub, and
						 * we should certainly not process it now */
						while (d->next != NULL)
							d = d->next;
					}	break;
					case REWRITE: {
						/* rewrite metric name */
						if ((len = router_rewrite_metric(
									&newmetric, &newfirstspace,
									metric, firstspace,
									d->cl->members.replacement,
									w->nmatch, pmatch)) == 0)
						{
							logerr("router_route: failed to rewrite "
									"metric: newmetric size too small to hold "
									"replacement (%s -> %s)\n",
									metric, d->cl->members.replacement);
							break;
						};

						/* scary! write back the rewritten metric */
						memcpy(metric, newmetric, len);
						firstspace = metric + (newfirstspace - newmetric);
					}	break;
					case AGGRSTUB:
					case STATSTUB: {
						/* strip off the stub pattern, and reroute this
						 * thing */
						router_route_intern(
								blackholed,
								ret,
								curlen,
								retsize,
								srcaddr,
								metric + strlen(w->pattern),
								firstspace + strlen(w->pattern),
								w->dests->cl->members.routes);
					}	break;
					case GROUP: {
						/* this should not happen */
					}	break;
				}
			}

			/* stop processing further rules if requested */
			if (stop)
				break;
		}
	}
	if (!wassent)
		*blackholed = 1;

	return stop;
}

/**
 * Looks up the locations the given metric_path should be sent to, and
 * returns the list of servers in ret, the number of servers is
 * returned in retcnt.
 * Returns whether the metric was blackholed (e.g. not routed anywhere).
 */
inline char
router_route(
		router *rtr,
		destination ret[],
		size_t *retcnt,
		size_t retsize,
		char *srcaddr,
		char *metric,
		char *firstspace)
{
	size_t curlen = 0;
	char blackholed = 0;

	(void)router_route_intern(&blackholed, ret, &curlen, retsize, srcaddr,
			metric, firstspace, rtr->routes);

	*retcnt = curlen;
	return blackholed;
}

/**
 * Prints for metric_path which rules and/or aggregations would be
 * triggered.  Useful for testing regular expressions.
 */
static char
router_test_intern(char *metric, char *firstspace, route *routes)
{
	route *w;
	destinations *d;
	char gotmatch = 0;
	char newmetric[METRIC_BUFSIZ];
	char *newfirstspace = NULL;
	size_t len;
	regmatch_t pmatch[RE_MAX_MATCHES];

	for (w = routes; w != NULL; w = w->next) {
		if (w->dests->cl->type == GROUP) {
			/* just recurse, in test mode performance shouldn't be an
			 * issue at all */
			gotmatch |= router_test_intern(
					metric,
					firstspace,
					w->dests->cl->members.routes);
			if (gotmatch & 2)
				break;
		} else if (router_metric_matches(w, metric, firstspace, pmatch)) {
			gotmatch = 1;
			switch (w->dests->cl->type) {
				case AGGREGATION:
					fprintf(stdout, "aggregation\n");
					break;
				case REWRITE:
					fprintf(stdout, "rewrite\n");
					break;
				case AGGRSTUB:
				case STATSTUB: {
					gotmatch |= router_test_intern(
							metric + strlen(w->pattern),
							firstspace + strlen(w->pattern),
							w->dests->cl->members.routes);
					return gotmatch;
				}	break;
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
			for (d = w->dests; d != NULL; d = d->next) {
				switch (d->cl->type) {
					case AGGREGATION: {
						struct _aggr_computes *ac;
						int stublen = 0;
						char percentile[16];

						if (mode & MODE_DEBUG || d->next == NULL) {
							stublen = 0;
						} else {
							char x;
							stublen = snprintf(&x, 1,
									STUB_AGGR "%p__",
									d->cl->members.aggregation);
						}

						for (ac = d->cl->members.aggregation->computes; ac != NULL; ac = ac->next)
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
								len = snprintf(newmetric, sizeof(newmetric),
										"%s", ac->metric);
								if (len >= sizeof(newmetric))
										len = sizeof(newmetric) - 1;
								newfirstspace = newmetric + len;
							}

							snprintf(percentile, sizeof(percentile),
									"percentile%d", ac->percentile);
							fprintf(stdout, "    %s%s%s%s -> %s\n",
									ac->type == SUM ? "sum" : ac->type == CNT ? "count" :
									ac->type == MAX ? "max" : ac->type == MIN ? "min" :
									ac->type == AVG ? "average" :
									ac->type == MEDN ? "median" :
									ac->type == PCTL ? percentile :
									ac->type == VAR ? "variance" :
									ac->type == SDEV ? "stddev" : "<unknown>",
									w->nmatch > 0 ? "(" : "",
									w->nmatch > 0 ? ac->metric + stublen : "",
									w->nmatch > 0 ? ")" : "",
									newmetric + stublen);

							if (mode & MODE_DEBUG && d->next != NULL) {
								gotmatch |= router_test_intern(
										newmetric,
										newfirstspace,
										routes);
							}
						}
						if (mode & MODE_DEBUG) {
							return gotmatch;
						} else {
							gotmatch |= 4;
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
									d->cl->members.replacement,
									w->nmatch, pmatch)) == 0)
						{
							fprintf(stderr, "router_test: failed to rewrite "
									"metric: newmetric size too small to hold "
									"replacement (%s -> %s)\n",
									metric, d->cl->members.replacement);
							break;
						};

						/* scary! write back the rewritten metric */
						memcpy(metric, newmetric, len);
						firstspace = metric + (newfirstspace - newmetric);
						*firstspace = '\0';
						fprintf(stdout, "    into(%s) -> %s\n",
								d->cl->members.replacement, metric);
						*firstspace = ' ';
					}	break;
					case FORWARD: {
						servers *s;

						fprintf(stdout, "    forward(%s)\n", d->cl->name);
						for (s = d->cl->members.forward; s != NULL; s = s->next)
							fprintf(stdout, "        %s:%d\n",
									server_ip(s->server), server_port(s->server));
					}	break;
					case CARBON_CH:
					case FNV1A_CH:
					case JUMP_CH: {
						destination dst[CONN_DESTS_SIZE];
						int i;

						fprintf(stdout, "    %s_ch(%s)\n",
								d->cl->type == CARBON_CH ? "carbon" :
								d->cl->type == FNV1A_CH ? "fnv1a" :
								"jump_fnv1a", d->cl->name);
						if (gotmatch & 4)
							break;
						if (mode & MODE_DEBUG) {
							fprintf(stdout, "        hash_pos(%d)\n",
									ch_gethashpos(d->cl->members.ch->ring,
										metric, firstspace));
						}
						ch_get_nodes(dst,
								d->cl->members.ch->ring,
								d->cl->members.ch->repl_factor,
								metric,
								firstspace);
						for (i = 0; i < d->cl->members.ch->repl_factor; i++) {
							fprintf(stdout, "        %s:%d\n",
									server_ip(dst[i].dest),
									server_port(dst[i].dest));
							free((char *)dst[i].metric);
						}
					}	break;
					case FAILOVER:
					case ANYOF: {
						unsigned int hash;

						fprintf(stdout, "    %s(%s)\n",
								d->cl->type == ANYOF ? "any_of" : "failover",
								d->cl->name);
						if (gotmatch & 4)
							break;
						if (d->cl->type == ANYOF) {
							const char *p;
							fnv1a_32(hash, p, metric, firstspace);
							hash %= d->cl->members.anyof->count;
						} else {
							hash = 0;
						}
						fprintf(stdout, "        %s:%d\n",
								server_ip(d->cl->members.anyof->servers[hash]),
								server_port(d->cl->members.anyof->servers[hash]));
					}	break;
					default: {
						fprintf(stdout, "    cluster(%s)\n", d->cl->name);
					}	break;
				}
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
router_test(router *rtr, char *metric)
{
	char *firstspace;

	for (firstspace = metric; *firstspace != '\0'; firstspace++)
		if (*firstspace == ' ')
			break;
	if (!router_test_intern(metric, firstspace, rtr->routes)) {
		*firstspace = '\0';
		fprintf(stdout, "nothing matched %s\n", metric);
	}
	fflush(stdout);
}
