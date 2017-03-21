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
#include <netdb.h>
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
#include "allocator.h"
#include "relay.h"
#include "router.h"
#include "conffile.h"
#include "conffile.tab.h"

struct _router {
	cluster *clusters;
	route *routes;
	aggregator *aggregators;
	servers *srvrs;
	struct _router_collector {
		char *stub;
		int interval;
		char *prefix;
		col_mode mode;
	} collector;
	struct _router_parser_err {
		const char *msg;
		size_t line;
		size_t start;
		size_t stop;
	} parser_err;
	struct _router_conf {
		size_t queuesize;
		size_t batchsize;
		int maxstalls;
		unsigned short iotimeout;
		unsigned int sockbufsize;
	} conf;
	allocator *a;
};

/* declarations to avoid symbol/include hell when doing config.yy.h */
int router_yylex_init(void **);
void router_yy_scan_string(char *, void *);
int router_yylex_destroy(void *);

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

static char serverip_server[256];  /* RFC1035 = 250 */
static const char *
serverip(server *s)
{
	const char *srvr = server_ip(s);

	if (strchr(srvr, ':') != NULL) {
		snprintf(serverip_server, sizeof(serverip_server), "[%s]", srvr);
		return serverip_server;
	}

	return srvr;
}


/**
 * Callback for the bison parser for yyerror
 */
void
router_yyerror(
		void *locptr,
		void *s,
		router *r,
		allocator *ra,
		allocator *a,
		const char *msg)
{
	(void)s;
	ROUTER_YYLTYPE *locp = (ROUTER_YYLTYPE *)locptr;
	regex_t re;
	size_t nmatch = 3;
	regmatch_t pmatch[3];
	char buf1[METRIC_BUFSIZ];
	char buf2[METRIC_BUFSIZ];
	char *dummy;
	char (*sa)[METRIC_BUFSIZ];
	char (*sb)[METRIC_BUFSIZ];
	char (*st)[METRIC_BUFSIZ];

	if (r->parser_err.msg != NULL)
		return;

	/* clean up the "value" types */
	if (regcomp(&re, "cr(STRING|COMMENT|INTVAL)", REG_EXTENDED) != 0) {
		r->parser_err.msg = ra_strdup(a, msg);
		return;
	}
	snprintf(buf1, sizeof(buf1), "%s", msg);
	sa = &buf1;
	sb = &buf2;
	while (regexec(&re, *sa, nmatch, pmatch, 0) == 0) {
		dummy = *sa + strlen(*sa);
		if (router_rewrite_metric(sb, &dummy,
					*sa, dummy, "\\_1", nmatch, pmatch) == 0)
		{
			r->parser_err.msg = ra_strdup(a, msg);
			return;
		}
		st = sa;
		sa = sb;
		sb = st;
	}
	regfree(&re);

	/* clean up the keywords */
	if (regcomp(&re, "cr([A-Z]+)", REG_EXTENDED) != 0) {
		r->parser_err.msg = ra_strdup(a, msg);
		return;
	}
	while (regexec(&re, *sa, nmatch, pmatch, 0) == 0) {
		dummy = *sa + strlen(*sa);
		if (router_rewrite_metric(sb, &dummy,
					*sa, dummy, "'\\_1'", nmatch, pmatch) == 0)
		{
			r->parser_err.msg = ra_strdup(a, msg);
			return;
		}
		st = sa;
		sa = sb;
		sb = st;
	}
	regfree(&re);

	/* clean up bison speak */
	if (regcomp(&re, "\\$end", REG_EXTENDED) != 0) {
		r->parser_err.msg = ra_strdup(a, msg);
		return;
	}
	while (regexec(&re, *sa, nmatch, pmatch, 0) == 0) {
		dummy = *sa + strlen(*sa);
		if (router_rewrite_metric(sb, &dummy,
					*sa, dummy, "end of file", nmatch, pmatch) == 0)
		{
			r->parser_err.msg = ra_strdup(a, msg);
			return;
		}
		st = sa;
		sa = sb;
		sb = st;
	}
	regfree(&re);

	r->parser_err.msg = ra_strdup(a, *sa);
	if (locp != NULL) {
		r->parser_err.line = 1 + locp->first_line;
		r->parser_err.start = locp->first_column;
		if (locp->first_line == locp->last_line) {
			r->parser_err.stop = locp->last_column;
		} else {
			r->parser_err.stop = locp->first_column;
		}
	} else {
		r->parser_err.line = 0;
	}
}

/**
 * Parse ip string and validate it.  When it is a numeric address, write
 * the cannonical version in retip.
 */
