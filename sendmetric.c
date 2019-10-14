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
#include <sys/types.h>
#include <sys/un.h>
#include <netdb.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <getopt.h>

#include "config.h"

#define SERVER_MAX_SEND 10

#ifdef HAVE_GZIP
#include <zlib.h>
#endif

typedef struct _z_strm {
        ssize_t (*strmwrite)(struct _z_strm *, const void *, size_t);
        int (*strmflush)(struct _z_strm *);
        int (*strmclose)(struct _z_strm *);
        const char *(*strmerror)(struct _z_strm *, int);     /* get last err str */
		struct _z_strm *nextstrm;
		union {
			int sock;
#ifdef HAVE_GZIP
			z_streamp gz;
#endif
		} hdl;
        char *obuf;
        int obuflen;
        int obufsize;
} z_strm;


/* ordinary socket */
static int sockflush(z_strm *strm);

static inline ssize_t
sockwrite(z_strm *strm, const void *buf, size_t sze)
{
	/* ensure we have space available */
	if (strm->obuflen + sze > strm->obufsize)
		if (sockflush(strm) != 0)
			return -1;

	/* append metric to buf */
	memcpy(strm->obuf + strm->obuflen, buf, sze);
	strm->obuflen += sze;

	return sze;
}

static inline int
sockflush(z_strm *strm)
{
	ssize_t slen;
	size_t len;
	char *p;
	int cnt;
	if (strm->obuflen == 0)
		return 0;
	p = strm->obuf;
	len = strm->obuflen;
	/* Flush stream, this may not succeed completely due
	 * to flow control and whatnot, which the docs suggest need
	 * resuming to complete.  So, use a loop, but to avoid
	 * getting endlessly stuck on this, only try a limited
	 * number of times. */
	for (cnt = 0; cnt < SERVER_MAX_SEND; cnt++) {
		if ((slen = write(strm->hdl.sock, p, len)) != len) {
			if (slen >= 0) {
				p += slen;
				len -= slen;
			} else if (errno != EINTR)
				return -1;
		} else {
			strm->obuflen = 0;
			return 0;
		}
	}

	return -1;
}

static inline const char *
sockerror(z_strm *strm, int rval)
{
	return strerror(errno);
}

static inline int
sockclose(z_strm *strm)
{
	return close(strm->hdl.sock);
}

z_strm *socknew(size_t bufsize, int sock) {
	z_strm *strm = malloc(sizeof(z_strm));
	if (strm == NULL)
		return NULL;

	strm->obuf = malloc(bufsize);
	if (strm->obuf == NULL) {
		free(strm);
		return NULL;
	}

	strm->nextstrm = NULL;
	strm->hdl.sock = sock;

	strm->strmwrite = sockwrite;
	strm->strmflush = sockflush;
	strm->strmclose = sockclose;
	strm->strmerror = sockerror;

	return strm;
}

#ifdef HAVE_GZIP
/* gzip wrapped socket */

static inline int gzipflush(z_strm *strm);

static inline ssize_t
gzipwrite(z_strm *strm, const void *buf, size_t sze)
{
	/* ensure we have space available */
	if (strm->obuflen + sze > strm->obufsize)
			if (gzipflush(strm) != 0)
					return -1;

	/* append metric to buf */
	memcpy(strm->obuf + strm->obuflen, buf, sze);
	strm->obuflen += sze;

	return sze;
}

static inline int
gzipflush(z_strm *strm)
{
	int cret;
	char cbuf[8192];
	size_t cbuflen;
	char *cbufp;
	int oret;

	strm->hdl.gz->next_in = (Bytef *)strm->obuf;
	strm->hdl.gz->avail_in = strm->obuflen;
	strm->hdl.gz->next_out = (Bytef *)cbuf;
	strm->hdl.gz->avail_out = sizeof(cbuf);

	do {
			cret = deflate(strm->hdl.gz, Z_PARTIAL_FLUSH);
			if (cret == Z_OK && strm->hdl.gz->avail_out == 0) {
					/* too large output block, unlikely given the input, discard */
					/* TODO */
					break;
			}
			if (cret == Z_OK) {
					cbufp = cbuf;
					cbuflen = sizeof(cbuf) - strm->hdl.gz->avail_out;
					while (cbuflen > 0) {
							oret = strm->nextstrm->strmwrite(strm->nextstrm,
											cbufp, cbuflen);
							if (oret < 0)
									return -1;  /* failure is failure */

							/* update counters to possibly retry the remaining bit */
							cbufp += oret;
							cbuflen -= oret;
					}

					if (strm->hdl.gz->avail_in == 0)
							break;

					strm->hdl.gz->next_out = (Bytef *)cbuf;
					strm->hdl.gz->avail_out = sizeof(cbuf);
			} else {
					/* discard */
					break;
			}
	} while (1);
	/* flush whatever we wrote */
	strm->nextstrm->strmflush(strm->nextstrm);

	/* reset the write position, from this point it will always need to
	 * restart */
	strm->obuflen = 0;

	if (cret != Z_OK)
			return -1;  /* we must reset/free gzip */

	return 0;
}

