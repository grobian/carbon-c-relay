/*
 * Copyright 2013-2019 Fabian Groffen
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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#ifndef HAVE_RTLD_NEXT
# error must have dlsym/RTLD_NEXT to fake time
#endif

#include <time.h>
#include <sys/time.h>
#include <dlfcn.h>

/* for repeatability, always return time starting from 1981-02-01 from the
 * first call to time().  This way we can control the aggregator time
 * buckets */
#define MYEPOCH  349830000

time_t (*orig_time)(time_t *tloc) = NULL;
int (*orig_gettimeofday)(struct timeval *tp, void *tz) = NULL;
time_t fake_offset = 0;

__attribute__((constructor)) void init(void) {
	orig_time = dlsym(RTLD_NEXT, "time");
	orig_gettimeofday = dlsym(RTLD_NEXT, "gettimeofday");
}

time_t time(time_t *tloc) {
	time_t now;

	(void)orig_time(&now);

	if (fake_offset == 0)
		fake_offset = now - MYEPOCH;

	now -= fake_offset;
	if (tloc)
		*tloc = now;
	return now;
}

int gettimeofday(struct timeval *tp, void *tzp) {
	/* some platforms, like Darwin and Solaris, have an implementation
	 * of gettimeofday that does not call time(), others, like glibc do
	 * which means we need to deal with that here */
	time_t now;

	int ret = orig_gettimeofday(tp, tzp);
	if (ret != 0)
		return ret;

	/* setup clock offset */
	now = time(NULL);
	now += fake_offset;

	if (now >= tp->tv_sec)
		tp->tv_sec -= fake_offset;

	return 0;
}