char *
router_validate_address(
		router *rtr,
		char **retip, int *retport, void **retsaddr, void **rethint,
		char *ip, serv_ctype proto)
{
	int port = 2003;
	struct addrinfo *saddr = NULL;
	struct addrinfo hint;
	char sport[8];
	char hnbuf[256];
	char *p;
	char *lastcolon = NULL;

	for (p = ip; *p != '\0'; p++)
		if (*p == ':')
			lastcolon = p;

	if (*(p - 1) == ']')
		lastcolon = NULL;
	if (lastcolon != NULL) {
		char *endp = NULL;
		*lastcolon = '\0';
		port = (int)strtol(lastcolon + 1, &endp, 10);
		if (port < 1 || endp != p)
			return(ra_strdup(rtr->a, "invalid port number"));
	}
	if (*ip == '[') {
		ip++;
		if (lastcolon != NULL && *(lastcolon - 1) == ']') {
			*(lastcolon - 1) = '\0';
		} else if (lastcolon == NULL && *(p - 1) == ']') {
			*(p - 1) = '\0';
		} else {
			return(ra_strdup(rtr->a, "expected ']'"));
		}
	}

	/* try to see if this is a "numeric" IP address, in
	 * which case we take the cannonical representation so
	 * as to ensure (string) comparisons will match lateron */
	memset(&hint, 0, sizeof(hint));
	saddr = NULL;

	hint.ai_family = PF_UNSPEC;
	hint.ai_socktype = proto == CON_UDP ? SOCK_DGRAM : SOCK_STREAM;
	hint.ai_protocol = proto == CON_UDP ? IPPROTO_UDP : IPPROTO_TCP;
	hint.ai_flags = AI_NUMERICHOST | AI_NUMERICSERV;
	snprintf(sport, sizeof(sport), "%u", port);

	if (getaddrinfo(ip, sport, &hint, &saddr) != 0) {
		int err;
		/* now resolve this the normal way to check validity */
		hint.ai_flags = AI_NUMERICSERV;
		if ((err = getaddrinfo(ip, sport, &hint, &saddr)) != 0) {
			char errmsg[512];
			snprintf(errmsg, sizeof(errmsg), "failed to resolve %s, port %s, "
					"proto %s: %s", ip, sport, proto == CON_UDP ? "udp" : "tcp",
					gai_strerror(err));
			return(ra_strdup(rtr->a, errmsg));
		}

		/* no ra_malloc, it gets owned by server */
		*rethint = malloc(sizeof(hint));
		if (*rethint == NULL)
			return(ra_strdup(rtr->a, "out of memory copying hint structure"));
		memcpy(*rethint, &hint, sizeof(hint));
	} else {
		/* serialise the IP address, to make sure we use cannonical form
		 * which we can then string-match lateron (to do duplicate
		 * detection) */
		*rethint = NULL;
		if (saddr->ai_family == AF_INET) {
			if (inet_ntop(saddr->ai_family,
						&((struct sockaddr_in *)saddr->ai_addr)->sin_addr,
						hnbuf, sizeof(hnbuf)) != NULL)
				ip = ra_strdup(rtr->a, hnbuf);
		} else if (saddr->ai_family == AF_INET6) {
			if (inet_ntop(saddr->ai_family,
						&((struct sockaddr_in6 *)saddr->ai_addr)->sin6_addr,
						hnbuf, sizeof(hnbuf)) != NULL)
				ip = ra_strdup(rtr->a, hnbuf);
		}
	}

	*retip = ip;
	*retport = port;
	*retsaddr = saddr;
	return NULL;
}

/**
 * Parse path string and validate it can be written to.
 */
char *
router_validate_path(router *rtr, char *path)
{
	struct stat st;
	char fileexists = 1;
	FILE *probe;

	/* if the file doesn't exist, remove it after trying to create it */
	if (stat(path, &st) == -1)
		fileexists = 0;

	if ((probe = fopen(path, "w+")) == NULL) {
		char errbuf[512];
		snprintf(errbuf, sizeof(errbuf),
				"failed to open file '%s' for writing: %s",
				path, strerror(errno));
		return ra_strdup(rtr->a, errbuf);
	}
	fclose(probe);

	if (!fileexists)
		unlink(path);

	return NULL;
}

/**
 * Checks pat against regular expression syntax (or being '*') and
 * returns a route struct with the parsed info.  If retr is NULL a route
 * struct is allocated.
 */
