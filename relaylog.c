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

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <unistd.h>
#include <time.h>
#include <assert.h>

#include "relay.h"

char relay_hostname[256];
enum rmode mode = NORMAL;

char *relay_logfile = NULL;
FILE *relay_stdout = NULL;
FILE *relay_stderr = NULL;
char relay_can_log = 0;

/**
 * Writes to the setup output stream, prefixed with a timestamp, and if
 * the stream is not going to stdout or stderr, prefixed with MSG or
 * ERR.
 */
int
relaylog(enum logdst dest, const char *fmt, ...)
{
	va_list ap;
	char prefix[64];
	size_t len;
	time_t now;
	struct tm *tm_now;
	FILE *dst = NULL;
	char console = 0;
	int ret;

	switch (dest) {
		case LOGOUT:
			dst = relay_stdout;
			if (dst == stdout)
				console = 1;
			break;
		case LOGERR:
			dst = relay_stderr;
			if (dst == stderr)
				console = 1;
			break;
	}
	assert(dst != NULL);

	/* briefly stall if we're swapping fds */
	while (!relay_can_log)
		usleep((100 + (rand() % 200)) * 1000);  /* 100ms - 300ms */

	time(&now);
	tm_now = localtime(&now);
	len = strftime(prefix, sizeof(prefix), "[%Y-%m-%d %H:%M:%S]", tm_now);

	if (!console)
		len += snprintf(prefix + len, sizeof(prefix) - len, " (%s)", dest == LOGOUT ? "MSG" : "ERR");

	fprintf(dst, "%s ", prefix);

	va_start(ap, fmt);

	ret = vfprintf(dst, fmt, ap);
	fflush(dst);

	va_end(ap);

	return ret;
}
