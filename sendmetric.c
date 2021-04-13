/*
 * Copyright 2013-2021 Fabian Groffen
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

#include <signal.h>
#include <sys/stat.h>
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
#define SLEEP_US        10000

#ifdef HAVE_GZIP
#include <zlib.h>
#endif

#ifdef HAVE_LZ4
#include <lz4.h>
#include <lz4frame.h>
#endif

#ifdef HAVE_SNAPPY
#include <snappy-c.h>
#endif

#ifdef HAVE_SSL
#include <openssl/ssl.h>
#include <openssl/err.h>

char *sslCA = NULL;
char sslCAisdir = 0;
#endif

int ignore_sigpipe() {
	struct sigaction sa, osa;
	sa.sa_handler = SIG_IGN;
	sigemptyset(&sa.sa_mask);
	sa.sa_flags = 0;
	return sigaction(SIGPIPE, &sa, &osa);
}

char debug = 0;

#define F_SYNC 0
#define F_PART 1
#define F_NOAUTO (1 << 1)



typedef struct _z_strm {
	ssize_t (*strmwrite)(struct _z_strm *, const void *, size_t);
	int (*strmflush)(struct _z_strm *);
	int (*strmclose)(struct _z_strm *);
	const char *(*strmerror)(struct _z_strm *, int);     /* get last err str */
	struct _z_strm *nextstrm;
	/* transport stream (socket or SSL):
	 * F_SYNC - flush in one try
	 * F_PART - flush in 3 parts (half size)
	 *
	 * compress stream:
	 * F_SYNC - flush nextstrm when compressed strm flushed
	 * F_NOAUTO - flush only nextrm buffer full, need manual flush nextrm before close
	 */
	char mode;
#ifdef HAVE_SSL
	SSL_CTX *ctx;
#endif
	union {
		int sock;
#ifdef HAVE_SSL
		SSL *ssl;
#endif
#ifdef HAVE_GZIP
		struct {
			z_streamp gz;
			void *cbuf;
			size_t cbuflen;
			int error;
		} z;
#endif
#ifdef HAVE_LZ4
		struct {
			void *cbuf;
			size_t cbuflen;
			size_t error;
		} lz;
#endif
#ifdef HAVE_SNAPPY
/* #if defined(HAVE_GZIP) || defined(HAVE_LZ4) || defined(HAVE_SNAPPY) */
		struct {
			void *cbuf;
			size_t cbuflen;
			int error;
		} sn;
#endif
	} hdl;
	char *obuf;
	size_t obuflen;
	size_t obufsize;
} z_strm;


/* ordinary socket */
static int sockflush(struct _z_strm *);

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
	size_t plen;
	char *p;
	int cnt;
	if (strm->obuflen == 0)
		return 0;
	p = strm->obuf;
	len = strm->obuflen;
	if (strm->mode == F_PART)
		plen = len / 2 + len % 1;
	else
		plen = len;
	/* Flush stream, this may not succeed completely due
	 * to flow control and whatnot, which the docs suggest need
	 * resuming to complete.  So, use a loop, but to avoid
	 * getting endlessly stuck on this, only try a limited
	 * number of times. */
	for (cnt = 0; cnt < SERVER_MAX_SEND; cnt++) {
		if ((slen = write(strm->hdl.sock, p, plen)) > 0) {
			p += slen;
			len -= slen;
			if (len == 0) {
				if (debug)
					fprintf(stderr, "sockflush: %lu bytes\n", strm->obuflen);
				strm->obuflen = 0;
				return 0;
			}
			if (plen > len)
				plen = len;
			usleep(SLEEP_US);
		} else if (errno != EINTR) {
			return -1;
		}
	}
	errno = EBUSY;
	return -1;
}


static inline const char *
sockerror(z_strm *strm, int rval)
{
	return strerror(rval);
}