char *
router_validate_expression(router *rtr, route **retr, char *pat, char subst)
{
	route *r = *retr;
	if (r == NULL) {
		r = *retr = ra_malloc(rtr->a, sizeof(route));
		if (r == NULL)
			return ra_strdup(rtr->a, "out of memory allocating route");
		r->next = NULL;
		r->dests = NULL;
	}
	if (strcmp(pat, "*") == 0) {
		r->pattern = NULL;
		r->strmatch = NULL;
		r->matchtype = MATCHALL;
	} else {
		int err = determine_if_regex(r, pat,
				REG_EXTENDED | (subst ? 0 : REG_NOSUB));
		if (err != 0) {
			char ebuf[512];
			size_t s = snprintf(ebuf, sizeof(ebuf),
					"invalid expression '%s': ", pat);
			regerror(err, &r->rule, ebuf + s, sizeof(ebuf) - s);
			return ra_strdup(rtr->a, ebuf);
		}
	}

	return NULL;
}

/**
 * Check if cluster is defined, and set the pointer to it.
 */
char *
router_validate_cluster(router *rtr, cluster **clptr, char *cl)
{
	cluster *w;

	/* lookup cluster */
	for (w = rtr->clusters; w != NULL; w = w->next) {
		if (w->type != GROUP &&
				w->type != AGGRSTUB &&
				w->type != STATSTUB &&
				w->type != AGGREGATION &&
				w->type != REWRITE &&
				w->type != VALIDATION &&
				strcmp(w->name, cl) == 0)
			break;
	}
	if (w == NULL)
		return ra_strdup(rtr->a, "unknown cluster");

	*clptr = w;
	return NULL;
}

/**
 * Adds a server to the chain in router, expands if necessary.
 */
