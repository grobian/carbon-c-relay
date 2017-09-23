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


#ifndef HAVE_RELAY_H
#define HAVE_RELAY_H 1

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#define METRIC_BUFSIZ 8192

/* these are the various modes in which the relay runs */
#define MODE_DEBUG      (1 << 0)
#define MODE_SUBMISSION (1 << 1)
#define MODE_TEST       (1 << 2)
#define MODE_DAEMON     (1 << 3)
#define MODE_TRACE      (1 << 4)
#define MODE_CONFIGTEST (1 << 5)
extern unsigned char mode;

#ifdef ENABLE_TRACE
#define tracef(...) if (mode & MODE_TRACE) { fprintf(stdout, __VA_ARGS__); fflush(stdout); }
#else
#define tracef(...) /* noop */
#endif

extern char relay_hostname[];

enum logdst { LOGOUT, LOGERR };

typedef enum { CON_TCP, CON_UDP, CON_PIPE, CON_FILE, CON_UNIX } con_proto;
typedef enum { T_LINEMODE } con_type;
typedef enum { W_PLAIN, W_GZIP, W_LZ4, W_SSL } con_trnsp;

int relaylog(enum logdst dest, const char *fmt, ...);
#define logout(args...) relaylog(LOGOUT, args)
#define logerr(args...) relaylog(LOGERR, args)

#define saddr_ntop(SADDR, BUF) { \
	BUF[0] = '\0'; \
	if ((SADDR)->ai_family == PF_INET) { \
		inet_ntop((SADDR)->ai_family, \
					&((struct sockaddr_in *)(SADDR)->ai_addr)->sin_addr, \
					BUF, sizeof(BUF)); \
	} else if ((SADDR)->ai_family == PF_INET6) { \
		inet_ntop((SADDR)->ai_family, \
					&((struct sockaddr_in6 *)(SADDR)->ai_addr)->sin6_addr, \
					BUF, sizeof(BUF)); \
	} \
}

#endif