static inline int
sockclose(z_strm *strm)
{
	free(strm->obuf);
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
	strm->mode = F_SYNC;
#ifdef HAVE_SSL
	strm->ctx = NULL;
	strm->hdl.ssl = NULL;
#endif
	strm->hdl.sock = sock;

	strm->obuflen = 0;
	strm->obufsize = bufsize;

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
	char cbuf[8192];
	size_t cbuflen;
	char *cbufp;
	int oret;

	if (strm->obuflen == 0)
		return 0;

	strm->hdl.z.gz->next_in = (Bytef *)strm->obuf;
	strm->hdl.z.gz->avail_in = strm->obuflen;
	strm->hdl.z.gz->next_out = (Bytef *)cbuf;
	strm->hdl.z.gz->avail_out = sizeof(cbuf);

	do {
		strm->hdl.z.error = deflate(strm->hdl.z.gz, Z_PARTIAL_FLUSH);
		if (strm->hdl.z.error == Z_OK && strm->hdl.z.gz->avail_out == 0) {
			/* too large output block, unlikely given the input, discard */
			/* TODO */
			break;
		}
		if (strm->hdl.z.error == Z_OK) {
			cbufp = cbuf;
			cbuflen = sizeof(cbuf) - strm->hdl.z.gz->avail_out;
			while (cbuflen > 0) {
				oret = strm->nextstrm->strmwrite(strm->nextstrm,
							cbufp, cbuflen);
				if (oret < 0)
					return -1;  /* failure is failure */

				/* update counters to possibly retry the remaining bit */
				cbufp += oret;
				cbuflen -= oret;
			}

			if (strm->hdl.z.gz->avail_in == 0)
					break;

			strm->hdl.z.gz->next_out = (Bytef *)cbuf;
			strm->hdl.z.gz->avail_out = sizeof(cbuf);
		} else {
			return -1;
		}
	} while (1);

	/* reset the write position, from this point it will always need to
	 * restart */
	if (debug)
		fprintf(stderr, "gzipflush: %lu bytes\n", strm->obuflen);
	strm->obuflen = 0;
	if (strm->mode == F_NOAUTO)
		return 0;

	/* flush whatever we wrote */
	if (strm->nextstrm->strmflush(strm->nextstrm) == -1)
		return -1;

	if (strm->hdl.z.error != Z_OK)
		return -1;  /* we must reset/free gzip */

	return 0;
}


static const char *Z_UNKNOWN_ERR_STR = "gzip unknown error";
static const char *Z_STREAM_END_STR = "gzip stream end";
static const char *Z_NEED_DICT_STR = "need a dictionary";
static const char *Z_STREAM_ERROR_STR = "gzip stream error";
static const char *Z_DATA_ERROR_STR = "gzip data error";
static const char *Z_MEM_ERROR_STR = "gzip memory error";
static const char *Z_BUF_ERROR_STR = "gzip buffer error";
static const char *Z_VERSION_EROR_STR = "gzip version error";

static inline const char *
gziperror(z_strm *strm, int rval)
{
	if (strm->hdl.z.error == Z_OK || strm->hdl.z.error == Z_ERRNO)
		return strm->nextstrm->strmerror(strm->nextstrm, rval);
	else {
		switch(strm->hdl.z.error) {
			case Z_STREAM_END:
				return Z_STREAM_END_STR;
				break;
			case Z_NEED_DICT:
				return Z_NEED_DICT_STR;
				break;
			case Z_STREAM_ERROR:
				return Z_STREAM_ERROR_STR;
				break;
			case Z_DATA_ERROR:
				return Z_DATA_ERROR_STR;
				break;
			case Z_MEM_ERROR:
				return Z_MEM_ERROR_STR;
				break;
			case Z_BUF_ERROR:
				return Z_BUF_ERROR_STR;
				break;
			case Z_VERSION_ERROR:
				return Z_VERSION_EROR_STR;
				break;
			default:
				return Z_UNKNOWN_ERR_STR;
		}
	}
}

static inline int
gzipclose(z_strm *strm)
{
	int ret = strm->nextstrm->strmclose(strm->nextstrm);
	deflateEnd(strm->hdl.z.gz);
	free(strm->hdl.z.gz);
	free(strm->obuf);
	return ret;
}

