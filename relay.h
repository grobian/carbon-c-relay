/*
 * Copyright 2013-2015 Fabian Groffen
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

#define VERSION "0.39"

#define METRIC_BUFSIZ 8192

enum rmode { NORMAL, DEBUG, SUBMISSION, TEST };

typedef enum { CON_TCP, CON_UDP, CON_PIPE } serv_ctype;

extern char relay_hostname[];
extern enum rmode mode;

enum logdst { LOGOUT, LOGERR };

int relaylog(enum logdst dest, const char *fmt, ...);
#define logout(args...) relaylog(LOGOUT, args)
#define logerr(args...) relaylog(LOGERR, args)

#endif