char *
router_add_server(
		router *ret,
		char *ip,
		int port,
		char *inst,
		serv_ctype proto,
		struct addrinfo *saddrs,
		struct addrinfo *hint,
		char useall,
		cluster *cl)
{
	struct addrinfo *walk;
	char hnbuf[256];
	char errbuf[512];
	server *newserver;
	servers *w = NULL;

	walk = saddrs;  /* NULL if file */
	do {
		servers *s;

		if (useall) {
			/* serialise the IP address, to make the targets explicit
			 * (since we're expanding all A/AAAA records) */
			if (walk->ai_family == AF_INET) {
				if (inet_ntop(walk->ai_family,
							&((struct sockaddr_in *)walk->ai_addr)->sin_addr,
							hnbuf, sizeof(hnbuf)) != NULL)
					ip = hnbuf;
			} else if (walk->ai_family == AF_INET6) {
				if (inet_ntop(walk->ai_family,
							&((struct sockaddr_in6 *)walk->ai_addr)->sin6_addr,
							hnbuf, sizeof(hnbuf)) != NULL)
					ip = hnbuf;
			}
		}

		newserver = NULL;
		for (s = ret->srvrs; s != NULL; s = s->next) {
			if (strcmp(server_ip(s->server), ip) == 0 &&
					server_port(s->server) == port &&
					server_ctype(s->server) == proto)
			{
				newserver = s->server;
				s->refcnt++;
				break;
			}
		}
		if (newserver == NULL) {
			newserver = server_new(ip, (unsigned short)port,
					proto, saddrs, hint,
					ret->conf.queuesize, ret->conf.batchsize,
					ret->conf.maxstalls, ret->conf.iotimeout,
					ret->conf.sockbufsize);
			if (newserver == NULL) {
				freeaddrinfo(saddrs);
				if (hint)
					free(hint);
				snprintf(errbuf, sizeof(errbuf),
						"failed to add server %s:%d (%s) "
						"to cluster %s", ip, port,
						proto == CON_UDP ? "udp" :
						proto == CON_TCP ? "tcp" :
						proto == CON_FILE ? "file" : "???", cl->name);
				return ra_strdup(ret->a, errbuf);
			}
			if (ret->srvrs == NULL) {
				s = ret->srvrs = ra_malloc(ret->a, sizeof(servers));
			} else {
				for (s = ret->srvrs; s->next != NULL; s = s->next)
					;
				s = s->next = ra_malloc(ret->a, sizeof(servers));
			}
			s->next = NULL;
			s->refcnt = 1;
			s->server = newserver;
		}
		
		/* check instance matches the existing server */
		if (s->refcnt > 1) {
			char *sinst = server_instance(newserver);
			if (sinst == NULL && inst != NULL) {
				freeaddrinfo(saddrs);
				if (hint)
					free(hint);
				snprintf(errbuf, sizeof(errbuf),
						"cannot set instance '%s' for "
						"server %s:%d: server was previously "
						"defined without instance",
						inst, serverip(newserver),
						server_port(newserver));
				return ra_strdup(ret->a, errbuf);
			} else if (sinst != NULL && inst == NULL) {
				freeaddrinfo(saddrs);
				if (hint)
					free(hint);
				snprintf(errbuf, sizeof(errbuf),
						"cannot define server %s:%d without "
						"instance: server was previously "
						"defined with instance '%s'",
						serverip(newserver),
						server_port(newserver), sinst);
				return ra_strdup(ret->a, errbuf);
			} else if (sinst != NULL && inst != NULL &&
					strcmp(sinst, inst) != 0)
			{
				freeaddrinfo(saddrs);
				if (hint)
					free(hint);
				snprintf(errbuf, sizeof(errbuf),
						"cannot set instance '%s' for "
						"server %s:%d: server was previously "
						"defined with instance '%s'",
						inst, serverip(newserver),
						server_port(newserver), sinst);
				return ra_strdup(ret->a, errbuf);
			} /* else: sinst == inst == NULL */
		}
		if (inst != NULL)
			server_set_instance(newserver, inst);

		if (cl->type == CARBON_CH ||
				cl->type == FNV1A_CH ||
				cl->type == JUMP_CH)
		{
			if (cl->members.ch->servers == NULL) {
				cl->members.ch->servers = w =
					ra_malloc(ret->a, sizeof(servers));
			} else {
				for (w = cl->members.ch->servers; w->next != NULL; w = w->next)
					;
				w = w->next = ra_malloc(ret->a, sizeof(servers));
			}
			if (w == NULL) {
				snprintf(errbuf, sizeof(errbuf), "malloc failed for %s_ch %s",
						cl->type == CARBON_CH ? "carbon" :
						cl->type == FNV1A_CH ? "fnv1a" :
						"jump_fnv1a", ip);
				freeaddrinfo(saddrs);
				if (hint)
					free(hint);
				return ra_strdup(ret->a, errbuf);
			}
			w->next = NULL;
			w->server = newserver;
			cl->members.ch->ring = ch_addnode(
					cl->members.ch->ring,
					w->server);
			if (cl->members.ch->ring == NULL) {
				freeaddrinfo(saddrs);
				if (hint)
					free(hint);
				snprintf(errbuf, sizeof(errbuf),
						"failed to add server %s:%d "
						"to ring for cluster %s: out of memory",
						ip, port, cl->name);
				return ra_strdup(ret->a, errbuf);
			}
		} else if (cl->type == FORWARD ||
				cl->type == FILELOG ||
				cl->type == FILELOGIP)
		{
			if (cl->members.forward == NULL) {
				cl->members.forward = w =
					ra_malloc(ret->a, sizeof(servers));
			} else {
				for (w = cl->members.forward; w->next != NULL; w = w->next)
					;
				w = w->next = ra_malloc(ret->a, sizeof(servers));
			}
			if (w == NULL) {
				snprintf(errbuf, sizeof(errbuf), "malloc failed for %s %s",
						cl->type == FORWARD ? "forward" :
						cl->type == FILELOG ? "file" :
						"file ip", ip);
				freeaddrinfo(saddrs);
				if (hint)
					free(hint);
				return ra_strdup(ret->a, errbuf);
			}
			w->next = NULL;
			w->server = newserver;
		} else if (cl->type == ANYOF ||
				cl->type == FAILOVER)
		{
			if (cl->members.anyof == NULL) {
				cl->members.anyof = ra_malloc(ret->a, sizeof(serverlist));
				if (cl->members.anyof != NULL) {
					cl->members.anyof->list = w =
						ra_malloc(ret->a, sizeof(servers));
					cl->members.anyof->count = 1;
					cl->members.anyof->servers = NULL;
				}
			} else {
				for (w = cl->members.anyof->list; w->next != NULL; w = w->next)
					;
				w = w->next = ra_malloc(ret->a, sizeof(servers));
				cl->members.anyof->count++;
			}
			if (w == NULL) {
				snprintf(errbuf, sizeof(errbuf), "malloc failed for %s %s",
						cl->type == ANYOF ? "any_of" :
						"failover", ip);
				freeaddrinfo(saddrs);
				if (hint)
					free(hint);
				return ra_strdup(ret->a, errbuf);
			}
			w->next = NULL;
			w->server = newserver;

			if (s->refcnt > 1) {
				freeaddrinfo(saddrs);
				if (hint)
					free(hint);
				snprintf(errbuf, sizeof(errbuf),
						"cannot share server %s:%d with "
						"any_of/failover cluster '%s'",
						serverip(newserver),
						server_port(newserver),
						cl->name);
				return ra_strdup(ret->a, errbuf);
			}
		}

		walk = useall ? walk->ai_next : NULL;
	} while (walk != NULL);

	return NULL;
}