z_strm *gzipnew(size_t bufsize, int compression) {
	z_strm *strm = malloc(sizeof(z_strm));
	if (strm == NULL)
		return NULL;

	strm->obuf = malloc(bufsize);
	if (strm->obuf == NULL) {
		free(strm);
		return NULL;
	}

	strm->hdl.z.gz = malloc(sizeof(z_stream));
	if (strm->hdl.z.gz == NULL) {
		free(strm->obuf);
		return NULL;
	}
#ifdef HAVE_SSL
	strm->ctx = NULL;
#endif
	strm->hdl.z.gz->zalloc = Z_NULL;
	strm->hdl.z.gz->zfree = Z_NULL;
	strm->hdl.z.gz->opaque = Z_NULL;
	strm->hdl.z.gz->next_in = Z_NULL;
	if (deflateInit2(strm->hdl.z.gz,
						compression,
						Z_DEFLATED,
						15 + 16,
						8,
						Z_DEFAULT_STRATEGY) != Z_OK) {
		free(strm->obuf);
		free(strm->hdl.z.gz);
		free(strm);
		return NULL;
	}

	strm->obufsize = bufsize;
	strm->obuflen = 0;

	strm->hdl.z.error = Z_OK;

	strm->strmwrite = gzipwrite;
	strm->strmflush = gzipflush;
	strm->strmclose = gzipclose;
	strm->strmerror = gziperror;

	return strm;
};
#endif

#ifdef HAVE_LZ4
/* lz4 wrapped socket */

static inline int lzflush(z_strm *strm);

static inline ssize_t
lzwrite(z_strm *strm, const void *buf, size_t sze)
{
	size_t towrite = sze;

	/* ensure we have space available */
	if (strm->obuflen + sze > strm->obufsize)
		if (lzflush(strm) != 0)
			return -1;

	/* use the same strategy as gzip: fill the buffer until space
	   runs out. we completely fill the output buffer before flushing */

	while (towrite > 0) {

		size_t avail = strm->obufsize - strm->obuflen;
		size_t copysize = towrite > avail ? avail : towrite;

		/* copy into the output buffer as much as we can */

		if (copysize > 0) {
			memcpy(strm->obuf + strm->obuflen, buf, copysize);
			strm->obuflen += copysize;
			towrite -= copysize;
			buf += copysize;
		}

		/* if output buffer is full & still have bytes to write, flush now */

		if (strm->obuflen == strm->obufsize && lzflush(strm) != 0) {
			return -1;
		}
	}

	return sze;
}

static inline int
lzflush(z_strm *strm)
{
	size_t ret;
	int oret;

	/* anything to do? */

	if (strm->obuflen == 0)
		return 0;

	/* the buffered data goes out as a single frame */

	ret = LZ4F_compressFrame(strm->hdl.lz.cbuf, strm->hdl.lz.cbuflen, strm->obuf, strm->obuflen, NULL);
	if (LZ4F_isError(ret)) {
		strm->hdl.lz.error = ret;
		return -1;
	}

	/* write and flush */
	if ((oret = strm->nextstrm->strmwrite(strm->nextstrm,
										  strm->hdl.lz.cbuf, ret)) < 0) {
		return oret;
	}
	if (debug)
		fprintf(stderr, "lzflush: %lu bytes\n", strm->obuflen);
	strm->obuflen = 0;
	if (strm->mode == F_NOAUTO)
		return 0;

	if (strm->nextstrm->strmflush(strm->nextstrm) == -1) {
		return -1;
	}
	return 0;
}

static inline int
lzclose(z_strm *strm)
{
	lzflush(strm);
	free(strm->obuf);
	free(strm->hdl.lz.cbuf);
	return strm->nextstrm->strmclose(strm->nextstrm);
}

static inline const char *
lzerror(z_strm *strm, int rval)
{
	if (LZ4F_isError(strm->hdl.lz.error)) {
		return LZ4F_getErrorName(strm->hdl.lz.error);
	} else {
		return strm->nextstrm->strmerror(strm->nextstrm, rval);
	}
}