static inline const char *
gziperror(z_strm *strm, int rval)
{
	return strm->nextstrm->strmerror(strm->nextstrm, rval);
}


static inline int
gzipclose(z_strm *strm)
{
	int ret = strm->nextstrm->strmclose(strm->nextstrm);
	deflateEnd(strm->hdl.gz);
	free(strm->hdl.gz);
	free(strm->obuf);
	return ret;
}

z_strm *gzipnew(size_t bufsize, int compression) {
	z_strm *strm = malloc(sizeof(z_strm));
	if (strm == NULL)
		return NULL;

	strm->obuf = malloc(bufsize);
	if (strm->obuf == NULL)
		return NULL;

	strm->hdl.gz = malloc(sizeof(z_stream));
	if (strm->hdl.gz == NULL) {
		free(strm->obuf);
		return NULL;
	}
	strm->hdl.gz->zalloc = Z_NULL;
	strm->hdl.gz->zfree = Z_NULL;
	strm->hdl.gz->opaque = Z_NULL;
	strm->hdl.gz->next_in = Z_NULL;
	if (deflateInit2(strm->hdl.gz,
						compression,
						Z_DEFLATED,
						15 + 16,
						8,
						Z_DEFAULT_STRATEGY) != Z_OK) {
		free(strm->obuf);
		free(strm->hdl.gz);
		free(strm);
		return NULL;
	}
	strm->obuflen = 0;

	strm->strmwrite = gzipwrite;
	strm->strmflush = gzipflush;
	strm->strmclose = gzipclose;
	strm->strmerror = gziperror;

	return strm;
};
#endif

/* simple utility to send metrics from stdin to a relay over a unix
 * socket */

#ifndef PF_LOCAL
# define PF_LOCAL PF_UNIX
#endif

#define S_LOCAL 1
#define S_TCP   (1 << 1)
#define S_UDP   (1 << 2)

#define S_PLAIN (1 << 3)
#define S_GZIP  (1 << 4)

struct conf {
	int stype;
	const char *spath;
	ssize_t bsize;
};

void do_usage(char *name, int exitcode)
{
        printf("Usage: %s [opts] <socket_path>\n", name);
        printf("\n");
        printf("Options:\n");
        printf("  -b bsize  Read blocksize, defaults to 8192\n");
#ifdef HAVE_GZIP
        printf("  -z        Compress output with zlib\n");
#endif
        exit(exitcode);
}

void get_options(int argc, char *argv[], struct conf *c) {
	int choice;
	int subtype  = S_PLAIN;
	while (1)
	{
		static const char *options = "b:ztuh";
		static const struct option long_options[] =
		{
			{ "bsize",  required_argument, 0, 'b' },
			{ "gzip",	no_argument,	   0, 'z'},
			{ "tcp",	no_argument,	   0, 't'},
			{ "udp",	no_argument,	   0, 'u'},
			{ "help",	no_argument,	   0, 'h'},
			{ 0, 0, 0, 0 }
		};

		int option_index = 0;
		choice = getopt_long(argc, argv, options,
					long_options, &option_index);

		if (choice == -1)
			break;

		switch(choice) {
			case 'b':
				c->bsize = atoi(optarg);
				if (c->bsize < 1) {
					fprintf(stderr, "bsize is incorrect: %s\n", optarg);
					do_usage(argv[0], 1);
				}
				break;
			case 'z':
				subtype = S_GZIP;
				break;
			case 't':
				c->stype = S_TCP;
				break;
			case 'u':
				c->stype = S_UDP;
				break;
			case 'h':
				do_usage(argv[0], 0);
				break;
			case 0:
				break;
			default:
				break;
   		}
		c->stype |= subtype;
	}

	if (optind != argc - 1) {
		do_usage(argv[0], 1);
	}
	c->spath = argv[optind];
}

