/*
 * Copyright 2013-2017 Fabian Groffen
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
#include <unistd.h>
#include <string.h>
#include <ctype.h>
#include <glob.h>
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
	VALIDATION, /* pseudo type to perform additional data validation */
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
	int refcnt;
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

typedef struct _validate {
	struct _route *rule;
	enum { VAL_LOG, VAL_DROP } action;
} validate;

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
		struct _validate *validation;
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
	servers *srvrs;
	char *collector_stub;
	struct _router_allocator {
		void *memory_region;
		void *nextp;
		size_t sz;
		struct _router_allocator *next;
	} *allocator;
};

/* custom constant, meant to force regex mode matching */
#define REG_FORCE   01000000

/* catch string used for aggrgator destinations */
#define STUB_AGGR   "_aggregator_stub_"
/* catch string used for collector output */
#define STUB_STATS  "_collector_stub_"

/**
 * Free the allocators associated to this router.
 */
static void
ra_free(router *rtr)
{
	struct _router_allocator *ra;
	struct _router_allocator *ra_next;

	for (ra = rtr->allocator; ra != NULL; ra = ra_next) {
		free(ra->memory_region);
		ra_next = ra->next;
		free(ra);
		ra = NULL;
	}
}

/**
 * malloc() in one of the allocators for this router.  If insufficient
 * memory is available in the allocator, a new one is allocated.  If
 * that fails, NULL is returned, else a pointer that can be written to
 * up to sz bytes.
 */
static void *
ra_malloc(router *rtr, size_t sz)
{
	struct _router_allocator *ra;
	void *retp = NULL;
	size_t nsz;

#define ra_alloc(RA, SZ) { \
		assert(SZ >= 0); \
		nsz = 256 * 1024; \
		if (SZ > nsz) \
			nsz = ((SZ / 1024) + 1) * 1024; \
		RA = malloc(sizeof(struct _router_allocator)); \
		if (RA == NULL) \
			return NULL; \
		RA->memory_region = malloc(sizeof(char) * nsz); \
		if (RA->memory_region == NULL) { \
			free(RA); \
			RA = NULL; \
			return NULL; \
		} \
		RA->nextp = RA->memory_region; \
		RA->sz = nsz; \
		RA->next = NULL; \
	}

	if (rtr->allocator == NULL)
		ra_alloc(rtr->allocator, 0);

	for (ra = rtr->allocator; ra != NULL; ra = ra->next) {
		if (ra->sz - (ra->nextp - ra->memory_region) >= sz) {
			retp = ra->nextp;
			ra->nextp += sz;
			return retp;
		}
		if (ra->next == NULL)
			ra_alloc(ra->next, sz);
	}

	/* this should be unreachable code */
	return NULL;
}

/**
 * strdup using ra_malloc, e.g. get memory from the allocator associated
 * to the given router.
 */
static char *
ra_strdup(router *rtr, const char *s)
{
	size_t sz = strlen(s) + 1;
	char *m = ra_malloc(rtr, sz);
	if (m == NULL)
		return m;
	memcpy(m, s, sz);
	return m;
}

/**
 * Frees routes and clusters.
 */