z_strm *lznew(size_t bufsize) {
	z_strm *strm = malloc(sizeof(z_strm));
	if (strm == NULL) {
		return NULL;
	}
	strm->obuf = malloc(bufsize);
	if (strm->obuf == NULL) {
		free(strm);
		return NULL;
	}

	/* get the maximum size that should ever be required and allocate for it */
	strm->hdl.lz.cbuflen = LZ4F_compressFrameBound(bufsize, NULL);
	if ((strm->hdl.lz.cbuf = malloc(strm->hdl.lz.cbuflen)) == NULL) {
		free(strm->obuf);
		free(strm);
		return NULL;
	}
#ifdef HAVE_SSL
	strm->ctx = NULL;
#endif

	strm->obuflen = 0;
	strm->obufsize = bufsize;

	strm->hdl.lz.error = 0;

	strm->strmwrite = lzwrite;
	strm->strmflush = lzflush;
	strm->strmclose = lzclose;
	strm->strmerror = lzerror;

	return strm;
}
#endif

#ifdef HAVE_SNAPPY
/* snappy wrapped socket */
static inline int snappyflush(z_strm *strm);

static inline ssize_t
snappywrite(z_strm *strm, const void *buf, size_t sze)
{
	/* ensure we have space available */
	if (strm->obuflen > 0 && strm->obuflen + sze > strm->obuflen)
		if (snappyflush(strm) != 0)
			return -1;

	/* append metric to buf */
	memcpy(strm->obuf + strm->obuflen, buf, sze);
	strm->obuflen += sze;

	return sze;
}

static inline int
snappyflush(z_strm *strm)
{
	char *cbufp = strm->hdl.sn.cbuf;
	size_t cbuflen = strm->hdl.sn.cbuflen;
	ssize_t oret;
	int ret;

	ret = snappy_compress(strm->obuf, strm->obuflen, cbufp, &cbuflen);
	/* reset the write position, from this point it will always need to
	 * restart */

	if (ret != SNAPPY_OK) {
		strm->hdl.sn.error = ret;
		return -1;  /* we must reset/free snappy */
	}

	while (cbuflen > 0) {
		oret = strm->nextstrm->strmwrite(strm->nextstrm, cbufp, cbuflen);
		if (oret < 0)
			return -1;  /* failure is failure */
		/* update counters to possibly retry the remaining bit */
		cbufp += oret;
		cbuflen -= oret;
	}
	if (debug)
		fprintf(stderr, "lzflush: %lu bytes\n", strm->obuflen);
	strm->obuflen = 0;

	if (strm->mode == F_NOAUTO) {
		return 0;
	}

	if (strm->nextstrm->strmflush(strm->nextstrm) == 0) {
		return 0;
	} else
		return -1;
}

static inline int
snappyclose(z_strm *strm)
{
	free(strm->obuf);
	return strm->nextstrm->strmclose(strm->nextstrm);
}

static const char *SNAPPY_UNKNOWN_ERR_STR = "snappy unknown error";
static const char *SNAPPY_INVALID_INPUT_STR = "snappy invalid input";
static const char *SNAPPY_BUFFER_TOO_SMALL_STR = "snappy buffer to small";

static inline const char *
snappyerror(z_strm *strm, int rval)
{
	if (strm->hdl.sn.error == SNAPPY_OK) {
		return strm->nextstrm->strmerror(strm->nextstrm, rval);
	} else {
		switch (strm->hdl.sn.error) {
			case SNAPPY_INVALID_INPUT:
				return SNAPPY_INVALID_INPUT_STR;
				break;
			case SNAPPY_BUFFER_TOO_SMALL:
				return SNAPPY_BUFFER_TOO_SMALL_STR;
				break;
			default:
				return SNAPPY_UNKNOWN_ERR_STR;
		}
	}
}

