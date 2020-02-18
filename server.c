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


#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <fcntl.h>
#include <pthread.h>
#include <poll.h>
#include <errno.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <netdb.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>
#include <sys/resource.h>
#include <assert.h>

#include "relay.h"
#include "queue.h"
#include "dispatcher.h"
#include "collector.h"
#include "server.h"

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
#endif

typedef struct _z_strm {
	ssize_t (*strmwrite)(struct _z_strm *, const void *, size_t);
	int (*strmflush)(struct _z_strm *);
	int (*strmclose)(struct _z_strm *);
	const char *(*strmerror)(struct _z_strm *, int);     /* get last err str */
	struct _z_strm *nextstrm;                            /* set when chained */
#if defined(HAVE_LZ4) || defined(HAVE_SNAPPY) || defined(HAVE_GZIP)
	char obuf[METRIC_BUFSIZ];
	int obuflen;
#endif
#ifdef HAVE_SSL
	SSL_CTX *ctx;
#endif
	union {
#ifdef HAVE_GZIP
		z_streamp gz;
#endif
#ifdef HAVE_LZ4
		struct {
			void *cbuf;
			size_t cbuflen;
		} z;
#endif
#ifdef HAVE_SSL
		SSL *ssl;
#endif
		int sock;
	} hdl;
} z_strm;

struct _server {
	const char *ip;
	unsigned short port;
	char *instance;
	struct addrinfo *saddr;
	struct addrinfo *hint;
	char reresolve:1;
	int fd;
	z_strm *strm;
	queue *queue;
	size_t bsize;
	short iotimeout;
	unsigned int sockbufsize;
	unsigned char maxstalls:SERVER_STALL_BITS;
	const char **batch;
	con_type type;
	con_trnsp transport;
	con_proto ctype;
	pthread_t tid;
	struct _server **secondaries;
	size_t secondariescnt;
	char failover:1;
	char failure;       /* full byte for atomic access */
	char running;       /* full byte for atomic access */
	char keep_running;  /* full byte for atomic access */
	unsigned char stallseq;  /* full byte for atomic access */
	size_t metrics;
	size_t dropped;
	size_t stalls;
	size_t ticks;
	size_t prevmetrics;
	size_t prevdropped;
	size_t prevstalls;
	size_t prevticks;
};


/* connection specific writers and closers */

/* ordinary socket */
static inline ssize_t
sockwrite(z_strm *strm, const void *buf, size_t sze)
{
	return write(strm->hdl.sock, buf, sze);
}

static inline int
sockflush(z_strm *strm)
{
	/* noop, we don't use a stream in the normal case */
	(void)strm;
	return 0;
}

static inline int
sockclose(z_strm *strm)
{
	return close(strm->hdl.sock);
}

static inline const char *
sockerror(z_strm *strm, int rval)
{
	(void)strm;
	(void)rval;
	return strerror(errno);
}

#ifdef HAVE_GZIP
/* gzip wrapped socket */
static inline int gzipflush(z_strm *strm);