static void
router_free_intern(cluster *clusters, route *routes)
{
	while (routes != NULL) {
		if (routes->matchtype == REGEX)
			regfree(&routes->rule);

		if (routes->next == NULL || routes->next->dests != routes->dests) {
			while (routes->dests != NULL) {
				if (routes->dests->cl->type == GROUP ||
						routes->dests->cl->type == AGGRSTUB ||
						routes->dests->cl->type == STATSTUB)
					router_free_intern(NULL, routes->dests->cl->members.routes);
				if (routes->dests->cl->type == VALIDATION)
					router_free_intern(NULL,
							routes->dests->cl->members.validation->rule);

				routes->dests = routes->dests->next;
			}
		}

		routes = routes->next;
	}

	while (clusters != NULL) {
		switch (clusters->type) {
			case CARBON_CH:
			case FNV1A_CH:
			case JUMP_CH:
				ch_free(clusters->members.ch->ring);
				break;
			case FORWARD:
			case FILELOG:
			case FILELOGIP:
			case BLACKHOLE:
			case ANYOF:
			case FAILOVER:
				/* no special structures to free */
				break;
			case GROUP:
			case AGGRSTUB:
			case STATSTUB:
			case VALIDATION:
				/* handled at the routes above */
				break;
			case AGGREGATION:
				/* aggregators starve when they get no more input */
				break;
			case REWRITE:
				/* everything is in the allocators */
				break;
		}

		clusters = clusters->next;
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
 */
router *
router_readconfig(router *orig,
		const char *path,
		size_t queuesize,
		size_t batchsize,
		int maxstalls,
		unsigned short iotimeout,
		unsigned int sockbufsize)
{
	FILE *cnf;
	char *buf;
	size_t len = 0;
	char *p;
	struct stat st;
	struct addrinfo *saddrs;
	char matchcatchallfound = 0;
	router *ret = NULL;
	cluster *cl = NULL;
	aggregator *a = NULL;
	route *r = NULL;

	/* if there is no config, don't try anything */
	if (stat(path, &st) == -1) {
		logerr("unable to stat file '%s': %s\n", path, strerror(errno));
		return NULL;
	}

	if (orig == NULL) {
		/* get a return structure (and allocator) in place */
		if ((ret = malloc(sizeof(router))) == NULL) {
			logerr("malloc failed for router return struct\n");
			return NULL;
		}
		ret->allocator = NULL;
		ret->collector_stub = NULL;
		ret->routes = NULL;
		ret->aggregators = NULL;
		ret->srvrs = NULL;
		ret->clusters = NULL;

		/* create virtual blackhole cluster */
		cl = ra_malloc(ret, sizeof(cluster));
		if (cl == NULL) {
			logerr("malloc failed for blackhole cluster\n");
			router_free(ret);
			return NULL;
		}
		cl->name = ra_strdup(ret, "blackhole");
		cl->type = BLACKHOLE;
		cl->members.forward = NULL;
		cl->next = NULL;
		ret->clusters = cl;
	} else {
		ret = orig;
		/* position a, cl and r at the end of chains */
		for (a = ret->aggregators; a != NULL && a->next != NULL; a = a->next)
			;
		for (cl = ret->clusters; cl->next != NULL; cl = cl->next)
			;
		for (r = ret->routes; r != NULL && r->next != NULL; r = r->next)
			;
	}

	if ((buf = ra_malloc(ret, st.st_size + 1)) == NULL) {
		logerr("malloc failed for config file buffer\n");
		return NULL;
	}
	if ((cnf = fopen(path, "r")) == NULL) {
		logerr("failed to open config file '%s': %s\n", path, strerror(errno));
		return NULL;
	}
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
			char useall = 0;
			p += 8;
			for (; *p != '\0' && isspace(*p); p++)
				;
			name = p;
			for (; *p != '\0' && !isspace(*p); p++)
				;
			if (*p == '\0') {
				logerr("unexpected end of file after 'cluster'\n");
				router_free(ret);
				return NULL;
			}
			*p++ = '\0';

			if ((cl = cl->next = ra_malloc(ret, sizeof(cluster))) == NULL) {
				logerr("malloc failed for cluster '%s'\n", name);
				router_free(ret);
				return NULL;
			}
			cl->name = ra_strdup(ret, name);
			cl->next = NULL;

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
						router_free(ret);
						return NULL;
					}
					*p++ = '\0';
					if ((replcnt = atoi(repl)) == 0)
						replcnt = 1;
				}

				cl->type = chtype;
				cl->members.ch = ra_malloc(ret, sizeof(chashring));
				if (cl->members.ch == NULL) {
					logerr("malloc failed for ch in cluster '%s'\n", name);
					router_free(ret);
					return NULL;
				}
				cl->members.ch->repl_factor = (unsigned char)replcnt;
				cl->members.ch->ring =
					ch_new(chtype == CARBON_CH ? CARBON :
							chtype == FNV1A_CH ? FNV1a : JUMP_FNV1a);
			} else if (strncmp(p, "forward", 7) == 0 && isspace(*(p + 7))) {
				p += 8;

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

				cl->type = ANYOF;
				cl->members.anyof = NULL;
			} else if (strncmp(p, "failover", 8) == 0 && isspace(*(p + 8))) {
				p += 9;

				cl->type = FAILOVER;
				cl->members.anyof = NULL;
			} else if (strncmp(p, "file", 4) == 0 && isspace(*(p + 4))) {
				p += 5;

				for (; *p != '\0' && isspace(*p); p++)
					;
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
				router_free(ret);
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
					router_free(ret);
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
						router_free(ret);
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
						router_free(ret);
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
							router_free(ret);
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
						router_free(ret);
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
					servers *s;

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

					newserver = NULL;
					for (s = ret->srvrs; s != NULL; s = s->next) {
						if (strcmp(server_ip(s->server), ip) == 0 &&
								server_port(s->server) == port &&
								(*proto != 't' || server_ctype(s->server) == CON_TCP) &&
								(*proto != 'u' || server_ctype(s->server) == CON_UDP))
						{
							newserver = s->server;
							s->refcnt++;
							break;
						}
					}
					if (newserver == NULL) {
						newserver = server_new(ip, (unsigned short)port,
								*proto == 'f' ? CON_FILE :
								*proto == 'u' ? CON_UDP : CON_TCP,
								walk == (void *)1 ? NULL : walk,
								queuesize, batchsize, maxstalls,
								iotimeout, sockbufsize);
						if (newserver == NULL) {
							logerr("failed to add server %s:%d (%s) "
									"to cluster %s: %s\n", ip, port, proto,
									name, strerror(errno));
							router_free(ret);
							freeaddrinfo(saddrs);
							return NULL;
						}
						if (ret->srvrs == NULL) {
							s = ret->srvrs = ra_malloc(ret, sizeof(servers));
						} else {
							for (s = ret->srvrs; s->next != NULL; s = s->next)
								;
							s = s->next = ra_malloc(ret, sizeof(servers));
						}
						s->next = NULL;
						s->refcnt = 1;
						s->server = newserver;
					}

					if (cl->type == CARBON_CH ||
							cl->type == FNV1A_CH ||
							cl->type == JUMP_CH)
					{
						if (w == NULL) {
							cl->members.ch->servers = w =
								ra_malloc(ret, sizeof(servers));
						} else {
							w = w->next = ra_malloc(ret, sizeof(servers));
						}
						if (w == NULL) {
							logerr("malloc failed for %s_ch %s\n",
									cl->type == CARBON_CH ? "carbon" :
									cl->type == FNV1A_CH ? "fnv1a" :
									"jump_fnv1a", ip);
							router_free(ret);
							freeaddrinfo(saddrs);
							return NULL;
						}
						w->next = NULL;
						if (s->refcnt > 1) {
							char *sinst = server_instance(newserver);
							if (sinst == NULL && inst != NULL) {
								logerr("cannot set instance '%s' for "
										"server %s:%d: server was previously "
										"defined without instance\n",
										inst, server_ip(newserver),
										server_port(newserver));
								router_free(ret);
								freeaddrinfo(saddrs);
								return NULL;
							} else if (sinst != NULL && inst == NULL) {
								logerr("cannot define server %s:%d without "
										"instance: server was previously "
										"defined with instance '%s'\n",
										server_ip(newserver),
										server_port(newserver), sinst);
								router_free(ret);
								freeaddrinfo(saddrs);
								return NULL;
							} else if (sinst != NULL && inst != NULL &&
									strcmp(sinst, inst) != 0)
							{
								logerr("cannot set instance '%s' for "
										"server %s:%d: server was previously "
										"defined with instance '%s'\n",
										inst, server_ip(newserver),
										server_port(newserver), sinst);
								router_free(ret);
								freeaddrinfo(saddrs);
								return NULL;
							} /* else: sinst == inst == NULL */
						}
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
							router_free(ret);
							freeaddrinfo(saddrs);
							return NULL;
						}
					} else if (cl->type == FORWARD ||
							cl->type == ANYOF ||
							cl->type == FAILOVER ||
							cl->type == FILELOG ||
							cl->type == FILELOGIP)
					{
						if (w == NULL) {
							w = ra_malloc(ret, sizeof(servers));
						} else {
							w = w->next = ra_malloc(ret, sizeof(servers));
						}
						if (w == NULL) {
							logerr("malloc failed for %s %s\n",
									cl->type == FORWARD ? "forward" :
									cl->type == ANYOF ? "any_of" :
									cl->type == FAILOVER ? "failover" :
									"file",
									ip);
							router_free(ret);
							freeaddrinfo(saddrs);
							return NULL;
						}
						w->next = NULL;
						w->server = newserver;
						if ((cl->type == FORWARD ||
							 cl->type == FILELOG || cl->type == FILELOGIP)
									&& cl->members.forward == NULL)
							cl->members.forward = w;
						if (cl->type == ANYOF || cl->type == FAILOVER) {
							if (s->refcnt > 1) {
								logerr("cannot share server %s:%d with "
										"any_of/failover cluster '%s'\n",
										server_ip(newserver),
										server_port(newserver),
										cl->name);
								router_free(ret);
								freeaddrinfo(saddrs);
								return NULL;
							}
							if (cl->members.anyof == NULL) {
								cl->members.anyof =
									ra_malloc(ret, sizeof(serverlist));
								if (cl->members.anyof == NULL) {
									logerr("malloc failed for %s anyof list\n",
											ip);
									router_free(ret);
									freeaddrinfo(saddrs);
									return NULL;
								}
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
					ra_malloc(ret, sizeof(server *) * cl->members.anyof->count);
				if (cl->members.anyof->servers == NULL) {
					logerr("malloc failed for anyof servers\n");
					router_free(ret);
					return NULL;
				}
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
					router_free(ret);
					return NULL;
				}
			}
		} else if (strncmp(p, "match", 5) == 0 && isspace(*(p + 5))) {
			/* match rule */
			char *pat;
			char *dest;
			char stop = -1;
			cluster *w;
			route *m = NULL;
			destinations *d = NULL;
			destinations *dw = NULL;

			p += 6;
			for (; *p != '\0' && isspace(*p); p++)
				;

			do {
				pat = p;
				for (; *p != '\0' && !isspace(*p); p++)
					;
				if (*p == '\0') {
					logerr("unexpected end of file after 'match'\n");
					router_free(ret);
					return NULL;
				}
				*p++ = '\0';
				for (; *p != '\0' && isspace(*p); p++)
					;

				if (r == NULL) {
					ret->routes = r = ra_malloc(ret, sizeof(route));
				} else {
					r = r->next = ra_malloc(ret, sizeof(route));
				}
				if (r == NULL) {
					logerr("malloc failed for match '%s'\n", pat);
					router_free(ret);
					return NULL;
				}
				r->next = NULL;
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
						router_free(ret);
						return NULL;
					}
				}
			} while (
					(strncmp(p, "send", 4) != 0 || !isspace(*(p + 4))) &&
					(strncmp(p, "validate", 8) != 0 || !isspace(*(p + 8))));
			if (*p == 'v') {
				p += 9;

				if (dw == NULL) {
					dw = d = ra_malloc(ret, sizeof(destinations));
				} else {
					d = d->next = ra_malloc(ret, sizeof(destinations));
				}
				if (d == NULL) {
					logerr("out of memory allocating new validation "
							"for 'match %s'\n", pat);
					router_free(ret);
					return NULL;
				}
				d->next = NULL;
				if ((d->cl = ra_malloc(ret, sizeof(cluster))) == NULL) {
					logerr("malloc failed for validation rule in 'match %s'\n",
							pat);
					router_free(ret);
					return NULL;
				}
				d->cl->name = NULL;
				d->cl->type = VALIDATION;
				d->cl->next = NULL;
				d->cl->members.validation = ra_malloc(ret, sizeof(validate));
				if (d->cl->members.validation == NULL) {
					logerr("malloc failed for validation member in "
							"'match %s'\n", pat);
					router_free(ret);
					return NULL;
				}

				for (; *p != '\0' && isspace(*p); p++)
					;
				pat = p;
				do {
					for (; *p != '\0' && !isspace(*p); p++)
						;
				} while (*p != '\0' && *(p - 1) == '\\' && *(p++) != '\0');
				if (*p == '\0') {
					logerr("unexpected end of file after 'validate'\n");
					router_free(ret);
					return NULL;
				}
				*p++ = '\0';
				for (; *p != '\0' && isspace(*p); p++)
					;
				if (strcmp(pat, "*") == 0) {
					logerr("cannot use '*' as validate expression\n");
					router_free(ret);
					return NULL;
				} else {
					route *rule = ra_malloc(ret, sizeof(route));
					int err;

					if (rule == NULL) {
						logerr("malloc failed for validate '%s'\n", pat);
						router_free(ret);
						return NULL;
					}
					rule->next = NULL;
					rule->dests = NULL;
					d->cl->members.validation->rule = rule;

					err = determine_if_regex(rule, pat,
							REG_EXTENDED | REG_NOSUB);
					if (err != 0) {
						char ebuf[512];
						regerror(err, &rule->rule, ebuf, sizeof(ebuf));
						logerr("invalid expression '%s' for validate: %s\n",
								pat, ebuf);
						router_free(ret);
						return NULL;
					}
				}

				if (strncmp(p, "else", 4) != 0 || !isspace(*(p + 4))) {
					logerr("expected 'else' after validate %s\n", pat);
					router_free(ret);
					return NULL;
				}
				p += 5;

				for (; *p != '\0' && isspace(*p); p++)
					;

				if ((strncmp(p, "drop", 4) != 0 || !isspace(*(p + 4))) &&
						(strncmp(p, "log", 3) != 0 || !isspace(*(p + 3))))
				{
					logerr("expected 'drop' or 'log' after validate %s else\n",
							pat);
					router_free(ret);
					return NULL;
				}
				if (*p == 'd') {
					p += 5;
					d->cl->members.validation->action = VAL_DROP;
				} else {
					p += 4;
					d->cl->members.validation->action = VAL_LOG;
				}

				for (; *p != '\0' && isspace(*p); p++)
					;
				if (*p == '\0') {
					logerr("unexpected end of file after validate %s\n", pat);
					router_free(ret);
					return NULL;
				}

				if (strncmp(p, "stop", 4) == 0) {
					if (isspace(*(p + 4))) {
						p += 5;
						for (; *p != '\0' && isspace(*p); p++)
							;
					} else if (*(p + 4) != ';') {
						logerr("expected ';' after 'stop' for validate %s\n",
								pat);
						router_free(ret);
						return NULL;
					}
				}
				if (*p == ';') {
					p++;
					do {
						m->dests = dw;
						/* always ignore stop here, else the whole
						 * contruct makes no sense */
						m->stop = 0;
					} while (m != r && (m = m->next) != NULL);
					continue;
				}

				if (strncmp(p, "send", 4) != 0 || !isspace(*(p + 4))) {
					logerr("expected 'send to' after validate %s\n", pat);
					router_free(ret);
					return NULL;
				}
			}
			p += 5;
			for (; *p != '\0' && isspace(*p); p++)
				;
			if (strncmp(p, "to", 2) != 0 || !isspace(*(p + 2))) {
				logerr("expected 'to' after 'send' for match %s\n",
						r->pattern ? r->pattern : "*");
				router_free(ret);
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
				for (w = ret->clusters; w != NULL; w = w->next) {
					if (w->type != GROUP &&
							w->type != AGGRSTUB &&
							w->type != STATSTUB &&
							w->type != AGGREGATION &&
							w->type != REWRITE &&
							w->type != VALIDATION &&
							strcmp(w->name, dest) == 0)
						break;
				}
				if (w == NULL) {
					logerr("no such cluster '%s' for 'match %s'\n", dest,
							r->pattern ? r->pattern : "*");
					router_free(ret);
					return NULL;
				}

				if (dw == NULL) {
					dw = d = ra_malloc(ret, sizeof(destinations));
				} else {
					d = d->next = ra_malloc(ret, sizeof(destinations));
				}
				if (d == NULL) {
					logerr("out of memory allocating new destination '%s' "
							"for 'match %s'\n", dest,
							r->pattern ? r->pattern : "*");
					router_free(ret);
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
				router_free(ret);
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
				router_free(ret);
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

			w = ra_malloc(ret, sizeof(cluster));
			if (w == NULL) {
				logerr("malloc failed for aggregate\n");
				router_free(ret);
				return NULL;
			}
			w->name = NULL;
			w->type = AGGREGATION;
			w->next = NULL;

			do {
				pat = p;
				for (; *p != '\0' && !isspace(*p); p++)
					;
				if (*p == '\0') {
					logerr("unexpected end of file after 'aggregate'\n");
					router_free(ret);
					return NULL;
				}
				*p++ = '\0';
				for (; *p != '\0' && isspace(*p); p++)
					;

				if (r == NULL) {
					ret->routes = r = ra_malloc(ret, sizeof(route));
				} else {
					r = r->next = ra_malloc(ret, sizeof(route));
				}
				if (r == NULL) {
					logerr("malloc failed for aggregate route\n");
					router_free(ret);
					return NULL;
				}
				r->next = NULL;
				if (m == NULL)
					m = r;
				err = determine_if_regex(r, pat, REG_EXTENDED);
				if (err != 0) {
					char ebuf[512];
					regerror(err, &r->rule, ebuf, sizeof(ebuf));
					logerr("invalid expression '%s' "
							"for aggregation: %s\n", pat, ebuf);
					router_free(ret);
					return NULL;
				}
			} while (strncmp(p, "every", 5) != 0 || !isspace(*(p + 5)));
			p += 6;
			for (; *p != '\0' && isspace(*p); p++)
				;
			num = p;
			for (; *p != '\0' && isdigit(*p); p++)
				;
			if (*p == '\0') {
				logerr("unexpected end of file after 'every %s'\n", num);
				router_free(ret);
				return NULL;
			}
			if (!isspace(*p)) {
				logerr("unexpected character '%c', "
						"expected number after 'every'\n", *p);
				router_free(ret);
				return NULL;
			}
			*p++ = '\0';
			for (; *p != '\0' && isspace(*p); p++)
				;
			if (strncmp(p, "seconds", 7) != 0 || !isspace(*(p + 7))) {
				logerr("expected 'seconds' after 'every %s'\n", num);
				router_free(ret);
				return NULL;
			}
			p += 8;
			intv = atoi(num);
			if (intv == 0) {
				logerr("interval must be non-zero\n");
				router_free(ret);
				return NULL;
			}
			for (; *p != '\0' && isspace(*p); p++)
				;
			if (strncmp(p, "expire", 6) != 0 || !isspace(*(p + 6))) {
				logerr("expected 'expire after' after 'every %s seconds\n",
						num);
				router_free(ret);
				return NULL;
			}
			p += 7;
			for (; *p != '\0' && isspace(*p); p++)
				;
			if (strncmp(p, "after", 5) != 0 || !isspace(*(p + 5))) {
				logerr("expected 'after' after 'expire'\n");
				router_free(ret);
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
				router_free(ret);
				return NULL;
			}
			if (!isspace(*p)) {
				logerr("unexpected character '%c', "
						"expected number after 'expire after'\n", *p);
				router_free(ret);
				return NULL;
			}
			*p++ = '\0';
			for (; *p != '\0' && isspace(*p); p++)
				;
			if (strncmp(p, "seconds", 7) != 0 || !isspace(*(p + 7))) {
				logerr("expected 'seconds' after 'expire after %s'\n", num);
				router_free(ret);
				return NULL;
			}
			p += 8;
			exp = atoi(num);
			if (exp == 0) {
				logerr("expire must be non-zero\n");
				router_free(ret);
				return NULL;
			}
			if (exp < intv) {
				logerr("expire (%d) must be greater than interval (%d)\n",
						exp, intv);
				router_free(ret);
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
					router_free(ret);
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
					router_free(ret);
					return NULL;
				}
				for (; *p != '\0' && isspace(*p); p++)
					;
				if (strncmp(p, "of", 2) != 0 || !isspace(*(p + 2))) {
					logerr("expected 'of' after 'timestamp at ...'\n");
					router_free(ret);
					return NULL;
				}
				p += 3;
				for (; *p != '\0' && isspace(*p); p++)
					;
				if (strncmp(p, "bucket", 6) != 0 || !isspace(*(p + 6))) {
					logerr("expected 'bucket' after 'timestamp at ... of'\n");
					router_free(ret);
					return NULL;
				}
				p += 7;
				for (; *p != '\0' && isspace(*p); p++)
					;
			}

			w->members.aggregation = aggregator_new(intv, exp, tswhen);
			if (w->members.aggregation == NULL) {
				logerr("out of memory while allocating new aggregator\n");
				router_free(ret);
				return NULL;
			}
			if (a == NULL) {
				ret->aggregators = a = w->members.aggregation;
			} else {
				a = a->next = w->members.aggregation;
			}
			do {
				if (strncmp(p, "compute", 7) != 0 || !isspace(*(p + 7))) {
					logerr("expected 'compute' at: %.20s\n", p);
					router_free(ret);
					return NULL;
				}
				p += 8;
				pat = p;
				for (; *p != '\0' && !isspace(*p); p++)
					;
				if (*p == '\0') {
					logerr("unexpected end of file after 'compute'\n");
					router_free(ret);
					return NULL;
				}
				*p++ = '\0';
				type = pat;
				for (; *p != '\0' && isspace(*p); p++)
					;
				if (strncmp(p, "write", 5) != 0 || !isspace(*(p + 5))) {
					logerr("expected 'write to' after 'compute %s'\n", pat);
					router_free(ret);
					return NULL;
				}
				p += 6;
				for (; *p != '\0' && isspace(*p); p++)
					;
				if (strncmp(p, "to", 2) != 0 || !isspace(*(p + 2))) {
					logerr("expected 'write to' after 'compute %s'\n", pat);
					router_free(ret);
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
					router_free(ret);
					return NULL;
				}
			 	*p++ = '\0';

				if (aggregator_add_compute(w->members.aggregation, pat, type) != 0) {
					logerr("expected sum, count, max, min, average, "
							"median, percentile<%>, variance or stddev "
							"after 'compute', got '%s'\n", type);
					router_free(ret);
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
					router_free(ret);
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
					for (cw = ret->clusters; cw != NULL; cw = cw->next) {
						if (cw->type != GROUP &&
								cw->type != AGGRSTUB &&
								cw->type != STATSTUB &&
								cw->type != AGGREGATION &&
								cw->type != REWRITE &&
								cw->type != VALIDATION &&
								strcmp(cw->name, dest) == 0)
							break;
					}
					if (cw == NULL) {
						logerr("no such cluster '%s' for aggregate\n", dest);
						router_free(ret);
						return NULL;
					}

					if (dw == NULL) {
						dw = d = ra_malloc(ret, sizeof(destinations));
					} else {
						d = d->next = ra_malloc(ret, sizeof(destinations));
					}
					if (d == NULL) {
						logerr("out of memory allocating new destination '%s' "
								"for aggregation\n", dest);
						router_free(ret);
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
				router_free(ret);
				return NULL;
			}
			if (*p != ';') {
				logerr("expected ';' after aggregate %s\n", r->pattern);
				router_free(ret);
				return NULL;
			}
			p++;

			/* add cluster to the list of clusters */
			cl = cl->next = w;

			/* fill in the destinations for all the routes, this is for
			 * printing and free-ing */
			do {
				m->dests = ra_malloc(ret, sizeof(destinations));
				if (m->dests == NULL) {
					logerr("malloc failed for aggregation destinations\n");
					router_free(ret);
					return NULL;
				}
				m->dests->cl = w;
				m->dests->next = dw;
				m->stop = stop;
			} while (m != r && (m = m->next) != NULL);

			if (dw != NULL) {
				char stubname[48];

				m = ra_malloc(ret, sizeof(route));
				if (m == NULL) {
					logerr("malloc failed for aggregation destination stub\n");
					router_free(ret);
					return NULL;
				}
				m->pattern = NULL;
				m->strmatch = NULL;
				m->dests = dw;
				m->stop = 1;
				m->matchtype = MATCHALL;
				m->next = NULL;

				/* inject stub route for dests */
				d = ra_malloc(ret, sizeof(destinations));
				if (d == NULL) {
					logerr("malloc failed for aggr stub route\n");
					router_free(ret);
					return NULL;
				}
				d->next = NULL;
				cl = cl->next = d->cl = ra_malloc(ret, sizeof(cluster));
				if (cl == NULL) {
					logerr("malloc failed for aggr stub destinations\n");
					router_free(ret);
					return NULL;
				}
				cl->name = NULL;
				cl->type = AGGRSTUB;
				cl->members.routes = m;
				cl->next = NULL;

				snprintf(stubname, sizeof(stubname),
						STUB_AGGR "%p__", w->members.aggregation);
				m = ra_malloc(ret, sizeof(route));
				if (m == NULL) {
					logerr("malloc failed for catch stub aggr route\n");
					router_free(ret);
					return NULL;
				}
				m->pattern = ra_strdup(ret, stubname);
				m->strmatch = ra_strdup(ret, stubname);
				if (m->pattern == NULL || m->strmatch == NULL) {
					logerr("malloc failed for catch stub aggr route pattern\n");
					router_free(ret);
					return NULL;
				}
				m->dests = d;
				m->stop = 1;
				m->matchtype = STARTS_WITH;
				/* enforce first match to avoid interference */
				m->next = ret->routes;
				ret->routes = m;

				aggregator_set_stub(w->members.aggregation, stubname);
			}

			if (matchcatchallfound) {
				logerr("warning: aggregate %s will never be matched "
						"due to preceding match * ... stop\n",
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
				router_free(ret);
				return NULL;
			}
			*p++ = '\0';
			for (; *p != '\0' && isspace(*p); p++)
				;
			if (strncmp(p, "into", 4) != 0 || !isspace(*(p + 4))) {
				logerr("expected 'into' after rewrite %s\n", pat);
				router_free(ret);
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
				router_free(ret);
				return NULL;
			} else if (*p == ';') {
				*p++ = '\0';
			} else {
				*p++ = '\0';
				for (; *p != '\0' && isspace(*p) && *p != ';'; p++)
					;
				if (*p != ';') {
					logerr("expected ';' after %s\n", replacement);
					router_free(ret);
					return NULL;
				}
				p++;
			}

			if (r == NULL) {
				ret->routes = r = ra_malloc(ret, sizeof(route));
			} else {
				r = r->next = ra_malloc(ret, sizeof(route));
			}
			if (r == NULL) {
				logerr("malloc failed for rewrite route\n");
				router_free(ret);
				return NULL;
			}
			r->next = NULL;
			err = determine_if_regex(r, pat, REG_EXTENDED | REG_FORCE);
			if (err != 0) {
				char ebuf[512];
				regerror(err, &r->rule, ebuf, sizeof(ebuf));
				logerr("invalid expression '%s' for rewrite: %s\n", pat, ebuf);
				router_free(ret);
				return NULL;
			}

			if ((cl = cl->next = ra_malloc(ret, sizeof(cluster))) == NULL) {
				logerr("malloc failed for rewrite destination\n");
				router_free(ret);
				return NULL;
			}
			cl->type = REWRITE;
			cl->name = NULL;
			cl->members.replacement = ra_strdup(ret, replacement);
			cl->next = NULL;
			if (cl->members.replacement == NULL) {
				logerr("malloc failed for rewrite cluster replacement\n");
				router_free(ret);
				return NULL;
			}

			r->dests = ra_malloc(ret, sizeof(destinations));
			if (r->dests == NULL) {
				logerr("malloc failed for rewrite destinations\n");
				router_free(ret);
				return NULL;
			}
			r->dests->cl = cl;
			r->dests->next = NULL;
			r->stop = 0;
			r->next = NULL;

			if (matchcatchallfound) {
				logerr("warning: rewrite %s will never be matched "
						"due to preceding match * ... stop\n",
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
				router_free(ret);
				return NULL;
			}
			p += 10;
			for (; *p != '\0' && isspace(*p); p++)
				;
			if (strncmp(p, "to", 2) != 0 || !isspace(*(p + 2))) {
				logerr("expected 'to' after 'send statistics'\n");
				router_free(ret);
				return NULL;
			}
			p += 2;
			if (ret->collector_stub != NULL) {
				logerr("duplicate 'send statistics to' not allowed, "
						"use multiple destinations instead\n");
				router_free(ret);
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
				for (cw = ret->clusters; cw != NULL; cw = cw->next) {
					if (cw->type != GROUP &&
							cw->type != AGGRSTUB &&
							cw->type != STATSTUB &&
							cw->type != AGGREGATION &&
							cw->type != REWRITE &&
							cw->type != VALIDATION &&
							strcmp(cw->name, dest) == 0)
						break;
				}
				if (cw == NULL) {
					logerr("no such cluster '%s' for send statistics to\n",
							dest);
					router_free(ret);
					return NULL;
				}

				if (dw == NULL) {
					dw = d = ra_malloc(ret, sizeof(destinations));
				} else {
					d = d->next = ra_malloc(ret, sizeof(destinations));
				}
				if (d == NULL) {
					logerr("out of memory allocating new destination '%s' "
							"for 'send statistics to'\n", dest);
					router_free(ret);
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
				router_free(ret);
				return NULL;
			}

			/* create a top-rule with match for unique prefix and send
			 * that to the defined targets */
			m = ra_malloc(ret, sizeof(route));
			if (m == NULL) {
				logerr("malloc failed for send statistics route\n");
				router_free(ret);
				return NULL;
			}
			m->pattern = NULL;
			m->strmatch = NULL;
			m->dests = dw;
			m->stop = 1;
			m->matchtype = MATCHALL;
			m->next = NULL;

			/* inject stub route for dests */
			d = ra_malloc(ret, sizeof(destinations));
			if (d == NULL) {
				logerr("malloc failed for send statistics stub\n");
				router_free(ret);
				return NULL;
			}
			d->next = NULL;
			cl = cl->next = d->cl = ra_malloc(ret, sizeof(cluster));
			if (cl == NULL) {
				logerr("malloc failed for send statistics cluster\n");
				router_free(ret);
				return NULL;
			}
			cl->name = NULL;
			cl->type = STATSTUB;
			cl->members.routes = m;
			cl->next = NULL;

			snprintf(stubname, sizeof(stubname),
					STUB_STATS "%p__", ret);
			m = ra_malloc(ret, sizeof(route));
			if (m == NULL) {
				logerr("malloc failed for send statistics route stub\n");
				router_free(ret);
				return NULL;
			}
			m->pattern = ra_strdup(ret, stubname);
			m->strmatch = ra_strdup(ret, stubname);
			if (m->pattern == NULL || m->strmatch == NULL) {
				logerr("malloc failed for send statistics stub pattern\n");
				router_free(ret);
				return NULL;
			}
			m->dests = d;
			m->stop = 1;
			m->matchtype = STARTS_WITH;
			/* enforce first match to avoid interference */
			m->next = ret->routes;
			ret->routes = m;
			if (r == NULL)
				r = ret->routes;

			ret->collector_stub = m->pattern;
		} else if (strncmp(p, "include", 7) == 0 && isspace(*(p + 7))) {
			char *name = NULL;
			char endchar;

			p += 7;
			for (; *p != '\0' && isspace(*p); p++)
				;
			if (*p == '\0') {
				logerr("unexpected end of input after 'include'\n");
				router_free(ret);
				return NULL;
			}
			name = p;
			for (; *p != '\0' && !isspace(*p) && *p != ';'; p++)
				;
			if (*p == '\0') {
				logerr("unexpected end of input after 'include %s' "
						"(did you forget ';'?)\n", name);
				router_free(ret);
				return NULL;
			}
			endchar = *p;
			*p = '\0';
			if (strchr(name, '*') || strchr(name, '?') ||
					(strchr(name, '[') && strchr(name, ']')))
			{
				/* include path is a glob pattern */
				glob_t globbuf;
				char *globpath;
				size_t i;

				if ((i = glob(name, 0, NULL, &globbuf)) != 0) {
					switch (i) {
						case GLOB_NOSPACE:
							/* since we don't set a limit, we won't
							 * reach it either */
							logerr("out of memory while globbing files for "
									"pattern '%s'\n", name);
							router_free(ret);
							ret = NULL;
							break;
						case GLOB_ABORTED:
							/* we don't use a call-back, so this is
							 * a real error of some sort */
							logerr("unable to match any files from "
									"pattern '%s': %s\n",
									name, strerror(errno));
							router_free(ret);
							ret = NULL;
							break;
						case GLOB_NOMATCH:
							/* we don't want to abort on failing globs */
							logout("warning: pattern '%s' did not match "
									"any files\n", name);
							break;
					}
				}

				/* read all files matched by glob */
				for (i = 0; i < globbuf.gl_pathc; i++) {
					globpath = globbuf.gl_pathv[i];
					ret = router_readconfig(ret, globpath, queuesize,
							batchsize, maxstalls, iotimeout, sockbufsize);
					if (ret == NULL)
						break;
				}

				/* also after errors the globbuf structure is
				 * initialised and might need free-ing */
				globfree(&globbuf);
			} else {
				/* include path is a regular file path */
				ret = router_readconfig(ret, name, queuesize,
						batchsize, maxstalls, iotimeout, sockbufsize);
			}
			if (ret == NULL)
				/* router_readconfig already barked and freed ret */
				return NULL;
			/* the included file could have added new aggregates,
			 * matches, or clusters, so adjust these chains to point to
			 * the new end. */
			for (a = ret->aggregators; a != NULL && a->next != NULL; a = a->next)
				;
			for (cl = ret->clusters; cl->next != NULL; cl = cl->next)
				;
			for (r = ret->routes; r != NULL && r->next != NULL; r = r->next)
				;
			*p = endchar;
			for (; *p != '\0' && isspace(*p); p++)
				;
			if (*p != ';') {
				logerr("expected ';' after 'include %s'\n", name);
				router_free(ret);
				return NULL;
			}
			p++;
		} else {
			/* garbage? */
			logerr("unexpected input in config: %s\n", p);
			router_free(ret);
			return NULL;
		}
	} while (*p != '\0');

	return ret;
}

typedef struct _block {
	char *pattern;
	size_t hash;
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
	route *rlast;
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
	blocks->pattern = NULL;
	blocks->hash = 0;
	blocks->seqnr = seq++;
	blocks->prev = NULL;
	blocks->next = NULL;
	rlast = NULL;
	for (rwalk = r->routes; rwalk != NULL; rwalk = rnext) {
		/* matchall and rewrite rules cannot be in a group (issue #218) */
		if (rwalk->matchtype == MATCHALL || rwalk->dests->cl->type == REWRITE) {
			blast->next = malloc(sizeof(block));
			blast->next->prev = blast;
			blast = blast->next;
			blast->pattern = NULL;
			blast->hash = 0;
			blast->refcnt = 1;
			blast->seqnr = seq++;
			blast->firstroute = rwalk;
			blast->lastroute = rwalk;
			blast->next = NULL;
			rnext = rwalk->next;
			rwalk->next = NULL;
			bstart = blast;
			rlast = NULL;
			continue;
		}

		p = rwalk->pattern + strlen(rwalk->pattern);
		/* strip off chars that won't belong to a block */
		bsum = 0;
		while (
				p > rwalk->pattern && (
					bsum > 0 || (
						(*p < 'a' || *p > 'z') &&
						(*p < 'A' || *p > 'Z') &&
						*p != '_'
					)
				)
			  )
		{
			if (*p == ')' || *p == '(') {
				char esc = 0;
				char oe = *p;
				/* trim escapes, need to figure out if this is escaped
				 * or not, in order to find the a matching open or not */
				for (--p; p > rwalk->pattern && *p == '\\'; p--)
					esc = !esc;
				if (!esc) /* not escaped */
					bsum += oe == ')' ? 1 : -1;
				continue;  /* skip p-- */
			}
			p--;
		}
		if (p == rwalk->pattern) {
			/* nothing we can do with a pattern like this */
			blast->next = malloc(sizeof(block));
			blast->next->prev = blast;
			blast = blast->next;
			blast->pattern = NULL;
			blast->hash = 0;
			blast->refcnt = 1;
			blast->seqnr = seq++;
			blast->firstroute = rwalk;
			blast->lastroute = rwalk;
			blast->next = NULL;
			rnext = rwalk->next;
			rwalk->next = NULL;
			bstart = blast;
			rlast = NULL;
			continue;
		}
		/* find the block */
		bsum = 0;
		b = pblock;
		while (
				p >= rwalk->pattern && b - pblock < sizeof(pblock) &&
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
			/* this probably isn't selective enough, don't put in a group */
			blast->next = malloc(sizeof(block));
			blast->next->prev = blast;
			blast = blast->next;
			blast->pattern = NULL;
			blast->hash = 0;
			blast->refcnt = 1;
			blast->seqnr = seq++;
			blast->firstroute = rwalk;
			blast->lastroute = rwalk;
			blast->next = NULL;
			rnext = rwalk->next;
			rwalk->next = NULL;
			bstart = blast;
			rlast = NULL;
			continue;
		}

		/* at this point, b points to the tail block in reverse, see if
		 * we already had such tail in place */
		for (bwalk = bstart; bwalk != NULL; bwalk = bwalk->next) {
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
			blast->refcnt = 1;
			blast->seqnr = seq;
			blast->firstroute = rwalk;
			blast->lastroute = rwalk;
			blast->next = NULL;
			rnext = rwalk->next;
			rwalk->next = NULL;
			if (rwalk->stop) {
				bstart = blast;
				rlast = NULL;
				seq++;
			}
			continue;
		}

		bwalk->refcnt++;
		bwalk->lastroute = bwalk->lastroute->next = rwalk;
		rnext = rwalk->next;
		rwalk->next = NULL;
		if (rlast == NULL || strcmp(rlast->pattern, b) == 0) {
			if (rwalk->stop) {
				/* move this one to the end */
				if (bwalk->next != NULL) {
					bwalk->prev->next = bwalk->next;
					blast = blast->next = bwalk;
					bwalk->next = NULL;
				}
				rlast = rwalk;
			}
		} else {
			bstart = blast;
			rlast = NULL;
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
	/* create groups, if feasible */
	for (bwalk = blocks; bwalk != NULL; bwalk = blast) {
		if (bwalk->seqnr != seq) {
			seq++;
			blast = bwalk;
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
	}
}

/**
 * Returns all (unique) servers from the cluster-configuration.
 */
server **
router_getservers(router *r)
{
	server **ret;
	servers *s;
	int i;

	i = 0;
	for (s = r->srvrs; s != NULL; s = s->next)
		i++;

	ret = malloc(sizeof(server *) * (i + 1));
	if (ret == NULL)
		return NULL;

	i = 0;
	for (s = r->srvrs; s != NULL; s = s->next)
		ret[i++] = s->server;

	/* ensure NULL-termination */
	ret[i] = NULL;

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
		if (pmode & PMODE_HASH) {
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

			if (!(pmode & PMODE_AGGR))
				continue;

			if (pmode & PMODE_STUB || r->dests->next == NULL) {
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
			if (!(pmode & PMODE_STUB) && r->dests->next != NULL) {
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
			if (mode & PMODE_AGGR) {
				router srtr;
				memset(&srtr, 0, sizeof(srtr));
				srtr.routes = r->dests->cl->members.routes;
				/* recurse */
				router_printconfig(&srtr, f, pmode);
			}
		} else if (r->dests->cl->type == AGGRSTUB ||
				r->dests->cl->type == STATSTUB)
		{
			if (pmode & PMODE_STUB) {
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
			destinations *d;
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
			d = or->dests;
			if (d->cl->type == VALIDATION) {
				validate *v = d->cl->members.validation;
				fprintf(f, "    validate %s else %s\n",
						v->rule->pattern,
						v->action == VAL_LOG ? "log" : "drop");
				/* hide this pseudo target */
				d = d->next;
			}
			if (d != NULL) {
				fprintf(f, "    send to");
				if (d->next == NULL) {
					fprintf(f, " %s", d->cl->name);
				} else {
					for ( ; d != NULL; d = d->next)
						fprintf(f, "\n        %s", d->cl->name);
				}
				fprintf(f, "\n%s    ;\n", or->stop ? "    stop\n" : "");
			} else {
				fprintf(f, "    ;\n");
			}
		}
	}
	fflush(f);
}

/**
 * Prints the differences between old and new at the highest level of
 * the change.  (When a cluster is gone, its servers are not
 * considered.)  Returns whether there were changes found.
 */
char
router_printdiffs(router *old, router *new, FILE *out)
{
	FILE *f;
	int fd;
	char *tmp = getenv("TMPDIR");
	char patho[512];
	char pathn[512];
	char buf[1024];
	size_t len;
	int ret;
	mode_t mask;

	/* First idea was to printconfig both old and new, and run diff -u
	 * on them.  Simple and effective.  Does require diff and two files
	 * though.  For now, I guess we can live with that. */

	if (tmp == NULL)
		tmp = "/tmp";

	snprintf(patho, sizeof(patho), "%s/carbon-c-relay_route.XXXXXX", tmp);
	mask = umask(S_IWGRP | S_IWOTH);
	fd = mkstemp(patho);
	umask(mask);
	if (fd < 0) {
		logerr("failed to create temporary file: %s\n", strerror(errno));
		return 1;
	}
	f = fdopen(fd, "r+");
	if (f == NULL) {
		logerr("failed to open stream for file '%s': %s\n",
				patho, strerror(errno));
		return 1;
	}
	router_printconfig(old, f, PMODE_AGGR);
	fclose(f);

	snprintf(pathn, sizeof(pathn), "%s/carbon-c-relay_route.XXXXXX", tmp);
	mask = umask(S_IWGRP | S_IWOTH);
	fd = mkstemp(pathn);
	umask(mask);
	if (fd < 0) {
		logerr("failed to create temporary file: %s\n", strerror(errno));
		return 1;
	}
	f = fdopen(fd, "r+");
	if (f == NULL) {
		logerr("failed to open stream for file '%s': %s\n",
				pathn, strerror(errno));
		return 1;
	}
	router_printconfig(new, f, PMODE_AGGR);
	fclose(f);

	/* diff and print its output */
	snprintf(buf, sizeof(buf), "diff -u %s %s", patho, pathn);
	f = popen(buf, "r");
	while ((len = fread(buf, 1, sizeof(buf) - 1, f)) > 0) {
		if (fwrite(buf, len, 1, out) == 0)
			/* ignore */
			;
	}
	ret = pclose(f);

	unlink(patho);
	unlink(pathn);

	/* ret:
	 * 0 - no diffs
	 * 1 - diffs
	 * 2 - some error (file not found?)
	 * let's say that an error means potential difference
	 */
	return ret != 0;
}

/**
 * Evaluates all servers in new and if an identical server exists in
 * old, swaps their queues.
 */
void
router_transplant_queues(router *new, router *old)
{
	servers *os;
	servers *ns;

	for (ns = new->srvrs; ns != NULL; ns = ns->next) {
		for (os = old->srvrs; os != NULL; os = os->next) {
			if (strcmp(server_ip(ns->server), server_ip(os->server)) == 0 &&
					server_port(ns->server) == server_port(os->server) &&
					server_ctype(ns->server) == server_ctype(os->server))
			{
				server_swap_queue(ns->server, os->server);
				continue;
			}
		}
	}
}

/**
 * Starts all resources (servers) associated to this router.
 */
char
router_start(router *rtr)
{
	servers *s;
	char ret = 0;
	int err;

	for (s = rtr->srvrs; s != NULL; s = s->next) {
		if ((err = server_start(s->server)) != 0) {
			logerr("failed to start server %s:%u: %s\n",
					server_ip(s->server),
					server_port(s->server),
					strerror(err));
			ret = 1;
		}
	}

	return ret;
}

/**
 * Shuts down all resources (servers) associated to this router.
 */
void
router_shutdown(router *rtr)
{
	servers *s;

	for (s = rtr->srvrs; s != NULL; s = s->next)
		server_shutdown(s->server);
}

/**
 * Free the routes and all associated resources.
 */
void
router_free(router *rtr)
{
	servers *s;

	router_free_intern(rtr->clusters, rtr->routes);

	/* free all servers from the pool, in case of secondaries, the
	 * previous call to router_shutdown made sure nothing references the
	 * servers anymore */
	for (s = rtr->srvrs; s != NULL; s = s->next)
		server_free(s->server);

	ra_free(rtr);
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
	char firstspc = *firstspace;

	switch (r->matchtype) {
		case MATCHALL:
			ret = 1;
			break;
		case REGEX:
			*firstspace = '\0';
			ret = regexec(&r->rule, metric, r->nmatch, pmatch, 0) == 0;
			*firstspace = firstspc;
			break;
		case CONTAINS:
			*firstspace = '\0';
			ret = strstr(metric, r->strmatch) != NULL;
			*firstspace = firstspc;
			break;
		case STARTS_WITH:
			ret = strncmp(metric, r->strmatch, strlen(r->strmatch)) == 0;
			break;
		case ENDS_WITH:
			*firstspace = '\0';
			ret = strcmp(
					firstspace - strlen(r->strmatch),
					r->strmatch) == 0;
			*firstspace = firstspc;
			break;
		case MATCHES:
			*firstspace = '\0';
			ret = strcmp(metric, r->strmatch) == 0;
			*firstspace = firstspc;
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
				for (q = w->dests->cl->name, t = p;
						*q != '\0' && t >= metric;
						q++, t--)
				{
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
					case VALIDATION: {
						/* test whether data matches, if not, either log
						 * or drop and stop */
						char *lastchr = firstspace + strlen(firstspace) - 1;
						if (router_metric_matches(
									w->dests->cl->members.validation->rule,
									firstspace + 1,
									lastchr,
									pmatch))
							break;

						if (w->dests->cl->members.validation->action == VAL_LOG)
						{
							logerr("dropping metric due to validation error: "
									"%s", metric);
							wassent = 1;
						}

						/* only stop if this is a validate without
						 * destinations */
						stop |= d->next == NULL;
						/* break out of the dests loop */
						while (d->next != NULL)
							d = d->next;
						break;
					}
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
	char stop = 0;
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
			stop = w->stop;
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
					case VALIDATION: {
						char *lastspc = firstspace + strlen(firstspace);
						fprintf(stdout, "    validate\n        %s -> %s\n",
								d->cl->members.validation->rule->pattern,
								firstspace + 1);
						
						if (router_metric_matches(
									d->cl->members.validation->rule,
									firstspace + 1,
									lastspc,
									pmatch))
						{
							fprintf(stdout, "        match\n");
						} else {
							fprintf(stdout, "        fail -> %s\n",
									d->cl->members.validation->action == VAL_LOG ? "log" : "drop");
							stop |= d->next == NULL;
							while (d->next != NULL)
								d = d->next;
						}
					}	break;
					default: {
						fprintf(stdout, "    cluster(%s)\n", d->cl->name);
					}	break;
				}
			}
			if (stop) {
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