z_strm *snappynew(size_t bufsize) {
	z_strm *strm = malloc(sizeof(z_strm));
	if (strm == NULL) {
		return NULL;
	}
	strm->obuf = malloc(bufsize);
	if (strm->obuf == NULL) {
		free(strm);
		return NULL;
	}

	/* get the maximum size that should ever be required and allocate for it */
	strm->hdl.sn.cbuflen = snappy_max_compressed_length(bufsize);
	if ((strm->hdl.sn.cbuf = malloc(strm->hdl.sn.cbuflen)) == NULL) {
		free(strm->obuf);
		free(strm);
		return NULL;
	}
#ifdef HAVE_SSL
	strm->ctx = NULL;
#endif

	strm->obuflen = 0;
	strm->obufsize = bufsize;

	strm->hdl.sn.error = SNAPPY_OK;

	strm->strmwrite = snappywrite;
	strm->strmflush = snappyflush;
	strm->strmclose = snappyclose;
	strm->strmerror = snappyerror;

	return strm;
}
#endif

#ifdef HAVE_SSL
/* (Open|Libre)SSL wrapped socket */
static inline int
sslflush(z_strm *strm);

static inline ssize_t
sslwrite(z_strm *strm, const void *buf, size_t sze)
{
	/* ensure we have space available */
	if (strm->obuflen + sze > strm->obufsize)
		if (sslflush(strm) != 0)
			return -1;

	/* append metric to buf */
	memcpy(strm->obuf + strm->obuflen, buf, sze);
	strm->obuflen += sze;

	return sze;
}

static inline int
sslflush(z_strm *strm)
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
		//if ((slen = write(strm->hdl.sock, p, len)) != len) {
		if ((slen = (ssize_t)SSL_write(strm->hdl.ssl, p, len)) != len) {
			if (slen > 0) {
				p += slen;
				len -= slen;
			} else if (slen == 0) {
				errno = EPIPE;
				return -1;
			} else if (errno != EINTR) {
				return -1;
			}
		} else {
			strm->obuflen = 0;
			return 0;
		}
	}

	return -1;
}

static inline int
sslclose(z_strm *strm)
{
	int ret = SSL_shutdown(strm->hdl.ssl);
	SSL_free(strm->hdl.ssl);
	free(strm->obuf);
	return ret;
}

static char _error_buf[256];
static inline const char *
sslerror(z_strm *strm, int rval)
{
	int err = SSL_get_error(strm->hdl.ssl, rval);
	switch (err) {
		case SSL_ERROR_NONE:
			if (errno != 0)
				snprintf(_error_buf, sizeof(_error_buf),
							"%d: OS error: %s", err, strerror(errno));
			else
				snprintf(_error_buf, sizeof(_error_buf),
							"%d: SSL_ERROR_NONE", err);
			break;
		case SSL_ERROR_ZERO_RETURN:
			snprintf(_error_buf, sizeof(_error_buf),
						"%d: TLS/SSL connection has been closed", err);
			break;
		case SSL_ERROR_WANT_READ:
		case SSL_ERROR_WANT_WRITE:
		case SSL_ERROR_WANT_CONNECT:
		case SSL_ERROR_WANT_ACCEPT:
			snprintf(_error_buf, sizeof(_error_buf),
						"%d: the read or write operation did not complete", err);
			break;
		case SSL_ERROR_WANT_X509_LOOKUP:
			snprintf(_error_buf, sizeof(_error_buf),
						"%d: call callback via SSL_CTX_set_client_cert_cb()", err);
			break;
#ifdef SSL_ERROR_WANT_ASYNC
		case SSL_ERROR_WANT_ASYNC:
			snprintf(_error_buf, sizeof(_error_buf),
						"%d: asynchronous engine is still processing data", err);
			break;
#endif
#ifdef SSL_ERROR_WANT_ASYNC_JOB
		case SSL_ERROR_WANT_ASYNC_JOB:
			snprintf(_error_buf, sizeof(_error_buf),
						"%d: no async jobs available in the pool", err);
			break;
#endif
		case SSL_ERROR_SYSCALL:
			snprintf(_error_buf, sizeof(_error_buf),
						"%d: I/O error: %s", err, strerror(errno));
			break;
		case SSL_ERROR_SSL:
			ERR_error_string_n(ERR_get_error(),
							_error_buf, sizeof(_error_buf));
			break;
	}
	return _error_buf;
}

