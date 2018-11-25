/*
 * Copyright 2013-2018 Fabian Groffen
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

#include "relay.h"

#ifndef HAVE_RTLD_NEXT
# error must have dlsym/RTLD_NEXT to fake time
#endif

/* for repeatability, always return time starting from 1981-02-01 from the
 * first call to time().  This way we can control the aggregator time
 * buckets */
time_t time(time_t *tloc) {
	time_t now;

	(void)orig_time(&now);

	if (fake_offset == 0)
		fake_offset = now - 349830000;

	now -= fake_offset;
	if (tloc)
		*tloc = now;
	return now;
}