char *
router_add_cluster(router *r, cluster *cl)
{
	cluster *cw;
	cluster *last = NULL;
	servers *w;

	for (cw = r->clusters; cw != NULL; last = cw, cw = cw->next)
		if (cw->name != NULL && cl->name != NULL &&
				strcmp(cw->name, cl->name) == 0)
			return ra_strdup(r->a, "cluster with the same name already defined");
	if (last == NULL)
		last = r->clusters;
	last->next = cl;

	/* post checks/fixups */
	if (cl->type == ANYOF || cl->type == FAILOVER) {
		size_t i = 0;
		cl->members.anyof->servers =
			ra_malloc(r->a, sizeof(server *) * cl->members.anyof->count);
		if (cl->members.anyof->servers == NULL)
			return ra_strdup(r->a, "malloc failed for anyof servers");
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
		char errbuf[512];
		for (w = cl->members.ch->servers; w != NULL; w = w->next)
			i++;
		if (i < cl->members.ch->repl_factor) {
			snprintf(errbuf, sizeof(errbuf),
					"invalid cluster '%s': replication count (%u) is "
					"larger than the number of servers (%zu)\n",
					cl->name, cl->members.ch->repl_factor, i);
			return ra_strdup(r->a, errbuf);
		}
	}
	return NULL;
}

char *
router_add_route(router *rtr, route *rte)
{
	route *rw;
	route *last = NULL;
	route *matchallstop = NULL;

	for (rw = rtr->routes; rw != NULL; last = rw, rw = rw->next)
		if (rw->matchtype == MATCHALL && rw->stop)
			matchallstop = rw;
	if (last == NULL) {
		rtr->routes = rte;
		return NULL;
	}

#define matchtype(r) \
	r->dests->cl->type == AGGREGATION ? "aggregate" : \
	r->dests->cl->type == REWRITE ? "rewrite" : \
	"match"
	if (matchallstop != NULL) {
		logerr("warning: %s %s will never match "
				"due to preceding %s * ... stop\n",
				matchtype(rte), rte->pattern == NULL ? "*" : rte->pattern,
				matchtype(matchallstop));
	}
	last->next = rte;
	return NULL;
}

char *
router_add_aggregator(router *rtr, aggregator *a)
{
	aggregator *aw;
	aggregator *last = NULL;

	for (aw = rtr->aggregators; aw != NULL; last = aw, aw = aw->next)
		;
	if (last == NULL) {
		rtr->aggregators = a;
		return NULL;
	}
	last->next = a;
	return NULL;
}

char *
router_add_stubroute(
		router *rtr,
		enum clusttype type,
		cluster *w,
		destinations *dw)
{
	char stubname[48];
	route *m;
	destinations *d;
	cluster *cl;
	char *err;

	m = ra_malloc(rtr->a, sizeof(route));
	if (m == NULL)
		return ra_strdup(rtr->a, "malloc failed for stub route");
	m->pattern = NULL;
	m->strmatch = NULL;
	m->dests = dw;
	m->stop = 1;
	m->matchtype = MATCHALL;
	m->next = NULL;

	/* inject stub route for dests */
	d = ra_malloc(rtr->a, sizeof(destinations));
	if (d == NULL)
		return ra_strdup(rtr->a, "malloc failed for stub destinations");
	d->next = NULL;
	cl = d->cl = ra_malloc(rtr->a, sizeof(cluster));
	if (cl == NULL)
		return ra_strdup(rtr->a, "malloc failed for stub cluster");
	cl->name = NULL;
	cl->type = type;
	cl->members.routes = m;
	cl->next = NULL;
	err = router_add_cluster(rtr, cl);
	if (err != NULL)
		return err;

	if (type == AGGRSTUB) {
		snprintf(stubname, sizeof(stubname),
				STUB_AGGR "%p__", w->members.aggregation);
	} else if (type == STATSTUB) {
		snprintf(stubname, sizeof(stubname),
				STUB_STATS "%p__", rtr);
	} else {
		return ra_strdup(rtr->a, "unknown stub type");
	}
	m = ra_malloc(rtr->a, sizeof(route));
	if (m == NULL)
		return ra_strdup(rtr->a, "malloc failed for catch stub route");
	m->pattern = ra_strdup(rtr->a, stubname);
	m->strmatch = ra_strdup(rtr->a, stubname);
	if (m->pattern == NULL || m->strmatch == NULL)
		ra_strdup(rtr->a, "malloc failed for catch stub pattern");
	m->dests = d;
	m->stop = 1;
	m->matchtype = STARTS_WITH;
	/* enforce first match to avoid interference */
	m->next = rtr->routes;
	rtr->routes = m;

	if (type == AGGRSTUB) {
		aggregator_set_stub(w->members.aggregation, stubname);
	} else if (type == STATSTUB) {
		rtr->collector.stub = m->pattern;
	}

	return NULL;
}