#define GO_EXIT(code) do { rc = (code); goto EXIT; } while(0);

int main(int argc, char *argv[]) {
	int fd = -1;
	char *buf = NULL;
	size_t bread;
	int rc = 0;

	struct conf c;
	c.bsize = 8192;
	c.stype = S_LOCAL;
	c.spath = NULL;
	z_strm *strm = NULL;

	get_options(argc, argv, &c);

	buf = malloc(c.bsize);
	if (buf == NULL) {
		fprintf(stderr, "stream alloc error. %s\n", strerror(errno));
		GO_EXIT(-1);
	}

	if (c.stype & S_LOCAL) {
		struct sockaddr_un saddr;
		if ((fd = socket(PF_LOCAL, SOCK_STREAM, 0)) == -1) {
			fprintf(stderr, "failed to create socket: %s\n", strerror(errno));
			GO_EXIT(-1);
		}
		memset(&saddr, 0, sizeof(struct sockaddr_un));
		saddr.sun_family = PF_LOCAL;
		strncpy(saddr.sun_path, c.spath, sizeof(saddr.sun_path) - 1);

		if (connect(fd, (struct sockaddr *)&saddr, sizeof(saddr)) == -1) {
			fprintf(stderr, "failed to connect: %s\n", strerror(errno));
			GO_EXIT(-1);
		}
	} else {
		char *host;
		char *port;
		struct addrinfo hints;
		struct addrinfo *address;

		char *delim = strchr(c.spath, ':');
		if (delim == NULL || delim[1] == '\0') {
			fprintf(stderr, "address must be in format [host]:port\n");
			GO_EXIT(-1);
		}
		if (delim == c.spath) {
			host = strdup("127.0.0.1");
		} else {
			host = strndup(c.spath, delim - c.spath);
		}
		port = delim + 1;

		memset(&hints, 0, sizeof(hints));
		if (c.stype & S_TCP)
			hints.ai_socktype = SOCK_STREAM;
		else
			hints.ai_socktype = SOCK_DGRAM;
		if (getaddrinfo(host, port, &hints, &address)) {
			free(host);
			fprintf(stderr, "iresolve address failed. %s\n", strerror(errno));
			GO_EXIT(-1);
		}
		if ((fd = socket(address->ai_family, address->ai_socktype, address->ai_protocol)) == -1) {
			fprintf(stderr, "connect failed. %s\n", strerror(errno));
		}
		/* use first address only */
		else if (connect(fd, address->ai_addr, address->ai_addrlen) == -1) {
			fprintf(stderr, "connect failed. %s\n", strerror(errno));
			fd = -1;
		}
		free(host);
		freeaddrinfo(address);
		if (fd == -1)
			GO_EXIT(-1);
	}

	strm = socknew(c.bsize, fd);
	if (strm == NULL) {
		close(fd);
		fprintf(stderr, "stream alloc error. %s\n", strerror(errno));
		GO_EXIT(-1);
	}
#ifdef HAVE_GZIP
	if (c.stype & S_GZIP) {
		z_strm *zstrm = gzipnew(c.bsize, Z_DEFAULT_COMPRESSION);
		if (zstrm == NULL) {
			close(fd);
			fprintf(stderr, "gzip stream alloc error. %s\n", strerror(errno));
			GO_EXIT(-1);
		}
		zstrm->nextstrm = strm;
		strm = zstrm;
	}
#endif


	while ((bread = fread(buf, 1, c.bsize, stdin)) > 0) {
		/* garbage in/garbage out */
		if (strm->strmwrite(strm, buf, bread) != bread) {
			fprintf(stderr, "stream write error. %s\n", strerror(errno));
			GO_EXIT(-1);
		}
	}
	if (strm->strmflush(strm) == -1) {
		fprintf(stderr, "stream write error. %s\n", strerror(errno));
		GO_EXIT(-1);
	}
EXIT:
	if (strm != NULL) {
		strm->strmclose(strm);
		if (strm->nextstrm != NULL)
			free(strm->nextstrm);
		free(strm);
	}
	free(buf);
	return(rc);
}
