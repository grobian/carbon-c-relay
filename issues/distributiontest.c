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

/* This is a clumpsy program to test the distribution of a certain input
 * set of metrics.  See also:
 * https://github.com/graphite-project/carbon/issues/485
 *
 * compile using something like this:
 * clang -o distributiontest -I. issues/distributiontest.c consistent-hash.c \
 * server.c queue.c md5.c dispatcher.c router.c aggregator.c -pthread -lm */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <math.h>
#include <errno.h>

#include "consistent-hash.h"
#include "router.h"
#include "server.h"
#include "relay.h"
#include "md5.h"

#define SRVCNT 8
#define REPLCNT 2

unsigned char mode = 0;

int
relaylog(enum logdst dest, const char *fmt, ...)
{
	(void) dest;
	(void) fmt;
	return 0;
}

int main(int argc, char *argv[]) {
	FILE *f;
	int i, j;
	ch_ring *r;
	char buf[METRIC_BUFSIZ];
	server *s[SRVCNT];
	size_t scnt[SRVCNT];
	size_t metrics;
	time_t start, stop;
	size_t min, max;
	double mean, stddev;

	if (argc < 2) {
		fprintf(stderr, "need file argument\n");
		return 1;
	}
	if (strcmp(argv[1], "-") == 0) {
		f = stdin;
	} else {
		f = fopen(argv[1], "r");
	}
	if (f == NULL) {
		fprintf(stderr, "failed to open '%s': %s\n", argv[1], strerror(errno));
		return 1;
	}

	/* build hash-ring */
	r = ch_new(JUMP_FNV1a);  /* or CARBON, FNV1a */
	for (i = 0; i < SRVCNT; i++) {
		char ip[24];
		unsigned char md5[MD5_DIGEST_LENGTH];
		char md5sum[MD5_DIGEST_LENGTH * 2 + 1];
		snprintf(ip, sizeof(ip), "192.168.%d.%d", i, 10 + (i * 2));
		s[i] = server_new(
				ip,
				2003,
				CON_TCP,
				NULL,
				1024,
				128,
				4,
				800
				);
		MD5((unsigned char *)ip, strlen(ip), md5);
		for (j = 0; j < MD5_DIGEST_LENGTH; j++) {
			snprintf(md5sum + (j * 2), 3, "%02x", md5[j]);
		}
		/* to use instance, enable this */
		if (1 == 0)
			server_set_instance(s[i], md5sum);
		r = ch_addnode(r, s[i]);
	}

	/* process input */
	memset(scnt, 0, sizeof(size_t) * SRVCNT);
	metrics = 0;
	start = time(NULL);
	while (fgets(buf, METRIC_BUFSIZ, f) != NULL) {
		destination dst[REPLCNT];
		metrics++;
		ch_get_nodes(
				dst,
				r,
				REPLCNT,
				buf,
				buf + strlen(buf) - 1
				);
		for (i = 0; i < REPLCNT; i++) {
			free((void *)dst[i].metric);
			for (j = 0; j < SRVCNT; j++) {
				if (s[j] == dst[i].dest) {
					scnt[j]++;
					break;
				}
			}
		}
	}
	stop = time(NULL);

	mean = 0.0;
	stddev = 0.0;
	if (stop == start) {
		stop++;
	}
	printf("total metrics processed: %zd, time spent: ~%ds (~%d/s)\n",
			metrics, (int)(stop - start), (int)(metrics / (stop - start)));
	printf("replication count: %d, server count: %d\n", REPLCNT, SRVCNT);
	printf("server distribution:\n");
	for (i = 0; i < SRVCNT; i++) {
		printf("- server %15s: %6zd (%.2f%%)\n", server_ip(s[i]), scnt[i],
				((double)scnt[i] * 100.0) / (double)metrics);
		if (i == 0) {
			min = max = scnt[i];
		} else {
			if (scnt[i] < min)
				min = scnt[i];
			if (scnt[i] > max)
				max = scnt[i];
		}
		mean += (double)scnt[i];
	}
	mean /= (double)SRVCNT;
	for (i = 0; i < SRVCNT; i++) {
		stddev += pow((double)scnt[i] - mean, 2);
	}
	stddev = sqrt(stddev / (double)SRVCNT);
	printf("band: %zd - %zd (diff %zd, %.1f%%), mean: %.2f, stddev: %.2f\n",
			min, max, max - min,
			(double)(max-min) * 100.0 / (mean * SRVCNT),
			mean, stddev);
}
