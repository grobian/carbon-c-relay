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

#include <sys/socket.h>
#include <sys/un.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>

/* simple utility to send metrics from stdin to a relay over a unix
 * socket */

#ifndef PF_LOCAL
# define PF_LOCAL PF_UNIX
#endif

int main(int argc, char *argv[]) {
	char *spath;
	struct sockaddr_un saddr;
	int fd;
	char buf[8192];
	size_t bread;

	if (argc != 2) {
		fprintf(stderr, "need exactly one argument (unix socket path)\n");
		return(-1);
	}

	spath = argv[1];
	if ((fd = socket(PF_LOCAL, SOCK_STREAM, 0)) < 0) {
		fprintf(stderr, "failed to create socket: %s\n", strerror(errno));
		return(-1);
	}

	memset(&saddr, 0, sizeof(struct sockaddr_un));
	saddr.sun_family = PF_LOCAL;
	strncpy(saddr.sun_path, spath, sizeof(saddr.sun_path) - 1);

	if (connect(fd, (struct sockaddr *)&saddr, sizeof(saddr)) < -1) {
		fprintf(stderr, "failed to connect: %s\n", strerror(errno));
		return(-1);
	}

	while ((bread = fread(buf, 1, sizeof(buf), stdin)) > 0) {
		/* garbage in/garbage out
		 * this is a blocking write, so if it doesn't finish completely
		 * the remote end disconnected or something */
		if (write(fd, buf, bread) != bread) {
			fprintf(stderr, "failed to write %zd bytes: %s\n",
					bread, strerror(errno));
			return(-1);
		}
	}

	close(fd);
	return(0);
}