/* run once for new z_strm init */
z_strm* sslnew(const char *sslca, char sslcaisdir)
{
	z_strm *strm = malloc(sizeof(z_strm));
	if (strm == NULL) {
		fprintf(stderr, "failed to alloc z_strm\n");
		return NULL;
	}

	/* create an auto-negotiate context */
	const SSL_METHOD *m = SSLv23_client_method();
	strm->ctx = SSL_CTX_new(m);
	if (strm->ctx == NULL) {
		char *err = ERR_error_string(ERR_get_error(), NULL);
		fprintf(stderr, "failed to SSL_CTX_new"
							"%s: %s\n", sslca, err);
		free(strm);
		return NULL;
	}

	if (sslca != NULL) {
		if (SSL_CTX_load_verify_locations(strm->ctx,
										   sslcaisdir ? NULL : sslca,
										   sslcaisdir ? sslCA : NULL) == 0)
		{
			SSL_CTX_free(strm->ctx);
			strm->ctx = NULL;
			char *err = ERR_error_string(ERR_get_error(), NULL);
			fprintf(stderr, "failed to load SSL verify locations from "
							"%s: %s\n", sslca, err);
			free(strm);
			return NULL;
		}
		SSL_CTX_set_verify(strm->ctx, SSL_VERIFY_PEER, NULL);
	}
	strm->hdl.ssl = NULL;

	strm->strmwrite = sslwrite;
	strm->strmflush = sslflush;
	strm->strmclose = sslclose;
	strm->strmerror = sslerror;

	return strm;
}

/* run for z_strm socket connect  */
int sslsetup(z_strm *strm, size_t bufsize, int sock, const char *hostname)
{
	int rv;

	strm->obuf = malloc(bufsize);
	if (strm->obuf == NULL) {
		fprintf(stderr, "failed to alloc z_strm obuf\n");
		free(strm);
		return -1;
	}
	strm->obufsize = bufsize;
	strm->obuflen = 0;

	strm->hdl.ssl = SSL_new(strm->ctx);
	if (strm->hdl.ssl == NULL) {
		fprintf(stderr, "failed to SSL_new: %s\n",
					ERR_reason_error_string(ERR_get_error()));
		free(strm->obuf);
		close(sock);
		return -1;
	}
	SSL_set_tlsext_host_name(strm->hdl.ssl, hostname);
	if (SSL_set_fd(strm->hdl.ssl, sock) == 0) {
		fprintf(stderr, "failed to SSL_set_fd: %s\n",
					ERR_reason_error_string(ERR_get_error()));
		SSL_free(strm->hdl.ssl);
		free(strm->obuf);
		close(sock);
		return -1;
	}

	if ((rv = SSL_connect(strm->hdl.ssl)) != 1) {
		fprintf(stderr, "failed to connect ssl stream: %s\n",
					sslerror(strm, rv));
		strm->strmclose(strm);
		return -1;
	}
	if (SSL_CTX_get_verify_mode(strm->ctx) != SSL_VERIFY_NONE &&
		(rv = SSL_get_verify_result(strm->hdl.ssl)) != X509_V_OK) {
		fprintf(stderr, "failed to verify ssl certificate: %s\n",
					sslerror(strm, rv));
		strm->strmclose(strm);
		return -1;
	}


	return 0;
}
#endif

/* simple utility to send metrics from stdin to a relay over a unix
* socket */

#ifndef PF_LOCAL
#define PF_LOCAL PF_UNIX
#endif

#define S_NONE  0

#define S_LOCAL 1
#define S_TCP   (1 << 1)
#define S_UDP   (1 << 2)

#define S_PLAIN (1 << 3)
#define S_SSL   (1 << 4)

#define C_NONE   0
#define C_GZIP   1
#define C_LZ4	 2
#define C_SNAPPY 3

#define F_NONE   0
#define F_IMMEDIATE 1

struct conf {
	int stype;
	int compress;
	const char *spath;
	ssize_t bsize;
	char flush_mode;
};