static inline ssize_t
gzipwrite(z_strm *strm, const void *buf, size_t sze)
{
	/* ensure we have space available */
	if (strm->obuflen + sze > METRIC_BUFSIZ)
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
	char cbuf[METRIC_BUFSIZ];
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

static inline int
gzipclose(z_strm *strm)
{
	int ret = strm->nextstrm->strmclose(strm->nextstrm);
	deflateEnd(strm->hdl.gz);
	free(strm->hdl.gz);
	return ret;
}

static inline const char *
gziperror(z_strm *strm, int rval)
{
	return strm->nextstrm->strmerror(strm->nextstrm, rval);
}
#endif

#ifdef HAVE_LZ4
/* lz4 wrapped socket */

static inline int lzflush(z_strm *strm);

static inline ssize_t
lzwrite(z_strm *strm, const void *buf, size_t sze)
{
	size_t towrite = sze;

	/* use the same strategy as gzip: fill the buffer until space
	   runs out. we completely fill the output buffer before flushing */

	while (towrite > 0) {

		size_t avail = METRIC_BUFSIZ - strm->obuflen;
		size_t copysize = towrite > avail ? avail : towrite;

		/* copy into the output buffer as much as we can */

		if (copysize > 0) {
			memcpy(strm->obuf + strm->obuflen, buf, copysize);
			strm->obuflen += copysize;
			towrite -= copysize;
			buf += copysize;
		}

		/* if output buffer is full & still have bytes to write, flush now */

		if (strm->obuflen == METRIC_BUFSIZ && towrite > 0 && lzflush(strm) != 0) {
			logerr("Failed to flush LZ4 data to make space\n");
			return -1;
		}
	}

	return sze;
}

static inline int
lzflush(z_strm *strm)
{
	int oret;
	size_t ret;

	/* anything to do? */

	if (strm->obuflen == 0)
		return 0;

	/* the buffered data goes out as a single frame */

	ret = LZ4F_compressFrame(strm->hdl.z.cbuf, strm->hdl.z.cbuflen, strm->obuf, strm->obuflen, NULL);
	if (LZ4F_isError(ret)) {
		logerr("Failed to compress %d LZ4 bytes into a frame: %d\n",
		       strm->obuflen, (int)ret);
		return -1;
	}
		/* write and flush */

	if ((oret = strm->nextstrm->strmwrite(strm->nextstrm,
					      strm->hdl.z.cbuf, ret)) < 0) {
		logerr("Failed to write %lu bytes of compressed LZ4 data: %d\n",
		       ret, oret);
		return oret;
	}

	strm->nextstrm->strmflush(strm->nextstrm);

	/* the buffer is gone. reset the write position */

	strm->obuflen = 0;
	return 0;
}

static inline int
lzclose(z_strm *strm)
{
	lzflush(strm);
	free(strm->hdl.z.cbuf);
	return strm->nextstrm->strmclose(strm->nextstrm);
}

static inline const char *
lzerror(z_strm *strm, int rval)
{
	return strm->nextstrm->strmerror(strm->nextstrm, rval);
}
#endif

#ifdef HAVE_SNAPPY
/* snappy wrapped socket */
static inline int snappyflush(z_strm *strm);

static inline ssize_t
snappywrite(z_strm *strm, const void *buf, size_t sze)
{
	/* ensure we have space available */
	if (strm->obuflen + sze > METRIC_BUFSIZ)
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
	int cret;
	char cbuf[METRIC_BUFSIZ];
	size_t cbuflen = sizeof(cbuf);
	char *cbufp = cbuf;
	int oret;

	cret = snappy_compress(strm->obuf, strm->obuflen, cbuf, &cbuflen);
	/* reset the write position, from this point it will always need to
	 * restart */
	strm->obuflen = 0;

	if (cret != SNAPPY_OK)
		return -1;  /* we must reset/free snappy */
	while (cbuflen > 0) {
		oret = strm->nextstrm->strmwrite(strm->nextstrm, cbufp, cbuflen);
		if (oret < 0)
			return -1;  /* failure is failure */
		strm->nextstrm->strmflush(strm->nextstrm);
		/* update counters to possibly retry the remaining bit */
		cbufp += oret;
		cbuflen -= oret;
	}

	return 0;
}

static inline int
snappyclose(z_strm *strm)
{
	int ret = strm->nextstrm->strmclose(strm->nextstrm);
	return ret;
}

static inline const char *
snappyerror(z_strm *strm, int rval)
{
	return strm->nextstrm->strmerror(strm->nextstrm, rval);
}
#endif

#ifdef HAVE_SSL
/* (Open|Libre)SSL wrapped socket */
static inline ssize_t
sslwrite(z_strm *strm, const void *buf, size_t sze)
{
	return (ssize_t)SSL_write(strm->hdl.ssl, buf, (int)sze);
}

static inline int
sslflush(z_strm *strm)
{
	/* noop */
	(void)strm;
	return 0;
}

static inline int
sslclose(z_strm *strm)
{
	int sock = SSL_get_fd(strm->hdl.ssl);
	SSL_free(strm->hdl.ssl);
	return close(sock);
}

static char _sslerror_buf[256];
static inline const char *
sslerror(z_strm *strm, int rval)
{
	int err = SSL_get_error(strm->hdl.ssl, rval);
	switch (err) {
		case SSL_ERROR_NONE:
			snprintf(_sslerror_buf, sizeof(_sslerror_buf),
					"%d: SSL_ERROR_NONE", err);
			break;
		case SSL_ERROR_ZERO_RETURN:
			snprintf(_sslerror_buf, sizeof(_sslerror_buf),
					"%d: TLS/SSL connection has been closed", err);
			break;
		case SSL_ERROR_WANT_READ:
		case SSL_ERROR_WANT_WRITE:
		case SSL_ERROR_WANT_CONNECT:
		case SSL_ERROR_WANT_ACCEPT:
			snprintf(_sslerror_buf, sizeof(_sslerror_buf),
					"%d: the read or write operation did not complete", err);
			break;
		case SSL_ERROR_WANT_X509_LOOKUP:
			snprintf(_sslerror_buf, sizeof(_sslerror_buf),
					"%d: call callback via SSL_CTX_set_client_cert_cb()", err);
			break;
#ifdef SSL_ERROR_WANT_ASYNC
		case SSL_ERROR_WANT_ASYNC:
			snprintf(_sslerror_buf, sizeof(_sslerror_buf),
					"%d: asynchronous engine is still processing data", err);
			break;
#endif
#ifdef SSL_ERROR_WANT_ASYNC_JOB
		case SSL_ERROR_WANT_ASYNC_JOB:
			snprintf(_sslerror_buf, sizeof(_sslerror_buf),
					"%d: no async jobs available in the pool", err);
			break;
#endif
		case SSL_ERROR_SYSCALL:
			snprintf(_sslerror_buf, sizeof(_sslerror_buf),
					"%d: I/O error: %s", err, strerror(errno));
			break;
		case SSL_ERROR_SSL:
			ERR_error_string_n(ERR_get_error(),
					_sslerror_buf, sizeof(_sslerror_buf));
			break;
	}
	return _sslerror_buf;
}
#endif

/**
 * Reads from the queue and sends items to the remote server.  This
 * function is designed to be a thread.  Data sending is attempted to be
 * batched, but sent one by one to reduce loss on sending failure.
 * A connection with the server is maintained for as long as there is
 * data to be written.  As soon as there is none, the connection is
 * dropped if a timeout of DISCONNECT_WAIT_TIME exceeds.
 */
static void *
server_queuereader(void *d)
{
	server *self = (server *)d;
	size_t len;
	ssize_t slen;
	const char **metric = self->batch;
	struct timeval start, stop;
	struct timeval timeout;
	queue *squeue;
	char idle = 0;
	size_t *secpos = NULL;
	unsigned char cnt;
	const char *p;

	*metric = NULL;

#define FAIL_WAIT_TIME   6  /* 6 * 250ms = 1.5s */
#define DISCONNECT_WAIT_TIME   12  /* 12 * 250ms = 3s */
#define LEN_CRITICAL(Q)  (queue_free(Q) < self->bsize)
	self->running = 1;
	while (1) {
		if (queue_len(self->queue) == 0) {
			/* if we're idling, close the TCP connection, this allows us
			 * to reduce connections, while keeping the connection alive
			 * if we're writing a lot */
			gettimeofday(&start, NULL);
			if (self->ctype == CON_TCP && self->fd >= 0 &&
					idle++ > DISCONNECT_WAIT_TIME)
			{
				self->strm->strmclose(self->strm);
				self->fd = -1;
			}
			if (idle == 1)
				/* ensure blocks are pushed out as soon as we're idling,
				 * this allows compressors to benefit from a larger
				 * stream of data to gain better compression */
				self->strm->strmflush(self->strm);
			gettimeofday(&stop, NULL);
			__sync_add_and_fetch(&(self->ticks), timediff(start, stop));
			if (__sync_bool_compare_and_swap(&(self->keep_running), 0, 0))
				break;
			/* nothing to do, so slow down for a bit */
			usleep((200 + (rand() % 100)) * 1000);  /* 200ms - 300ms */
			/* if we are in failure mode, keep checking if we can
			 * connect, this avoids unnecessary queue moves */
			if (__sync_bool_compare_and_swap(&(self->failure), 0, 0))
				/* it makes no sense to try and do something, so skip */
				continue;
		} else if (self->secondariescnt > 0 &&
				(__sync_add_and_fetch(&(self->failure), 0) >= FAIL_WAIT_TIME ||
				 (!self->failover && LEN_CRITICAL(self->queue))))
		{
			size_t i;

			gettimeofday(&start, NULL);
			if (self->secondariescnt > 0) {
				if (secpos == NULL) {
					secpos = malloc(sizeof(size_t) * self->secondariescnt);
					if (secpos == NULL) {
						logerr("server: failed to allocate memory "
								"for secpos\n");
						gettimeofday(&stop, NULL);
						__sync_add_and_fetch(&(self->ticks),
								timediff(start, stop));
						continue;
					}
					for (i = 0; i < self->secondariescnt; i++)
						secpos[i] = i;
				}
				if (!self->failover) {
					/* randomise the failover list such that in the
					 * grand scheme of things we don't punish the first
					 * working server in the list to deal with all
					 * traffic meant for a now failing server */
					for (i = 0; i < self->secondariescnt; i++) {
						size_t n = rand() % (self->secondariescnt - i);
						if (n != i) {
							size_t t = secpos[n];
							secpos[n] = secpos[i];
							secpos[i] = t;
						}
					}
				}
			}

			/* offload data from our queue to our secondaries
			 * when doing so, observe the following:
			 * - avoid nodes that are in failure mode
			 * - avoid nodes which queues are >= critical_len
			 * when no nodes remain given the above
			 * - send to nodes which queue size < critical_len
			 * where there are no such nodes
			 * - do nothing (we will overflow, since we can't send
			 *   anywhere) */
			*metric = NULL;
			squeue = NULL;
			for (i = 0; i < self->secondariescnt; i++) {
				/* both conditions below make sure we skip ourself */
				if (__sync_add_and_fetch(
							&(self->secondaries[secpos[i]]->failure), 0))
					continue;
				squeue = self->secondaries[secpos[i]]->queue;
				if (!self->failover && LEN_CRITICAL(squeue)) {
					squeue = NULL;
					continue;
				}
				if (*metric == NULL) {
					/* send up to batch size of our queue to this queue */
					len = queue_dequeue_vector(
							self->batch, self->queue, self->bsize);
					self->batch[len] = NULL;
					metric = self->batch;
				}

				for (; *metric != NULL; metric++)
					if (!queue_putback(squeue, *metric))
						break;
				/* try to put back stuff that didn't fit */
				for (; *metric != NULL; metric++)
					if (!queue_putback(self->queue, *metric))
						break;
			}
			for (; *metric != NULL; metric++) {
				if (mode & MODE_DEBUG)
					logerr("dropping metric: %s", *metric);
				free((char *)*metric);
				__sync_add_and_fetch(&(self->dropped), 1);
			}
			gettimeofday(&stop, NULL);
			__sync_add_and_fetch(&(self->ticks),
					timediff(start, stop));
			if (squeue == NULL) {
				/* we couldn't do anything, take it easy for a bit */
				if (__sync_add_and_fetch(&(self->failure), 0) > 1) {
					/* This is a compound because I can't seem to figure
					 * out how to atomically just "set" a variable.
					 * It's not bad when in the middle there is a ++,
					 * all that counts is that afterwards its > 0. */
					__sync_and_and_fetch(&(self->failure), 0);
					__sync_add_and_fetch(&(self->failure), 1);
				}
				if (__sync_bool_compare_and_swap(&(self->keep_running), 0, 0))
					break;
				usleep((200 + (rand() % 100)) * 1000);  /* 200ms - 300ms */
			}
		} else if (__sync_add_and_fetch(&(self->failure), 0) > 0) {
			if (__sync_bool_compare_and_swap(&(self->keep_running), 0, 0))
				break;
			usleep((200 + (rand() % 100)) * 1000);  /* 200ms - 300ms */
			/* avoid overflowing */
			if (__sync_add_and_fetch(&(self->failure), 0) > FAIL_WAIT_TIME)
				__sync_sub_and_fetch(&(self->failure), 1);
		}

		/* at this point we've got work to do, if we're instructed to
		 * shut down, however, try to get everything out of the door
		 * (until we fail, see top of this loop) */

		gettimeofday(&start, NULL);

		/* try to connect */
		if (self->fd < 0) {
			if (self->reresolve) {  /* can only be CON_UDP/CON_TCP */
				struct addrinfo *saddr;
				char sport[8];

				/* re-lookup the address info, if it fails, stay with
				 * whatever we have such that resolution errors incurred
				 * after starting the relay won't make it fail */
				freeaddrinfo(self->saddr);
				snprintf(sport, sizeof(sport), "%u", self->port);
				if (getaddrinfo(self->ip, sport, self->hint, &saddr) == 0) {
					self->saddr = saddr;
				} else {
					if (__sync_fetch_and_add(&(self->failure), 1) == 0)
						logerr("failed to resolve %s:%u, server unavailable\n",
								self->ip, self->port);
					self->saddr = NULL;
					/* this will break out below */
				}
			}

			if (self->ctype == CON_PIPE) {
				int intconn[2];
				if (pipe(intconn) < 0) {
					if (__sync_fetch_and_add(&(self->failure), 1) == 0)
						logerr("failed to create pipe: %s\n", strerror(errno));
					continue;
				}
				dispatch_addconnection(intconn[0], NULL);
				self->fd = intconn[1];
			} else if (self->ctype == CON_FILE) {
				if ((self->fd = open(self->ip,
								O_WRONLY | O_APPEND | O_CREAT,
								S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH)) < 0)
				{
					if (__sync_fetch_and_add(&(self->failure), 1) == 0)
						logerr("failed to open file '%s': %s\n",
								self->ip, strerror(errno));
					continue;
				}
			} else if (self->ctype == CON_UDP) {
				struct addrinfo *walk;

				for (walk = self->saddr; walk != NULL; walk = walk->ai_next) {
					if ((self->fd = socket(walk->ai_family,
									walk->ai_socktype,
									walk->ai_protocol)) < 0)
					{
						if (walk->ai_next == NULL &&
								__sync_fetch_and_add(&(self->failure), 1) == 0)
							logerr("failed to create udp socket: %s\n",
									strerror(errno));
						continue;
					}
					if (connect(self->fd, walk->ai_addr, walk->ai_addrlen) < 0)
					{
						if (walk->ai_next == NULL &&
								__sync_fetch_and_add(&(self->failure), 1) == 0)
							logerr("failed to connect udp socket: %s\n",
									strerror(errno));
						close(self->fd);
						self->fd = -1;
						continue;
					}

					/* we made it up here, so this connection is usable */
					break;
				}
				/* if this didn't resolve to anything, treat as failure */
				if (self->saddr == NULL)
					__sync_add_and_fetch(&(self->failure), 1);
				/* if all addrinfos failed, try again later */
				if (self->fd < 0)
					continue;
			} else {  /* CON_TCP */
				int ret;
				int args;
				struct addrinfo *walk;

				for (walk = self->saddr; walk != NULL; walk = walk->ai_next) {
					if ((self->fd = socket(walk->ai_family,
									walk->ai_socktype,
									walk->ai_protocol)) < 0)
					{
						if (walk->ai_next == NULL &&
								__sync_fetch_and_add(&(self->failure), 1) == 0)
							logerr("failed to create socket: %s\n",
									strerror(errno));
						continue;
					}

					/* put socket in non-blocking mode such that we can
					 * poll() (time-out) on the connect() call */
					args = fcntl(self->fd, F_GETFL, NULL);
					if (fcntl(self->fd, F_SETFL, args | O_NONBLOCK) < 0) {
						logerr("failed to set socket non-blocking mode: %s\n",
								strerror(errno));
						close(self->fd);
						self->fd = -1;
						continue;
					}
					ret = connect(self->fd, walk->ai_addr, walk->ai_addrlen);

					if (ret < 0 && errno == EINPROGRESS) {
						/* wait for connection to succeed if the OS thinks
						 * it can succeed */
						struct pollfd ufds[1];
						ufds[0].fd = self->fd;
						ufds[0].events = POLLIN | POLLOUT;
						ret = poll(ufds, 1, self->iotimeout + (rand() % 100));
						if (ret == 0) {
							/* time limit expired */
							if (walk->ai_next == NULL &&
									__sync_fetch_and_add(
										&(self->failure), 1) == 0)
								logerr("failed to connect() to "
										"%s:%u: Operation timed out\n",
										self->ip, self->port);
							close(self->fd);
							self->fd = -1;
							continue;
						} else if (ret < 0) {
							/* some select error occurred */
							if (walk->ai_next &&
									__sync_fetch_and_add(
										&(self->failure), 1) == 0)
								logerr("failed to poll() for %s:%u: %s\n",
										self->ip, self->port, strerror(errno));
							close(self->fd);
							self->fd = -1;
							continue;
						} else {
							if (ufds[0].revents & POLLHUP) {
								if (walk->ai_next == NULL &&
										__sync_fetch_and_add(
											&(self->failure), 1) == 0)
									logerr("failed to connect() for %s:%u: "
											"Connection refused\n",
											self->ip, self->port);
								close(self->fd);
								self->fd = -1;
								continue;
							}
						}
					} else if (ret < 0) {
						if (walk->ai_next == NULL &&
								__sync_fetch_and_add(&(self->failure), 1) == 0)
						{
							logerr("failed to connect() to %s:%u: %s\n",
									self->ip, self->port, strerror(errno));
							dispatch_check_rlimit_and_warn();
						}
						close(self->fd);
						self->fd = -1;
						continue;
					}

					/* make socket blocking again */
					if (fcntl(self->fd, F_SETFL, args) < 0) {
						logerr("failed to remove socket non-blocking "
								"mode: %s\n", strerror(errno));
						close(self->fd);
						self->fd = -1;
						continue;
					}

					/* disable Nagle's algorithm, issue #208 */
					args = 1;
					if (setsockopt(self->fd, IPPROTO_TCP, TCP_NODELAY,
								&args, sizeof(args)) != 0)
						; /* ignore */

#ifdef TCP_USER_TIMEOUT
					/* break out of connections when no ACK is being
					 * received for +- 10 seconds instead of
					 * retransmitting for +- 15 minutes available on
					 * linux >= 2.6.37
					 * the 10 seconds is in line with the SO_SNDTIMEO
					 * set on the socket below */
					args = 10000 + (rand() % 300);
					if (setsockopt(self->fd, IPPROTO_TCP, TCP_USER_TIMEOUT,
								&args, sizeof(args)) != 0)
						; /* ignore */
#endif

					/* if we reached up here, we're good to go, so don't
					 * continue with the other addrinfos */
					break;
				}
				/* if this didn't resolve to anything, treat as failure */
				if (self->saddr == NULL)
					__sync_add_and_fetch(&(self->failure), 1);
				/* all available addrinfos failed on us */
				if (self->fd < 0)
					continue;
			}

			/* ensure we will break out of connections being stuck more
			 * quickly than the kernel would give up */
			timeout.tv_sec = 10;
			timeout.tv_usec = (rand() % 300) * 1000;
			if (setsockopt(self->fd, SOL_SOCKET, SO_SNDTIMEO,
						&timeout, sizeof(timeout)) != 0)
				; /* ignore */
			if (self->sockbufsize > 0)
				if (setsockopt(self->fd, SOL_SOCKET, SO_SNDBUF,
							&self->sockbufsize, sizeof(self->sockbufsize)) != 0)
					; /* ignore */
#ifdef SO_NOSIGPIPE
			if (self->ctype == CON_TCP || self->ctype == CON_UDP) {
				int enable = 1;
				if (setsockopt(self->fd, SOL_SOCKET, SO_NOSIGPIPE,
							(void *)&enable, sizeof(enable)) != 0)
					logout("warning: failed to ignore SIGPIPE on socket: %s\n",
							strerror(errno));
			}
#endif

#ifdef HAVE_GZIP
			if ((self->transport & 0xFFFF) == W_GZIP) {
				self->strm->hdl.gz = malloc(sizeof(z_stream));
				if (self->strm->hdl.gz != NULL) {
					self->strm->hdl.gz->zalloc = Z_NULL;
					self->strm->hdl.gz->zfree = Z_NULL;
					self->strm->hdl.gz->opaque = Z_NULL;
					self->strm->hdl.gz->next_in = Z_NULL;
				}
				if (self->strm->hdl.gz == NULL ||
						deflateInit2(self->strm->hdl.gz,
							Z_DEFAULT_COMPRESSION,
							Z_DEFLATED,
							15 + 16,
							8,
							Z_DEFAULT_STRATEGY) != Z_OK)
				{
					logerr("failed to open gzip stream: out of memory\n");
					close(self->fd);
					self->fd = -1;
					continue;
				}
				self->strm->obuflen = 0;
			}
#endif
#ifdef HAVE_LZ4
			if ((self->transport & 0xFFFF) == W_LZ4) {
				self->strm->obuflen = 0;

				/* get the maximum size that should ever be required and allocate for it */

				self->strm->hdl.z.cbuflen = LZ4F_compressFrameBound(sizeof(self->strm->obuf), NULL);
				if ((self->strm->hdl.z.cbuf = malloc(self->strm->hdl.z.cbuflen)) == NULL) {
					logerr("Failed to allocate %lu bytes for compressed LZ4 data\n", self->strm->hdl.z.cbuflen);
					close(self->fd);
					self->fd = -1;
					continue;
				}
			}
#endif
#ifdef HAVE_SNAPPY
			if ((self->transport & 0xFFFF) == W_SNAPPY) {
				self->strm->obuflen = 0;
			}
#endif

#ifdef HAVE_SSL
			if ((self->transport & ~0xFFFF) == W_SSL) {
				int rv;
				z_strm *sstrm;

				if ((self->transport & 0xFFFF) == W_PLAIN) { /* just SSL, nothing else */
					sstrm = self->strm;
				} else {
					sstrm = self->strm->nextstrm;
				}

				sstrm->hdl.ssl = SSL_new(sstrm->ctx);
				SSL_set_tlsext_host_name(sstrm->hdl.ssl, self->ip);
				if (SSL_set_fd(sstrm->hdl.ssl, self->fd) == 0) {
					logerr("failed to SSL_set_fd: %s\n",
							ERR_reason_error_string(ERR_get_error()));
					sstrm->strmclose(sstrm);
					self->fd = -1;
					continue;
				}
				if ((rv = SSL_connect(sstrm->hdl.ssl)) != 1) {
					logerr("failed to connect ssl stream: %s\n",
							sslerror(sstrm, rv));
					sstrm->strmclose(sstrm);
					self->fd = -1;
					continue;
				}
				if ((rv = SSL_get_verify_result(sstrm->hdl.ssl)) != X509_V_OK) {
					logerr("failed to verify ssl certificate: %s\n",
							sslerror(sstrm, rv));
					sstrm->strmclose(sstrm);
					self->fd = -1;
					continue;
				}
			} else
#endif
			{
				z_strm *pstrm;

				if (self->transport == W_PLAIN) { /* just plain socket */
					pstrm = self->strm;
				} else {
					pstrm = self->strm->nextstrm;
				}
				pstrm->hdl.sock = self->fd;
			}
		}

		/* send up to batch size */
		len = queue_dequeue_vector(self->batch, self->queue, self->bsize);
		self->batch[len] = NULL;
		metric = self->batch;

		if (len != 0 &&
				__sync_bool_compare_and_swap(&(self->keep_running), 0, 0))
		{
			/* be noisy during shutdown so we can track any slowing down
			 * servers, possibly preventing us to shut down */
			logerr("shutting down %s:%u: waiting for %zu metrics\n",
					self->ip, self->port, len + queue_len(self->queue));
		}

		if (len == 0 && __sync_add_and_fetch(&(self->failure), 0)) {
			/* if we don't have anything to send, we have at least a
			 * connection succeed, so assume the server is up again,
			 * this is in particular important for recovering this
			 * node by probes, to avoid starvation of this server since
			 * its queue is possibly being offloaded to secondaries */
			if (self->ctype != CON_UDP)
				logerr("server %s:%u: OK after probe\n", self->ip, self->port);
			__sync_and_and_fetch(&(self->failure), 0);
		}

		for (; *metric != NULL; metric++) {
			len = *(size_t *)(*metric);
			/* Write to the stream, this may not succeed completely due
			 * to flow control and whatnot, which the docs suggest need
			 * resuming to complete.  So, use a loop, but to avoid
			 * getting endlessly stuck on this, only try a limited
			 * number of times for a single metric. */
			for (cnt = 0, p = *metric + sizeof(size_t); cnt < 10; cnt++) {
				if ((slen = self->strm->strmwrite(self->strm, p, len)) != len) {
					if (slen >= 0) {
						p += slen;
						len -= slen;
					} else if (errno != EINTR) {
						break;
					}
					/* allow the remote to catch up */
					usleep((50 + (rand() % 150)) * 1000);  /* 50ms - 200ms */
				} else {
					break;
				}
			}
			if (slen != len) {
				/* not fully sent (after tries), or failure
				 * close connection regardless so we don't get
				 * synchonisation problems */
				if (self->ctype != CON_UDP &&
						__sync_fetch_and_add(&(self->failure), 1) == 0)
					logerr("failed to write() to %s:%u: %s\n",
							self->ip, self->port,
							(slen < 0 ?
							 self->strm->strmerror(self->strm, slen) :
							 "incomplete write"));
				self->strm->strmclose(self->strm);
				self->fd = -1;
				/* put back stuff we couldn't process */
				for (; *metric != NULL; metric++) {
					if (!queue_putback(self->queue, *metric)) {
						if (mode & MODE_DEBUG)
							logerr("server %s:%u: dropping metric: %s",
									self->ip, self->port,
									*metric + sizeof(size_t));
						free((char *)*metric);
						__sync_add_and_fetch(&(self->dropped), 1);
					}
				}
				break;
			} else if (!__sync_bool_compare_and_swap(&(self->failure), 0, 0)) {
				if (self->ctype != CON_UDP)
					logerr("server %s:%u: OK\n", self->ip, self->port);
				__sync_and_and_fetch(&(self->failure), 0);
			}
			free((char *)*metric);
			__sync_add_and_fetch(&(self->metrics), 1);
		}

		gettimeofday(&stop, NULL);
		__sync_add_and_fetch(&(self->ticks), timediff(start, stop));

		idle = 0;
	}
	__sync_and_and_fetch(&(self->running), 0);

	if (self->fd >= 0)
		self->strm->strmclose(self->strm);
	if (secpos != NULL)
		free(secpos);
	return NULL;
}

/**
 * Allocate a new (outbound) server.  Effectively this means a thread
 * that reads from the queue and sends this as good as it can to the ip
 * address and port associated.
 */
server *
server_new(
		const char *ip,
		unsigned short port,
		con_type type,
		con_trnsp transport,
		con_proto ctype,
		struct addrinfo *saddr,
		struct addrinfo *hint,
		size_t qsize,
		size_t bsize,
		int maxstalls,
		unsigned short iotimeout,
		unsigned int sockbufsize)
{
	server *ret;

	if ((ret = malloc(sizeof(server))) == NULL)
		return NULL;

	ret->type = type;
	ret->transport = transport;
	ret->ctype = ctype;
	ret->tid = 0;
	ret->secondaries = NULL;
	ret->secondariescnt = 0;
	ret->ip = strdup(ip);
	if (ret->ip == NULL) {
		free(ret);
		return NULL;
	}
	ret->port = port;
	ret->instance = NULL;
	ret->bsize = bsize;
	ret->iotimeout = iotimeout < 250 ? 600 : iotimeout;
	ret->sockbufsize = sockbufsize;
	ret->maxstalls = maxstalls;
	if ((ret->batch = malloc(sizeof(char *) * (bsize + 1))) == NULL) {
		free((char *)ret->ip);
		free(ret);
		return NULL;
	}
	ret->fd = -1;
	if ((ret->strm = malloc(sizeof(z_strm))) == NULL) {
		free((char *)ret->ip);
		free(ret->batch);
		free(ret);
		return NULL;
	}
	ret->strm->nextstrm = NULL;

	/* setup normal or SSL-wrapped socket first */
#ifdef HAVE_SSL
	if ((transport & ~0xFFFF) == W_SSL) {
		/* create an auto-negotiate context */
		const SSL_METHOD *m = SSLv23_client_method();
		ret->strm->ctx = SSL_CTX_new(m);
		
		if (sslCA != NULL) {
			if (ret->strm->ctx == NULL ||
				SSL_CTX_load_verify_locations(ret->strm->ctx,
											   sslCAisdir ? NULL : sslCA,
											   sslCAisdir ? sslCA : NULL) == 0)
			{
				char *err = ERR_error_string(ERR_get_error(), NULL);
				logerr("failed to load SSL verify locations from %s for "
						"%s:%d: %s\n", sslCA, ret->ip, ret->port, err);
				free((char *)ret->ip);
				free(ret->batch);
				free(ret->strm);
				free(ret);
				return NULL;
			}
		}
		SSL_CTX_set_verify(ret->strm->ctx, SSL_VERIFY_PEER, NULL);

		ret->strm->strmwrite = &sslwrite;
		ret->strm->strmflush = &sslflush;
		ret->strm->strmclose = &sslclose;
		ret->strm->strmerror = &sslerror;
	} else
#endif
	{
		ret->strm->strmwrite = &sockwrite;
		ret->strm->strmflush = &sockflush;
		ret->strm->strmclose = &sockclose;
		ret->strm->strmerror = &sockerror;
	}
	
	/* now see if we have a compressor defined */
	if ((transport & 0xFFFF) == W_PLAIN) {
		/* catch noop */
	}
#ifdef HAVE_GZIP
	else if ((transport & 0xFFFF) == W_GZIP) {
		z_strm *gzstrm = malloc(sizeof(z_strm));
		if (gzstrm == NULL) {
			free((char *)ret->ip);
			free(ret->batch);
			free(ret->strm);
			free(ret);
			return NULL;
		}
		gzstrm->strmwrite = &gzipwrite;
		gzstrm->strmflush = &gzipflush;
		gzstrm->strmclose = &gzipclose;
		gzstrm->strmerror = &gziperror;
		gzstrm->nextstrm = ret->strm;
		ret->strm = gzstrm;
	}
#endif
#ifdef HAVE_LZ4
	else if ((transport & 0xFFFF) == W_LZ4) {
		z_strm *lzstrm = malloc(sizeof(z_strm));
		if (lzstrm == NULL) {
			free((char *)ret->ip);
			free(ret->batch);
			free(ret->strm);
			free(ret);
			return NULL;
		}
		lzstrm->strmwrite = &lzwrite;
		lzstrm->strmflush = &lzflush;
		lzstrm->strmclose = &lzclose;
		lzstrm->strmerror = &lzerror;
		lzstrm->nextstrm = ret->strm;
		ret->strm = lzstrm;
	}
#endif
#ifdef HAVE_SNAPPY
	else if ((transport & 0xFFFF) == W_SNAPPY) {
		z_strm *snpstrm = malloc(sizeof(z_strm));
		if (snpstrm == NULL) {
			free((char *)ret->ip);
			free(ret->batch);
			free(ret->strm);
			free(ret);
			return NULL;
		}
		snpstrm->strmwrite = &snappywrite;
		snpstrm->strmflush = &snappyflush;
		snpstrm->strmclose = &snappyclose;
		snpstrm->strmerror = &snappyerror;
		snpstrm->nextstrm = ret->strm;
		ret->strm = snpstrm;
	}
#endif

	ret->saddr = saddr;
	ret->reresolve = 0;
	ret->hint = NULL;
	if (hint != NULL) {
		ret->reresolve = 1;
		ret->hint = hint;
	}
	ret->queue = queue_new(qsize);
	if (ret->queue == NULL) {
		free(ret->batch);
		free((char *)ret->ip);
		free(ret);
		return NULL;
	}

	ret->failover = 0;
	ret->failure = 0;
	ret->running = 0;
	ret->keep_running = 1;
	ret->stallseq = 0;
	ret->metrics = 0;
	ret->dropped = 0;
	ret->stalls = 0;
	ret->ticks = 0;
	ret->prevmetrics = 0;
	ret->prevdropped = 0;
	ret->prevstalls = 0;
	ret->prevticks = 0;
	ret->tid = 0;

	return ret;
}

/**
 * Compare server s against the address info in saddr.  A server is
 * considered to be equal is it is of the same socket family, type and
 * protocol, and if the target address and port are the same.  When
 * saddr is NULL, a match against the given ip is attempted, e.g. for
 * file destinations.
 */
char
server_cmp(server *s, struct addrinfo *saddr, const char *ip, unsigned short port, con_proto proto)
{
	if ((saddr == NULL || s->saddr == NULL)) {
		if (strcmp(s->ip, ip) == 0 && s->port == port && s->ctype == proto)
			return 0;
	} else if (
			s->saddr->ai_family == saddr->ai_family &&
			s->saddr->ai_socktype == saddr->ai_socktype &&
			s->saddr->ai_protocol == saddr->ai_protocol
	   )
	{
		if (saddr->ai_family == AF_INET) {
			struct sockaddr_in *l = ((struct sockaddr_in *)s->saddr->ai_addr);
			struct sockaddr_in *r = ((struct sockaddr_in *)saddr->ai_addr);
			if (l->sin_port == r->sin_port &&
					l->sin_addr.s_addr == r->sin_addr.s_addr)
				return 0;
		} else if (saddr->ai_family == AF_INET6) {
			struct sockaddr_in6 *l =
				((struct sockaddr_in6 *)s->saddr->ai_addr);
			struct sockaddr_in6 *r =
				((struct sockaddr_in6 *)saddr->ai_addr);
			if (l->sin6_port == r->sin6_port &&
					memcmp(l->sin6_addr.s6_addr, r->sin6_addr.s6_addr,
						sizeof(l->sin6_addr.s6_addr)) == 0)
				return 0;
		}
	}

	return 1;  /* not equal */
}

/**
 * Starts a previously created server using server_new().  Returns
 * errno if starting a thread failed, after which the caller should
 * server_free() the given s pointer.
 */
char
server_start(server *s)
{
	return pthread_create(&s->tid, NULL, &server_queuereader, s);
}

/**
 * Adds a list of secondary servers to this server.  A secondary server
 * is a server which' queue will be checked when this server has nothing
 * to do.
 */
void
server_add_secondaries(server *self, server **secondaries, size_t count)
{
	self->secondaries = secondaries;
	self->secondariescnt = count;
}

/**
 * Flags this server as part of a failover cluster, which means the
 * secondaries are used only to offload on failure, not on queue stress.
 */
void
server_set_failover(server *self)
{
	self->failover = 1;
}

/**
 * Sets instance name only used for carbon_ch cluster type.
 */
void
server_set_instance(server *self, char *instance)
{
	if (self->instance != NULL)
		free(self->instance);
	self->instance = strdup(instance);
}

/**
 * Thin wrapper around the associated queue with the server object.
 * Returns true if the metric could be queued for sending, or the metric
 * was dropped because the associated server is down.  Returns false
 * otherwise (when a retry seems like it could succeed shortly).
 */
inline char
server_send(server *s, const char *d, char force)
{
	if (queue_free(s->queue) == 0) {
		char failure = __sync_add_and_fetch(&(s->failure), 0);
		if (!force && s->secondariescnt > 0) {
			size_t i;
			/* don't immediately drop if we know there are others that
			 * back us up */
			for (i = 0; i < s->secondariescnt; i++) {
				if (!__sync_add_and_fetch(&(s->secondaries[i]->failure), 0)) {
					failure = 0;
					break;
				}
			}
		}
		if (failure || force ||
				__sync_add_and_fetch(&(s->stallseq), 0) == s->maxstalls)
		{
			__sync_add_and_fetch(&(s->dropped), 1);
			/* excess event will be dropped by the enqueue below */
		} else {
			__sync_add_and_fetch(&(s->stallseq), 1);
			__sync_add_and_fetch(&(s->stalls), 1);
			return 0;
		}
	} else {
		__sync_and_and_fetch(&(s->stallseq), 0);
	}
	queue_enqueue(s->queue, d);

	return 1;
}

/**
 * Tells this server to finish sending pending items from its queue.
 */
void
server_shutdown(server *s)
{
	int i;
	size_t failures;
	size_t inqueue;

	/* this function should only be called on a running server */
	if (__sync_bool_compare_and_swap(&(s->keep_running), 0, 0))
		return;

	if (s->secondariescnt > 0) {
		/* if we have a working connection, or we still have stuff in
		 * our queue, wait for our secondaries, as they might need us,
		 * or we need them */
		do {
			failures = 0;
			inqueue = 0;
			for (i = 0; i < s->secondariescnt; i++) {
				if (__sync_add_and_fetch(&(s->secondaries[i]->failure), 0))
					failures++;
				if (__sync_add_and_fetch(&(s->secondaries[i]->running), 0))
					inqueue += queue_len(s->secondaries[i]->queue);
			}
			/* loop until we all failed, or nothing is in the queues */
		} while (failures != s->secondariescnt &&
				inqueue != 0 &&
				logout("any_of cluster pending %zu metrics "
					"(with %zu failed nodes)\n", inqueue, failures) >= -1 &&
				usleep((200 + (rand() % 100)) * 1000) <= 0);
		/* shut down entire cluster */
		for (i = 0; i < s->secondariescnt; i++)
			__sync_bool_compare_and_swap(
					&(s->secondaries[i]->keep_running), 1, 0);
		/* to pretend to be dead for above loop (just in case) */
		if (inqueue != 0)
			for (i = 0; i < s->secondariescnt; i++)
				__sync_add_and_fetch(&(s->secondaries[i]->failure), 1);
		/* wait for the secondaries to be stopped so we surely don't get
		 * invalid reads when server_free is called */
		for (i = 0; i < s->secondariescnt; i++) {
			while (__sync_add_and_fetch(&(s->secondaries[i]->running), 0))
				usleep((200 + (rand() % 100)) * 1000);
		}
	}

	__sync_bool_compare_and_swap(&(s->keep_running), 1, 0);
}

/**
 * Frees this server and associated resources.  This includes joining
 * the server thread.
 */
void
server_free(server *s) {
	int err;

	if (s->tid != 0 && (err = pthread_join(s->tid, NULL)) != 0)
		logerr("%s:%u: failed to join server thread: %s\n",
				s->ip, s->port, strerror(err));
	s->tid = 0;

	if (s->ctype == CON_TCP) {
		size_t qlen = queue_len(s->queue);
		if (qlen > 0)
			logerr("dropping %zu metrics for %s:%u\n",
					qlen, s->ip, s->port);
	}

	queue_destroy(s->queue);
	free(s->batch);
	if (s->instance)
		free(s->instance);
	if (s->saddr != NULL)
		freeaddrinfo(s->saddr);
	if (s->hint)
		free(s->hint);
	free((char *)s->ip);
	if (s->strm->nextstrm != NULL)
		free(s->strm->nextstrm);
	free(s->strm);
	s->ip = NULL;
	free(s);
}

/**
 * Swaps the queue between server l to r.  This is assumes both l and r
 * are not running.
 */
void
server_swap_queue(server *l, server *r)
{
	queue *t;

	assert(l->keep_running == 0 || l->tid == 0);
	assert(r->keep_running == 0 || r->tid == 0);

	t = l->queue;
	l->queue = r->queue;
	r->queue = t;
	
	/* swap associated statistics as well */
	l->metrics = r->metrics;
	l->dropped = r->dropped;
	l->stalls = r->stalls;
	l->ticks = r->ticks;
	l->prevmetrics = r->prevmetrics;
	l->prevdropped = r->prevdropped;
	l->prevstalls = r->prevstalls;
	l->prevticks = r->prevticks;
}

/**
 * Returns the ip address this server points to.
 */
inline const char *
server_ip(server *s)
{
	if (s == NULL)
		return NULL;
	return s->ip;
}

/**
 * Returns the port this server connects at.
 */
inline unsigned short
server_port(server *s)
{
	if (s == NULL)
		return 0;
	return s->port;
}

/**
 * Returns the instance associated with this server.
 */
inline char *
server_instance(server *s)
{
	return s->instance;
}

/**
 * Returns the connection protocol of this server.
 */
inline con_proto
server_ctype(server *s)
{
	if (s == NULL)
		return CON_PIPE;
	return s->ctype;
}

/**
 * Returns the connection transport of this server.
 */
inline con_trnsp
server_transport(server *s)
{
	if (s == NULL)
		return W_PLAIN;
	return s->transport;
}

/**
 * Returns the connection type of this server.
 */
inline con_type
server_type(server *s)
{
	if (s == NULL)
		return T_LINEMODE;
	return s->type;
}

/**
 * Returns whether the last action on this server caused a failure.
 */
inline char
server_failed(server *s)
{
	if (s == NULL)
		return 0;
	return __sync_add_and_fetch(&(s->failure), 0);
}

/**
 * Returns the wall-clock time in microseconds (us) consumed sending metrics.
 */
inline size_t
server_get_ticks(server *s)
{
	if (s == NULL)
		return 0;
	return __sync_add_and_fetch(&(s->ticks), 0);
}

/**
 * Returns the wall-clock time in microseconds (us) consumed since last
 * call to this function.
 */
inline size_t
server_get_ticks_sub(server *s)
{
	size_t d;
	if (s == NULL)
		return 0;
	d = __sync_add_and_fetch(&(s->ticks), 0) - s->prevticks;
	s->prevticks += d;
	return d;
}

/**
 * Returns the number of metrics sent since start.
 */
inline size_t
server_get_metrics(server *s)
{
	if (s == NULL)
		return 0;
	return __sync_add_and_fetch(&(s->metrics), 0);
}

/**
 * Returns the number of metrics sent since last call to this function.
 */
inline size_t
server_get_metrics_sub(server *s)
{
	size_t d;
	if (s == NULL)
		return 0;
	d = __sync_add_and_fetch(&(s->metrics), 0) - s->prevmetrics;
	s->prevmetrics += d;
	return d;
}

/**
 * Returns the number of metrics dropped since start.
 */
inline size_t
server_get_dropped(server *s)
{
	if (s == NULL)
		return 0;
	return __sync_add_and_fetch(&(s->dropped), 0);
}

/**
 * Returns the number of metrics dropped since last call to this function.
 */
inline size_t
server_get_dropped_sub(server *s)
{
	size_t d;
	if (s == NULL)
		return 0;
	d = __sync_add_and_fetch(&(s->dropped), 0) - s->prevdropped;
	s->prevdropped += d;
	return d;
}

/**
 * Returns the number of stalls since start.  A stall happens when the
 * queue is full, but it appears as if it would be a good idea to wait
 * for a brief period and retry.
 */
inline size_t
server_get_stalls(server *s)
{
	if (s == NULL)
		return 0;
	return __sync_add_and_fetch(&(s->stalls), 0);
}

/**
 * Returns the number of stalls since last call to this function.
 */
inline size_t
server_get_stalls_sub(server *s)
{
	size_t d;
	if (s == NULL)
		return 0;
	d = __sync_add_and_fetch(&(s->stalls), 0) - s->prevstalls;
	s->prevstalls += d;
	return d;
}

/**
 * Returns the (approximate) number of metrics waiting to be sent.
 */
inline size_t
server_get_queue_len(server *s)
{
	if (s == NULL)
		return 0;
	return queue_len(s->queue);
}

/**
 * Returns the allocated size of the queue backing metrics waiting to be
 * sent.
 */
inline size_t
server_get_queue_size(server *s)
{
	if (s == NULL)
		return 0;
	return queue_size(s->queue);
}