char *
router_set_statistics(router *rtr, destinations *dsts)
{
	if (rtr->collector.stub != NULL)
		return ra_strdup(rtr->a,
				"duplicate 'send statistics to' not allowed, "
				"use multiple destinations instead");

	return router_add_stubroute(rtr, STATSTUB, NULL, dsts);
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
	struct stat st;
	router *ret = NULL;
	void *lptr = NULL;
	allocator *palloc;

	if (strchr(path, '*') || strchr(path, '?') ||
			(strchr(path, '[') && strchr(path, ']')))
	{
		/* include path is a glob pattern */
		glob_t globbuf;
		char *globpath;
		size_t i;

		if ((i = glob(path, 0, NULL, &globbuf)) != 0) {
			switch (i) {
				case GLOB_NOSPACE:
					/* since we don't set a limit, we won't
					 * reach it either */
					logerr("out of memory while globbing files for "
							"pattern '%s'\n", path);
					return NULL;
				case GLOB_ABORTED:
					/* we don't use a call-back, so this is
					 * a real error of some sort */
					logerr("unable to match any files from "
							"pattern '%s': %s\n",
							path, strerror(errno));
					return NULL;
				case GLOB_NOMATCH:
					/* we don't want to abort on failing globs */
					logout("warning: pattern '%s' did not match "
							"any files\n", path);
					return NULL;
			}
		}

		/* read all files matched by glob */
		ret = orig;
		for (i = 0; i < globbuf.gl_pathc; i++) {
			globpath = globbuf.gl_pathv[i];
			ret = router_readconfig(ret, globpath, queuesize,
					batchsize, maxstalls, iotimeout, sockbufsize);
			if (ret == NULL) {
				/* readconfig will have freed when it found the error */
				break;
			}
		}

		/* also after errors the globbuf structure is
		 * initialised and might need free-ing */
		globfree(&globbuf);

		return ret;
	} /* else: include path is a regular file path */

	/* if there is no config, don't try anything */
	if (stat(path, &st) == -1) {
		logerr("unable to stat file '%s': %s\n", path, strerror(errno));
		return NULL;
	}

	if (orig == NULL) {
		cluster *cl;
		char *err;

		/* get a return structure (and allocator) in place */
		if ((ret = malloc(sizeof(router))) == NULL) {
			logerr("malloc failed for router return struct\n");
			return NULL;
		}
		ret->a = ra_new();
		if (ret->a == NULL) {
			logerr("malloc failed for allocator\n");
			free(ret);
			return NULL;
		}
		ret->routes = NULL;
		ret->aggregators = NULL;
		ret->srvrs = NULL;
		ret->clusters = NULL;

		err = router_set_collectorvals(ret, 60, "carbon.relays.\\.1", CUM);
		if (err != NULL) {
			logerr("setcollectorvals: %s\n");
			router_free(ret);
			return NULL;
		}
		ret->collector.stub = NULL;

		ret->conf.queuesize = queuesize;
		ret->conf.batchsize = batchsize;
		ret->conf.maxstalls = maxstalls;
		ret->conf.iotimeout = iotimeout;
		ret->conf.sockbufsize = sockbufsize;

		/* create virtual blackhole cluster */
		cl = ra_malloc(ret->a, sizeof(cluster));
		if (cl == NULL) {
			logerr("malloc failed for blackhole cluster\n");
			router_free(ret);
			return NULL;
		}
		cl->name = ra_strdup(ret->a, "blackhole");
		cl->type = BLACKHOLE;
		cl->members.forward = NULL;
		cl->next = NULL;
		ret->clusters = cl;
	} else {
		ret = orig;
	}

	/* parser allocator for memory needs during parsing only */
	palloc = ra_new();
	if (palloc == NULL) {
		logerr("malloc failed for parse allocator\n");
		router_free(ret);
		return NULL;
	}

	if ((buf = ra_malloc(palloc, st.st_size + 1)) == NULL) {
		logerr("malloc failed for config file buffer\n");
		router_free(ret);
		return NULL;
	}

	if ((cnf = fopen(path, "r")) == NULL) {
		logerr("failed to open config file '%s': %s\n", path, strerror(errno));
		router_free(ret);
		return NULL;
	}

	while ((len = fread(buf + len, 1, st.st_size - len, cnf)) != 0)
		;
	buf[st.st_size] = '\0';
	fclose(cnf);

	if (router_yylex_init(&lptr) != 0) {
		logerr("lex init failed\n");
		router_free(ret);
		return NULL;
	}
	/* copies buf due to modifications, we need orig for error reporting */
	router_yy_scan_string(buf, lptr);
	ret->parser_err.msg = NULL;
	if (router_yyparse(lptr, ret, ret->a, palloc) != 0) {
		if (ret->parser_err.msg == NULL) {
			fprintf(stderr, "parsing %s failed\n", path);
		} else if (ret->parser_err.line != 0) {
			char *line;
			char *p;
			char *carets;
			size_t carlen;
			fprintf(stderr, "%s:%zd:%zd: %s\n", path, ret->parser_err.line,
					ret->parser_err.start, ret->parser_err.msg);
			/* get some relevant context from buff and put ^^^^ below it
			 * to point out the position of the error */
			line = buf;
			while (ret->parser_err.line-- > 1 && line != NULL) {
				line = strchr(line, '\n');
				if (line)
					line++;
			}
			if (line == NULL)
				line = buf;  /* fallback, but will cause misplacement */
			if ((p = strchr(line + ret->parser_err.stop, '\n')) != NULL)
				*p = '\0';
			carlen = ret->parser_err.stop - ret->parser_err.start + 1;
			carets = ra_malloc(palloc, carlen + 1);
			memset(carets, '^', carlen);
			carets[carlen] = '\0';
			fprintf(stderr, "%s\n", line);
			/* deal with tabs in the input */
			for (p = line; p - line < ret->parser_err.start; p++)
				if (*p != '\t')
					*p = ' ';
			*p = '\0';
			fprintf(stderr, "%s%s\n", line, carets);
		} else {
			fprintf(stderr, "%s: %s\n", path, ret->parser_err.msg);
		}
		router_yylex_destroy(lptr);
		router_free(ret);
		ra_free(palloc);
		return NULL;
	}
	router_yylex_destroy(lptr);
	ra_free(palloc);

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
router_optimise(router *r, int threshold)
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

	/* avoid optimising anything if it won't pay off, note that threshold
	 * can be negative, meaning it will never optimise */
	seq = 0;
	for (rwalk = r->routes; rwalk != NULL && seq < threshold; rwalk = rwalk->next)
		seq++;
	if (seq < threshold)
		return;
	tracef("triggering optimiser, seq: %zd, threshold: %d\n", seq, threshold);

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
			tracef("skipping %s\n", rwalk->pattern ? rwalk->pattern : "*");
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
			tracef("skipping unusable %s\n", p);
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
			tracef("skipping too small pattern %s from %s\n",
					b, rwalk->pattern);
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
		tracef("found pattern %s from %s\n", b, rwalk->pattern);

		/* at this point, b points to the tail block in reverse, see if
		 * we already had such tail in place */
		for (bwalk = bstart; bwalk != NULL; bwalk = bwalk->next) {
			if (bwalk->hash != bsum || strcmp(bwalk->pattern, b) != 0)
				continue;
			break;
		}
		if (bwalk == NULL) {
			tracef("creating new group %s for %s\n", b, rwalk->pattern);
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

		tracef("adding %s to existing group %s\n", rwalk->pattern, b);
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
	return rtr->collector.stub;
}

/**
 * Returns the interval in seconds for collector stats.
 */
inline int
router_getcollectorinterval(router *rtr)
{
	return rtr->collector.interval;
}

/**
 * Returns the interval in seconds for collector stats.
 */
inline char *
router_getcollectorprefix(router *rtr)
{
	return rtr->collector.prefix;
}

/**
 * Returns the mode (SUB or CUM) in which the collector should operate.
 */
inline col_mode
router_getcollectormode(router *rtr)
{
	return rtr->collector.mode;
}

/**
 * Sets the collector interval in seconds, the metric prefix string, and
 * the emission mode (cumulative or sum).
 */
inline char *
router_set_collectorvals(router *rtr, int intv, char *prefix, col_mode smode)
{
	if (intv > 0)
		rtr->collector.interval = intv;
	if (prefix != NULL) {
		regex_t re;
		char cprefix[METRIC_BUFSIZ];
		char *expr = "^(([^.]+)(\\..*)?)$";
		char *dummy = relay_hostname + strlen(relay_hostname);
		size_t nmatch = 3;
		regmatch_t pmatch[3];

		if (regcomp(&re, expr, REG_EXTENDED) != 0)
			return ra_strdup(rtr->a, "failed to compile hostname regexp");
		if (regexec(&re, relay_hostname, nmatch, pmatch, 0) != 0)
			return ra_strdup(rtr->a, "failed to execute hostname regext");
		if (router_rewrite_metric(
				&cprefix, &dummy,
				relay_hostname, dummy, prefix,
				nmatch, pmatch) == 0)
			return ra_strdup(rtr->a, "rewriting statistics prefix failed");
		regfree(&re);
		rtr->collector.prefix = ra_strdup(rtr->a, cprefix);
		if (rtr->collector.prefix == NULL)
			return ra_strdup(rtr->a, "out of memory");
	}
	rtr->collector.mode = smode;

	return NULL;
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

	/* start with configuration wise standard components */
	if (rtr->collector.interval > 0) {
		fprintf(f, "statistics\n    submit every %d seconds\n",
				rtr->collector.interval);
		if (rtr->collector.mode == SUB)
			fprintf(f, "    reset counters after interval\n");
		fprintf(f, "    prefix with %s\n", rtr->collector.prefix);
		if (rtr->collector.stub != NULL) {
			fprintf(f, "    send to");
			for (r = rtr->routes; r != NULL; r = r->next) {
				if (r->dests->cl->type == STATSTUB) {
					destinations *d = r->dests->cl->members.routes->dests;
					if (d->next == NULL) {
						fprintf(f, " %s", d->cl->name);
					} else {
						for ( ; d != NULL; d = d->next)
							fprintf(f, "\n        %s", d->cl->name);
					}
					fprintf(f, "\n    stop\n");
					break;
				}
			}
		}
		fprintf(f, "    ;\n");

		fprintf(f, "\n");
	}

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
						serverip(s->server), server_port(s->server), PPROTO);
		} else if (c->type == FILELOG || c->type == FILELOGIP) {
			fprintf(f, "    file%s\n", c->type == FILELOGIP ? " ip" : "");
			for (s = c->members.forward; s != NULL; s = s->next)
				fprintf(f, "        %s\n",
						serverip(s->server));
		} else if (c->type == ANYOF || c->type == FAILOVER) {
			fprintf(f, "    %s\n", c->type == ANYOF ? "any_of" : "failover");
			for (s = c->members.anyof->list; s != NULL; s = s->next)
				fprintf(f, "        %s:%d%s\n",
						serverip(s->server), server_port(s->server), PPROTO);
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
						serverip(s->server), server_port(s->server),
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
			if (pmode & PMODE_AGGR) {
				router srtr;
				memset(&srtr, 0, sizeof(srtr));
				srtr.routes = r->dests->cl->members.routes;
				/* recurse */
				router_printconfig(&srtr, f, pmode);
				fprintf(f, "# end of group '%s'\n", b);
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
					serverip(s->server),
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

	ra_free(rtr->a);
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
	enum rewrite_case { RETAIN, LOWER, UPPER,
	                    RETAIN_DOT, LOWER_DOT, UPPER_DOT } rcase = RETAIN;

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
				} else if (escape == 1 && *p == '.') {
					if (rcase == LOWER) {
						rcase = LOWER_DOT;
					} else if (rcase == UPPER) {
						rcase = UPPER_DOT;
					} else {
						rcase = RETAIN_DOT;
					}
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

									case RETAIN_DOT:
										while (q < t) {
											if (*q == '.')
												*s++ = '_';
											else
												*s++ = *q;
											q++;
										}
										break;
									case LOWER_DOT:
										while (q < t) {
											if (*q == '.')
												*s++ = '_';
											else
												*s++ = (char)tolower(*q);
											q++;
										}
										break;
									case UPPER_DOT:
										while (q < t) {
											if (*q == '.')
												*s++ = '_';
											else
												*s++ = (char)toupper(*q);
											q++;
										}
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
		}

		/* stop processing further rules if requested */
		if (stop)
			break;
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
									serverip(s->server), server_port(s->server));
					}	break;
					case FILELOG:
					case FILELOGIP: {
						servers *s;

						fprintf(stdout, "    file%s(%s)\n",
								d->cl->type == FILELOGIP ? " ip" : "",
								d->cl->name);
						for (s = d->cl->members.forward; s != NULL; s = s->next)
							fprintf(stdout, "        %s\n",
									serverip(s->server));
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
									serverip(dst[i].dest),
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
								serverip(d->cl->members.anyof->servers[hash]),
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