void do_usage(char *name, int exitcode)
{
        printf("Usage: %s [opts] <[socket_path | host:port]>\n", name);
        printf("\n");
        printf("Options:\n");
        printf("  -b bsize  Read blocksize, defaults to 8192\n");
        printf("  [-t | -u] use TCP or UDP instead of unix socket\n");

#if defined(HAVE_GZIP) || defined(HAVE_LZ4)
	    printf("  -p        Partial write from output buffers with delay (for fragmentation check)\n");
	    printf("  -n        No auto flush transport stream (sicket or SSL) when flush compressed stream\n");
	    printf("  -f        Flush after avery write\n");
	    printf("  -d        Debug\n");
        printf("  -c alg    Compress output with other algorithms:\n");
#ifdef HAVE_GZIP
        printf(" gzip\n");
#endif
#ifdef HAVE_LZ4
        printf(" lz4\n");
#endif
#ifdef HAVE_SNAPPY
        printf(" snappy\n");
#endif
        printf("\n");
#endif
#ifdef HAVE_SSL
        printf("  -s        Encrypt output with openssl (for unix and tcp sockets)\n");
        printf("  -C sslCA  Load SSL CA\n");
#endif
        exit(exitcode);
}

void get_options(int argc, char *argv[], struct conf *c) {
	int choice;
	int stype = S_PLAIN;
	c->stype = S_LOCAL;
	c->compress = C_NONE;
	c->bsize = 8192;
	c->spath = NULL;
	c->flush_mode = F_SYNC;
	while (1)
	{
		static const char *options = "b:c:tusC:pndh";
		static const struct option long_options[] =
		{
			{ "bsize",  required_argument, 0, 'b' },
			{ "compress",  required_argument, 0, 'c' },
			{ "tcp",	no_argument,	   0, 't'},
			{ "udp",	no_argument,	   0, 'u'},
			{ "ssl",	no_argument,	   0, 's'},
			{ "sslCA",	no_argument,	   0, 'C'},
			{ "partial",    no_argument,   0, 'p'},
			{ "noautflush",  no_argument,   0, 'n'},
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
			case 'c':
#ifdef HAVE_GZIP
				if (strcmp(optarg, "gzip") == 0)
					c->compress = C_GZIP;
				else
#endif
#ifdef HAVE_LZ4
				if (strcmp(optarg, "lz4") == 0)
					c->compress = C_LZ4;
				else
#endif
#ifdef HAVE_SNAPPY
				if (strcmp(optarg, "snappy") == 0)
					c->compress = C_SNAPPY;
				else
#endif
					do_usage(argv[0], 1);
				break;
			case 't':
				c->stype = S_TCP;
				break;
			case 'u':
				c->stype = S_UDP;
				break;
#ifdef HAVE_SSL
			case 's':
				stype = S_SSL;
				break;
			case 'C':
				sslCA = optarg;
				/* check if we can read the given CA before starting up and stuff */
				if (sslCA != NULL) {
					struct stat st;
					if (stat(sslCA, &st) == -1) {
						fprintf(stderr, "failed to open TLS/SSL CA file '%s': %s\n",
										sslCA, strerror(errno));
						exit(1);
				   }
				   if (S_ISDIR(st.st_mode))
					   sslCAisdir = 1;
				}
				break;
#endif
			case 'p':
				c->flush_mode = F_IMMEDIATE;
				break;
			case 'n':
				c->flush_mode = F_NOAUTO;
				break;
			case 'd':
				debug = 1;
				break;
			case 'h':
				do_usage(argv[0], 0);
				break;
			case 0:
				break;
			default:
				break;
   		}
	}

	if (c->stype == S_UDP && stype == S_SSL) {
		fprintf(stderr, "udp and ssl are incompatible\n");
		do_usage(argv[0], 1);
	}
	c->stype |= stype;

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
	z_strm *strm = NULL;

	get_options(argc, argv, &c);

	ignore_sigpipe();

	buf = malloc(c.bsize);
	if (buf == NULL) {
		fprintf(stderr, "stream alloc error. %s\n", strerror(errno));
		GO_EXIT(-1);
	}

#ifdef HAVE_SSL
	if (c.stype & S_SSL) {
		/* initialize openssl */
		if (SSL_library_init() < 0) {
			fprintf(stderr, "failed to init ssl\n");
			GO_EXIT(-1);
		}
		SSL_load_error_strings();
		OpenSSL_add_all_algorithms();
	}
#endif

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
			fprintf(stderr, "resolve address failed. %s\n", strerror(errno));
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

	if (c.stype & S_PLAIN) {
		strm = socknew(c.bsize, fd);
		if (strm == NULL) {
			close(fd);
			fprintf(stderr, "stream alloc error. %s\n", strerror(errno));
			GO_EXIT(-1);
		}
#ifdef HAVE_SSL
		strm->nextstrm = NULL;
#endif
	}
#ifdef HAVE_SSL
	else if (c.stype & S_SSL) {
		strm = sslnew(sslCA, sslCAisdir);
		if (strm == NULL) {
			close(fd);
			GO_EXIT(-1);
		}
#ifdef HAVE_SSL
		strm->nextstrm = NULL;
#endif
		char hostname[256];
		gethostname(hostname, sizeof(hostname)- 1);
		if (sslsetup(strm, c.bsize, fd, hostname) == -1) {
			if (strm->ctx != NULL)
				SSL_CTX_free(strm->ctx);
			free(strm);
			strm = NULL;
			GO_EXIT(-1);
		}
	}
#endif
	if (c.flush_mode == F_PART)
		strm->mode = F_PART;

#ifdef HAVE_GZIP
	if (c.compress == C_GZIP) {
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
#ifdef HAVE_LZ4
	if (c.compress == C_LZ4) {
		z_strm *zstrm = lznew(c.bsize);
		if (zstrm == NULL) {
			close(fd);
			fprintf(stderr, "lz4 stream alloc error. %s\n", strerror(errno));
			GO_EXIT(-1);
		}
		zstrm->nextstrm = strm;
		strm = zstrm;
	}
#endif
#ifdef HAVE_SNAPPY
	if (c.compress == C_SNAPPY) {
		z_strm *zstrm = snappynew(c.bsize);
		if (zstrm == NULL) {
			close(fd);
			fprintf(stderr, "snappy stream alloc error. %s\n", strerror(errno));
			GO_EXIT(-1);
		}
		zstrm->nextstrm = strm;
		strm = zstrm;
	}
#endif
	if (strm->nextstrm != NULL && c.flush_mode == F_NOAUTO)
		strm->mode = F_NOAUTO;

	while ((bread = fread(buf, 1, c.bsize, stdin)) > 0) {
		/* garbage in/garbage out */
		if (strm->strmwrite(strm, buf, bread) != bread) {
			fprintf(stderr, "stream write error. %s\n", strm->strmerror(strm, errno));
			GO_EXIT(-1);
		}
		if (strm->strmflush(strm) == -1) {
			fprintf(stderr, "stream write error. %s\n", strm->strmerror(strm, errno));
			GO_EXIT(-1);
		}
	}
	if (strm->strmflush(strm) == -1) {
		fprintf(stderr, "stream write error. %s\n", strm->strmerror(strm, errno));
		GO_EXIT(-1);
	}
	if (strm->mode == F_NOAUTO && strm->nextstrm != NULL && strm->nextstrm->strmflush(strm->nextstrm) == -1) {
		fprintf(stderr, "nextstream write error. %s\n", strm->strmerror(strm, errno));
		GO_EXIT(-1);
	}
EXIT:
	if (strm != NULL) {
		strm->strmclose(strm);
#ifdef HAVE_SSL
	if (strm->nextstrm != NULL && strm->nextstrm->ctx != NULL)
		SSL_CTX_free(strm->nextstrm->ctx);
	if (strm->ctx != NULL)
		SSL_CTX_free(strm->ctx);
#endif
		if (strm->nextstrm != NULL)
			free(strm->nextstrm);
		free(strm);
	}
	free(buf);
	return(rc);

}
