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


#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <errno.h>
#include <poll.h>
#include <pthread.h>
#include <sys/uio.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/resource.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include "relay.h"
#include "router.h"
#include "server.h"
#include "collector.h"
#include "dispatcher.h"

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

enum conntype {
	LISTENER,
	CONNECTION
};

typedef struct _z_strm {
	ssize_t (*strmread)(struct _z_strm *, void *, size_t);  /* read func */
	int (*strmclose)(struct _z_strm *);
	union {
#ifdef HAVE_GZIP
		z_stream z;
#endif
#ifdef HAVE_LZ4
		LZ4F_decompressionContext_t lz;
#endif
#ifdef HAVE_SSL
		SSL *ssl;
#endif
		int sock;
		/* udp variant (in order to receive info about sender) */
		struct udp_strm {
			int sock;
			struct sockaddr_in6 saddr;
			char *srcaddr;
			size_t srcaddrlen;
		} udp;
	} hdl;
	char ibuf[METRIC_BUFSIZ];
	unsigned short ipos;
	struct _z_strm *nextstrm;
} z_strm;

typedef struct _connection {
	int sock;
	z_strm *strm;
	char takenby;   /* -2: being setup, -1: free, 0: not taken, >0: tid */
	char srcaddr[24];  /* string representation of source address */
	char buf[METRIC_BUFSIZ];
	int buflen;
	char needmore:1;
	char noexpire:1;
	char metric[METRIC_BUFSIZ];
	destination dests[CONN_DESTS_SIZE];
	size_t destlen;
	struct timeval lastwork;
	unsigned int maxsenddelay;
	char hadwork:1;
	char isaggr:1;
	char isudp:1;
} connection;

struct _dispatcher {
	pthread_t tid;
	enum conntype type;
	char id;
	size_t metrics;
	size_t blackholes;
	size_t discards;
	size_t ticks;
	size_t sleeps;
	size_t prevmetrics;
	size_t prevblackholes;
	size_t prevdiscards;
	size_t prevticks;
	size_t prevsleeps;
	char keep_running;  /* full byte for atomic access */
	router *rtr;
	router *pending_rtr;
	char route_refresh_pending;  /* full byte for atomic access */
	char hold;  /* full byte for atomic access */
	char *allowed_chars;
	char tags_supported;
	int maxinplen;
	int maxmetriclen;
};

static listener **listeners = NULL;
static connection *connections = NULL;
static size_t connectionslen = 0;
pthread_rwlock_t listenerslock = PTHREAD_RWLOCK_INITIALIZER;
pthread_rwlock_t connectionslock = PTHREAD_RWLOCK_INITIALIZER;
static size_t acceptedconnections = 0;
static size_t closedconnections = 0;
static unsigned int sockbufsize = 0;

/* connection specific readers and closers */

/* ordinary socket */
static inline ssize_t
sockread(z_strm *strm, void *buf, size_t sze)
{
	return read(strm->hdl.sock, buf, sze);
}

static inline int
sockclose(z_strm *strm)
{
	int ret = close(strm->hdl.sock);
	free(strm);
	return ret;
}

/* udp socket */
static inline ssize_t
udpsockread(z_strm *strm, void *buf, size_t sze)
{
	ssize_t ret;
	struct udp_strm *s = &strm->hdl.udp;
	socklen_t slen = sizeof(s->saddr);
	ret = recvfrom(s->sock, buf, sze, 0, (struct sockaddr *)&s->saddr, &slen);
	if (ret <= 0)
		return ret;

	/* figure out who's calling */
	s->srcaddr[0] = '\0';
	switch (s->saddr.sin6_family) {
		case PF_INET:
			inet_ntop(s->saddr.sin6_family,
					&((struct sockaddr_in *)&s->saddr)->sin_addr,
					s->srcaddr, s->srcaddrlen);
			break;
		case PF_INET6:
			inet_ntop(s->saddr.sin6_family, &s->saddr.sin6_addr,
					s->srcaddr, s->srcaddrlen);
			break;
	}

	return ret;
}

static inline int
udpsockclose(z_strm *strm)
{
	int ret = close(strm->hdl.udp.sock);
	free(strm);
	return ret;
}

#ifdef HAVE_GZIP
/* gzip wrapped socket */
static inline ssize_t
gzipread(z_strm *strm, void *buf, size_t sze)
{
	z_stream *zstrm = &(strm->hdl.z);
	char *ibuf = strm->ibuf;
	int ret;
	int iret;
	int err = 0;
	int inflatemode = Z_SYNC_FLUSH;

	/* read any available data, if it fits */
	ret = strm->nextstrm->strmread(strm->nextstrm,
			ibuf + strm->ipos,
			METRIC_BUFSIZ - strm->ipos);
	if (ret > 0) {
		strm->ipos += ret;
	} else if (ret < 0) {
		err = errno;
		if (err != EAGAIN)
			inflatemode = Z_FINISH;
	} else {
		/* ret == 0: EOF, which means we didn't read anything here, so
		 * calling inflate should be to flush whatever is in the zlib
		 * buffers, much like a read error. */
		inflatemode = Z_FINISH;
	}

	zstrm->next_in = (Bytef *)ibuf;
	zstrm->avail_in = (uInt)(strm->ipos);
	zstrm->next_out = (Bytef *)buf;
	zstrm->avail_out = (uInt)sze;

	iret = inflate(zstrm, inflatemode);

	/* if inflate consumed something, update our ibuf */
	if ((Bytef *)ibuf != zstrm->next_in) {
		memmove(ibuf, zstrm->next_in, zstrm->avail_in);
		strm->ipos = zstrm->avail_in;
	}

	switch (iret) {
		case Z_OK:  /* progress has been made */
			/* calculate the "returned" bytes */
			iret = sze - zstrm->avail_out;
			if (iret == 0) {
				/* something was done, so must have been reading input */
				errno = EAGAIN;
				return -1;
			}
			break;
		case Z_STREAM_END:  /* everything uncompressed, nothing pending */
			iret = sze - zstrm->avail_out;
			break;
		case Z_DATA_ERROR:  /* corrupt input */
			inflateSync(zstrm);
			/* return isn't much of interest, we will call inflate next
			 * time and sync again if it still fails */
			errno = err ? err : EAGAIN;
			return -1;
			break;
		case Z_MEM_ERROR:  /* out of memory */
			logerr("out of memory during read of gzip stream\n");
			errno = ENOMEM;
			return -1;
		case Z_BUF_ERROR:  /* output buffer full or nothing to read */
			/* if this happens, zlib still can't write even though last
			 * call should have increased the buffer size by consuming
			 * whatever was valid, so if we're stuck here, the situation
			 * isn't going to improve, and we better bail out */
			errno = err ? err : EMSGSIZE;
			return -1;
	}

	if (iret == 0 && ret != 0) {
		errno = EAGAIN;
		iret = -1;
	}

	return (ssize_t)iret;
}

static inline int
gzipclose(z_strm *strm)
{
	int ret = strm->nextstrm->strmclose(strm->nextstrm);
	inflateEnd(&(strm->hdl.z));
	free(strm);
	return ret;
}
#endif

#ifdef HAVE_LZ4
/* lz4 wrapped socket */
static inline ssize_t
lzread(z_strm *strm, void *buf, size_t sze)
{
	char *ibuf = strm->ibuf;
	size_t srcsize;
	size_t destsize;
	int ret;

	/* read any available data, if it fits */
	ret = strm->nextstrm->strmread(strm->nextstrm,
			ibuf + strm->ipos, METRIC_BUFSIZ - strm->ipos);

	/* if EOF(0) or no-data(-1) then only get out now if the input
	 * buffer is empty because start of next frame may be waiting for us */
	
	if (ret > 0) {
		strm->ipos += ret;
	} else if (ret < 0) {
		if (strm->ipos == 0)
			return -1;
	} else {
		/* ret == 0, a.k.a. EOF */
		if (strm->ipos == 0)
			return 0;
	}

	/* attempt to decompress something from the (partial) frame that's
	 * arrived so far.  srcsize is updated to the number of bytes
	 * consumed. likewise for destsize and bytes written */

	srcsize = strm->ipos;
	destsize = sze;
	
	ret = LZ4F_decompress(strm->hdl.lz, buf, &destsize, ibuf, &srcsize, NULL);

	/* check for error before doing anything else */

	if (LZ4F_isError(ret)) {

		/* liblz4 doesn't allow access to the error constants so have to
		 * return a generic code */

		logerr("Error %d reading LZ4 compressed data\n", (int)ret);
		errno = EBADMSG;
		strm->ipos = 0;

		return -1;
	}

	/* if we decompressed something, update our ibuf */

	if (srcsize > 0) {
		memmove(ibuf, ibuf + srcsize, strm->ipos - srcsize);
		strm->ipos -= srcsize;
	}

	if (destsize == 0) {
		tracef("No LZ4 data was produced\n");
		errno = EAGAIN;
		return -1;
	}

#ifdef ENABLE_TRACE
	/* debug logging */
	if (ret == 0)
		tracef("LZ4 frame fully decoded\n");
#endif

	return (ssize_t)destsize;
}

static inline int
lzclose(z_strm *strm)
{
	int ret = strm->nextstrm->strmclose(strm->nextstrm);
	LZ4F_freeDecompressionContext(strm->hdl.lz);
	free(strm);
	return ret;
}
#endif

#ifdef HAVE_SNAPPY
/* snappy wrapped socket */
static inline ssize_t
snappyread(z_strm *strm, void *buf, size_t sze)
{
	char *ibuf = strm->ibuf;
	int ret;
	size_t buflen = sze;

	/* read any available data, if it fits */
	ret = strm->nextstrm->strmread(strm->nextstrm,
			ibuf + strm->ipos,
			METRIC_BUFSIZ - strm->ipos);
	if (ret > 0) {
		strm->ipos += ret;
	} else if (ret < 0) {
		return -1;
	} else {
		/* ret == 0, a.k.a. EOF */
		return 0;
	}

	ret = snappy_uncompress(ibuf, strm->ipos, buf, &buflen);

	/* if we decompressed something, update our ibuf */
	if (ret == SNAPPY_OK) {
		strm->ipos = strm->ipos - buflen;
		memmove(ibuf, ibuf + buflen, strm->ipos);
	} else if (ret == SNAPPY_BUFFER_TOO_SMALL) {
		logerr("discarding snappy buffer: "
				"the uncompressed block is too large\n");
		strm->ipos = 0;
	}

	return (ssize_t)(ret == SNAPPY_OK ? buflen : -1);
}

static inline int
snappyclose(z_strm *strm)
{
	int ret = strm->nextstrm->strmclose(strm->nextstrm);
	free(strm);
	return ret;
}
#endif

#ifdef HAVE_SSL
/* (Open|Libre)SSL wrapped socket */
static inline ssize_t
sslread(z_strm *strm, void *buf, size_t sze)
{
	return (ssize_t)SSL_read(strm->hdl.ssl, buf, (int)sze);
}

static inline int
sslclose(z_strm *strm)
{
	int sock = SSL_get_fd(strm->hdl.ssl);
	SSL_free(strm->hdl.ssl);
	return close(sock);
}
#endif

/**
 * Helper function to try and be helpful to the user.  If errno
 * indicates no new fds could be made, checks what the current max open
 * files limit is, and if it's close to what we have in use now, write
 * an informative message to stderr.
 */
void
dispatch_check_rlimit_and_warn(void)
{
	if (errno == EISCONN || errno == EMFILE) {
		struct rlimit ofiles;
		/* rlimit can be changed for the running process (at least on
		 * Linux 2.6+) so refetch this value every time, should only
		 * occur on errors anyway */
		if (getrlimit(RLIMIT_NOFILE, &ofiles) < 0)
			ofiles.rlim_max = 0;
		if (ofiles.rlim_max != RLIM_INFINITY && ofiles.rlim_max > 0)
			logerr("process configured maximum connections = %d, "
					"consider raising max open files/max descriptor limit\n",
					(int)ofiles.rlim_max);
	}
}

#define MAX_LISTENERS 32  /* hopefully enough */

/**
 * Adds an (initial) listener socket to the chain of connections.
 * Listener sockets are those which need to be accept()-ed on.
 */
int
dispatch_addlistener(listener *lsnr)
{
	int c;
	int *socks;

	if (lsnr->ctype == CON_UDP) {
		/* Adds a pseudo-listener for datagram (UDP) sockets, which is
		 * pseudo, for in fact it adds a new connection, but makes sure
		 * that connection won't be closed after being idle, and won't
		 * count that connection as an incoming connection either. */
		for (socks = lsnr->socks; *socks != -1; socks++) {
			c = dispatch_addconnection(*socks, lsnr);

			if (c == -1)
				return 1;

			connections[c].noexpire = 1;
			connections[c].isudp = 1;
			acceptedconnections--;
		}

		return 0;
	}

	pthread_rwlock_wrlock(&listenerslock);
	for (c = 0; c < MAX_LISTENERS; c++) {
		if (listeners[c] == NULL) {
			listeners[c] = lsnr;
			for (socks = lsnr->socks; *socks != -1; socks++)
				(void) fcntl(*socks, F_SETFL, O_NONBLOCK);
			break;
		}
	}
	if (c == MAX_LISTENERS) {
		logerr("cannot add new listener: "
				"no more free listener slots (max = %d)\n",
				MAX_LISTENERS);
		pthread_rwlock_unlock(&listenerslock);
		return 1;
	}
	pthread_rwlock_unlock(&listenerslock);

	return 0;
}

/**
 * Remove listener from the listeners list.  Each removal will incur a
 * global lock.  Frequent usage of this function is not anticipated.
 */
void
dispatch_removelistener(listener *lsnr)
{
	int c;
	int *socks;

	if (lsnr->ctype != CON_UDP) {
		pthread_rwlock_wrlock(&listenerslock);
		/* find connection */
		for (c = 0; c < MAX_LISTENERS; c++)
			if (listeners[c] != NULL && listeners[c] == lsnr)
				break;
		if (c == MAX_LISTENERS) {
			/* not found?!? */
			logerr("dispatch: cannot find listener to remove!\n");
			pthread_rwlock_unlock(&listenerslock);
			return;
		}
		listeners[c] = NULL;
		pthread_rwlock_unlock(&listenerslock);
	}
	/* cleanup */
#ifdef HAVE_SSL
	if ((lsnr->transport & ~0xFFFF) == W_SSL)
		SSL_CTX_free(lsnr->ctx);
#endif
	/* acquire a write lock on connections, which is a bit wrong, but it
	 * ensures all dispatchers are stopped while we close the sockets,
	 * which avoids a race on the reading thereof if this is a UDP
	 * connection */
	pthread_rwlock_wrlock(&connectionslock);
	for (socks = lsnr->socks; *socks != -1; socks++) {
		close(*socks);
		*socks = -1;
	}
	pthread_rwlock_unlock(&connectionslock);
	if (lsnr->saddrs) {
		freeaddrinfo(lsnr->saddrs);
		lsnr->saddrs = NULL;
	}
}

/**
 * Copy over all state related things from olsnr to nlsnr and ensure
 * olsnr can be discarded (that is, thrown away without calling
 * dispatch_removelistener).
 */
void
dispatch_transplantlistener(listener *olsnr, listener *nlsnr, router *r)
{
	int c;

	pthread_rwlock_wrlock(&listenerslock);
	for (c = 0; c < MAX_LISTENERS; c++) {
		if (listeners[c] == olsnr) {
			router_transplant_listener_socks(r, olsnr, nlsnr);
#ifdef HAVE_SSL
			if ((nlsnr->transport & ~0xFFFF) == W_SSL)
				nlsnr->ctx = olsnr->ctx;
#endif
			if (olsnr->saddrs) {
				freeaddrinfo(olsnr->saddrs);
				olsnr->saddrs = NULL;
			}
			listeners[c] = nlsnr;
			break;  /* found and done */
		}
	}
	pthread_rwlock_unlock(&listenerslock);
}

#define CONNGROWSZ  1024

/**
 * Adds a connection socket to the chain of connections.
 * Connection sockets are those which need to be read from.
 * Returns the connection id, or -1 if a failure occurred.
 */
int
dispatch_addconnection(int sock, listener *lsnr)
{
	size_t c;
	struct sockaddr_in6 saddr;
	socklen_t saddr_len = sizeof(saddr);

	pthread_rwlock_rdlock(&connectionslock);
	for (c = 0; c < connectionslen; c++)
		if (__sync_bool_compare_and_swap(&(connections[c].takenby), -1, -2))
			break;
	pthread_rwlock_unlock(&connectionslock);

	if (c == connectionslen) {
		connection *newlst;

		pthread_rwlock_wrlock(&connectionslock);
		if (connectionslen > c) {
			/* another dispatcher just extended the list */
			pthread_rwlock_unlock(&connectionslock);
			return dispatch_addconnection(sock, lsnr);
		}
		newlst = realloc(connections,
				sizeof(connection) * (connectionslen + CONNGROWSZ));
		if (newlst == NULL) {
			logerr("cannot add new connection: "
					"out of memory allocating more slots (max = %zu)\n",
					connectionslen);

			pthread_rwlock_unlock(&connectionslock);
			return -1;
		} else if (newlst != connections) {
		    /* reset srcaddr after realloc due to issue 346 */
		    for (c = 0; c < connectionslen; c++) {
			if (newlst[c].isudp) {
			    newlst[c].strm->hdl.udp.srcaddr = newlst[c].srcaddr;
			    newlst[c].strm->hdl.udp.srcaddrlen = sizeof(newlst[c].srcaddr);
			}
		    }
		}

		for (c = connectionslen; c < connectionslen + CONNGROWSZ; c++) {
			memset(&newlst[c], '\0', sizeof(connection));
			newlst[c].takenby = -1;  /* free */
		}
		connections = newlst;
		c = connectionslen;  /* for the setup code below */
		newlst[c].takenby = -2;
		connectionslen += CONNGROWSZ;

		pthread_rwlock_unlock(&connectionslock);
	}

	/* figure out who's calling */
	if (getpeername(sock, (struct sockaddr *)&saddr, &saddr_len) == 0) {
		snprintf(connections[c].srcaddr, sizeof(connections[c].srcaddr),
				"(unknown)");
		switch (saddr.sin6_family) {
			case PF_INET:
				inet_ntop(saddr.sin6_family,
						&((struct sockaddr_in *)&saddr)->sin_addr,
						connections[c].srcaddr, sizeof(connections[c].srcaddr));
				break;
			case PF_INET6:
				inet_ntop(saddr.sin6_family, &saddr.sin6_addr,
						connections[c].srcaddr, sizeof(connections[c].srcaddr));
				break;
		}
	}

	(void) fcntl(sock, F_SETFL, O_NONBLOCK);
	if (sockbufsize > 0) {
		if (setsockopt(sock, SOL_SOCKET, SO_RCVBUF,
				&sockbufsize, sizeof(sockbufsize)) != 0)
			;
	}
	connections[c].sock = sock;
	connections[c].strm = malloc(sizeof(z_strm));
	if (connections[c].strm == NULL) {
		logerr("cannot add new connection: "
				"out of memory allocating stream\n");
		__sync_bool_compare_and_swap(&(connections[c].takenby), -2, -1);
		return -1;
	}

	/* set socket or SSL connection */
	connections[c].strm->nextstrm = NULL;
	if (lsnr == NULL || (lsnr->transport & ~0xFFFF) != W_SSL) {
		if (lsnr == NULL || lsnr->ctype != CON_UDP) {
			connections[c].strm->hdl.sock = sock;
			connections[c].strm->strmread = &sockread;
			connections[c].strm->strmclose = &sockclose;
		} else {
			connections[c].strm->hdl.udp.sock = sock;
			connections[c].strm->hdl.udp.srcaddr =
				connections[c].srcaddr;
			connections[c].strm->hdl.udp.srcaddrlen =
				sizeof(connections[c].srcaddr);
			connections[c].strm->strmread = &udpsockread;
			connections[c].strm->strmclose = &udpsockclose;
		}
#ifdef HAVE_SSL
	} else {
		if ((connections[c].strm->hdl.ssl = SSL_new(lsnr->ctx)) == NULL) {
			logerr("cannot add new connection: %s\n",
					ERR_reason_error_string(ERR_get_error()));
			__sync_bool_compare_and_swap(&(connections[c].takenby), -2, -1);
			return -1;
		};
		SSL_set_fd(connections[c].strm->hdl.ssl, sock);
		SSL_set_accept_state(connections[c].strm->hdl.ssl);

		connections[c].strm->strmread = &sslread;
		connections[c].strm->strmclose = &sslclose;
#endif
	}

	/* setup decompressor */
	if (lsnr == NULL) {
		/* do nothing, catch case only */
	}
#ifdef HAVE_GZIP
	else if ((lsnr->transport & 0xFFFF) == W_GZIP) {
		z_strm *zstrm = malloc(sizeof(z_strm));
		if (zstrm == NULL) {
			logerr("cannot add new connection: "
					"out of memory allocating gzip stream\n");
			__sync_bool_compare_and_swap(&(connections[c].takenby), -2, -1);
			return -1;
		}
		zstrm->hdl.z.zalloc = Z_NULL;
		zstrm->hdl.z.zfree = Z_NULL;
		zstrm->hdl.z.opaque = Z_NULL;
		zstrm->ipos = 0;
		inflateInit2(&(zstrm->hdl.z), 15 + 16);
		zstrm->strmread = &gzipread;
		zstrm->strmclose = &gzipclose;
		zstrm->nextstrm = connections[c].strm;
		connections[c].strm = zstrm;
	}
#endif
#ifdef HAVE_LZ4
	else if ((lsnr->transport & 0xFFFF) == W_LZ4) {
		z_strm *lzstrm = malloc(sizeof(z_strm));
		if (lzstrm == NULL) {
			logerr("cannot add new connection: "
					"out of memory allocating lz4 stream\n");
			__sync_bool_compare_and_swap(&(connections[c].takenby), -2, -1);
			return -1;
		}
		if (LZ4F_isError(LZ4F_createDecompressionContext(&lzstrm->hdl.lz, LZ4F_VERSION))) {
			logerr("Failed to create LZ4 decompression context\n");
			return -1;
		}
		
		lzstrm->strmread = &lzread;
		lzstrm->strmclose = &lzclose;
		lzstrm->nextstrm = connections[c].strm;
		connections[c].strm = lzstrm;
	}
#endif
#ifdef HAVE_SNAPPY
	else if ((lsnr->transport & 0xFFFF) == W_SNAPPY) {
		z_strm *lzstrm = malloc(sizeof(z_strm));
		if (lzstrm == NULL) {
			logerr("cannot add new connection: "
					"out of memory allocating snappy stream\n");
			__sync_bool_compare_and_swap(&(connections[c].takenby), -2, -1);
			return -1;
		}
		lzstrm->strmread = &snappyread;
		lzstrm->strmclose = &snappyclose;
		lzstrm->nextstrm = connections[c].strm;
		connections[c].strm = lzstrm;
	}
#endif
	connections[c].buflen = 0;
	connections[c].needmore = 0;
	connections[c].noexpire = noexpire;
	connections[c].isaggr = 0;
	connections[c].isudp = 0;
	connections[c].destlen = 0;
	gettimeofday(&connections[c].lastwork, NULL);
	connections[c].hadwork = 1;  /* force first iteration before stalling */
	/* after this dispatchers will pick this connection up */
	__sync_bool_compare_and_swap(&(connections[c].takenby), -2, 0);
	__sync_add_and_fetch(&acceptedconnections, 1);

	return c;
}

/**
 * Adds a connection which we know is from an aggregator, so direct
 * pipe.  This is different from normal connections that we don't want
 * to count them, never expire them, and want to recognise them when
 * we're doing reloads.
 */
int
dispatch_addconnection_aggr(int sock)
{
	int conn = dispatch_addconnection(sock, NULL);

	if (conn == -1)
		return 1;

	connections[conn].noexpire = 1;
	connections[conn].isaggr = 1;
	acceptedconnections--;

	return 0;
}

inline static char
dispatch_process_dests(connection *conn, dispatcher *self, struct timeval now)
{
	int i;
	char force;

	if (conn->destlen > 0) {
		if (conn->maxsenddelay == 0)
			conn->maxsenddelay = ((rand() % 750) + 250) * 1000;
		/* force when aggr (don't stall it) or after timeout */
		force = conn->isaggr ? 1 :
			timediff(conn->lastwork, now) > conn->maxsenddelay;
		for (i = 0; i < conn->destlen; i++) {
			tracef("dispatcher %d, connfd %d, metric %s, queueing to %s:%d\n",
					self->id, conn->sock, conn->dests[i].metric,
					server_ip(conn->dests[i].dest),
					server_port(conn->dests[i].dest));
			if (server_send(conn->dests[i].dest, conn->dests[i].metric, force) == 0)
				break;
		}
		if (i < conn->destlen) {
			conn->destlen -= i;
			memmove(&conn->dests[0], &conn->dests[i],
					(sizeof(destination) * conn->destlen));
			return 0;
		} else {
			/* finally "complete" this metric */
			conn->destlen = 0;
			conn->lastwork = now;
			conn->hadwork = 1;
		}
	}

	return 1;
}

#define IDLE_DISCONNECT_TIME  (10 * 60 * 1000 * 1000)  /* 10 minutes */
/**
 * Look at conn and see if works needs to be done.  If so, do it.  This
 * function operates on an (exclusive) lock on the connection it serves.
 * Schematically, what this function does is like this:
 *
 *   read (partial) data  <----
 *         |                   |
 *         v                   |
 *   split and clean metrics   |
 *         |                   |
 *         v                   |
 *   route metrics             | feedback loop
 *         |                   | (stall client)
 *         v                   |
 *   send 1st attempt          |
 *         \                   |
 *          v*                 | * this is optional, but if a server's
 *   retry send (<1s)  --------    queue is full, the client is stalled
 *      block reads
 */
static int
dispatch_connection(connection *conn, dispatcher *self, struct timeval start)
{
	char *p, *q, *firstspace, *lastnl;
	char search_tags;
	int len;

	/* first try to resume any work being blocked */
	if (dispatch_process_dests(conn, self, start) == 0) {
		__sync_bool_compare_and_swap(&(conn->takenby), self->id, 0);
		return 0;
	}

	/* don't poll (read) when the last time we ran nothing happened,
	 * this is to avoid excessive CPU usage, issue #126 */
	if (!conn->hadwork && timediff(conn->lastwork, start) < 100 * 1000) {
		__sync_bool_compare_and_swap(&(conn->takenby), self->id, 0);
		return 0;
	}
	conn->hadwork = 0;

	len = -2;
	/* try to read more data, if that succeeds, or we still have data
	 * left in the buffer, try to process the buffer */
	if (
			(!conn->needmore && conn->buflen > 0) ||
			(len = conn->strm->strmread(conn->strm,
						conn->buf + conn->buflen,
						(sizeof(conn->buf) - 1) - conn->buflen)) > 0
	   )
	{
		if (len > 0) {
			conn->buflen += len;
			tracef("dispatcher %d, connfd %d, read %d bytes from socket\n",
					self->id, conn->sock, len);
#ifdef ENABLE_TRACE
			conn->buf[conn->buflen] = '\0';
#endif
		}

		/* Metrics look like this: metric_path value timestamp\n
		 * due to various messups we need to sanitise the
		 * metrics_path here, to ensure we can calculate the metric
		 * name off the filesystem path (and actually retrieve it in
		 * the web interface).
		 * Since tag support, metrics can look like this:
		 *   metric_path[;tag=value ...] value timestamp\n
		 * where the tag=value part can be repeated.  It should not be
		 * sanitised, however. */
		q = conn->metric;
		firstspace = NULL;
		lastnl = NULL;
		search_tags = self->tags_supported;
		for (p = conn->buf; p - conn->buf < conn->buflen; p++) {
			if (*p == '\n' || *p == '\r') {
				/* end of metric */
				lastnl = p;

				/* just a newline on it's own? some random garbage?
				 * do we exceed the set limits? drop */
				if (q == conn->metric || firstspace == NULL ||
						q - conn->metric > self->maxinplen - 1 ||
						firstspace - conn->metric > self->maxmetriclen)
				{
					__sync_add_and_fetch(&(self->discards), 1);
					q = conn->metric;
					firstspace = NULL;
					continue;
				}

				__sync_add_and_fetch(&(self->metrics), 1);
				/* add newline and terminate the string, we can do this
				 * because we substract one from buf and we always store
				 * a full metric in buf before we copy it to metric
				 * (which is of the same size) */
				*q++ = '\n';
				*q = '\0';

				/* perform routing of this metric */
				tracef("dispatcher %d, connfd %d, metric %s",
						self->id, conn->sock, conn->metric);
				__sync_add_and_fetch(&(self->blackholes),
						router_route(self->rtr,
							conn->dests, &conn->destlen, CONN_DESTS_SIZE,
							conn->srcaddr,
							conn->metric, firstspace, self->id - 1));
				tracef("dispatcher %d, connfd %d, destinations %zd\n",
						self->id, conn->sock, conn->destlen);

				/* restart building new one from the start */
				q = conn->metric;
				firstspace = NULL;
				search_tags = self->tags_supported;

				conn->hadwork = 1;
				gettimeofday(&conn->lastwork, NULL);
				conn->maxsenddelay = 0;
				/* send the metric to where it is supposed to go */
				if (dispatch_process_dests(conn, self, conn->lastwork) == 0)
					break;
			} else if (*p == ' ' || *p == '\t' || *p == '.') {
				/* separator */
				if (q == conn->metric) {
					/* make sure we skip this on next iteration to
					 * avoid an infinite loop, issues #8 and #51 */
					lastnl = p;
					continue;
				}
				if (*p == '\t')
					*p = ' ';
				if (*p == ' ' && firstspace == NULL) {
					if (*(q - 1) == '.')
						q--;  /* strip trailing separator */
					firstspace = q;
					*q++ = ' ';
				} else {
					/* metric_path separator or space,
					 * - duplicate elimination
					 * - don't start with separator/space */
					if (*(q - 1) != *p && (q - 1) != firstspace)
						*q++ = *p;
				}
			} else if (search_tags && *p == ';') {
				/* copy up to next space */
				search_tags = 0;
				firstspace = q;
				*q++ = *p;
			} else if (
					*p != '\0' && (
						firstspace != NULL ||
						(*p >= 'a' && *p <= 'z') ||
						(*p >= 'A' && *p <= 'Z') ||
						(*p >= '0' && *p <= '9') ||
						strchr(self->allowed_chars, *p)
					)
				)
			{
				/* copy char */
				*q++ = *p;
			} else {
				/* something barf, replace by underscore */
				*q++ = '_';
			}
		}
		conn->needmore = q != conn->metric;
		if (lastnl != NULL) {
			/* move remaining stuff to the front */
			conn->buflen -= lastnl + 1 - conn->buf;
			tracef("dispatcher %d, conn->buf: %p, lastnl: %p, diff: %zd, "
					"conn->buflen: %d, sizeof(conn->buf): %lu, "
					"memmove(%d, %lu, %d)\n",
					self->id,
					conn->buf, lastnl, lastnl - conn->buf,
					conn->buflen, sizeof(conn->buf),
					0, lastnl + 1 - conn->buf, conn->buflen + 1);
			tracef("dispatcher %d, pre conn->buf: %s\n", self->id, conn->buf);
			/* copy last NULL-byte for debug tracing */
			memmove(conn->buf, lastnl + 1, conn->buflen + 1);
			tracef("dispatcher %d, post conn->buf: %s\n", self->id, conn->buf);
			lastnl = NULL;
		}
	}
	if (len == -1 && (errno == EINTR ||
				errno == EAGAIN ||
				errno == EWOULDBLOCK))
	{
		/* nothing available/no work done */
		struct timeval stop;
		gettimeofday(&stop, NULL);
		if (!conn->noexpire &&
				timediff(conn->lastwork, stop) > IDLE_DISCONNECT_TIME)
		{
			/* force close connection below */
			len = 0;
		} else {
			__sync_bool_compare_and_swap(&(conn->takenby), self->id, 0);
			return 0;
		}
	}
	if (len == -1 || len == 0 || conn->isudp) {  /* error + EOF */
		/* we also disconnect the client in this case if our reading
		 * buffer is full, but we still need more (read returns 0 if the
		 * size argument is 0) -> this is good, because we can't do much
		 * with such client */

		if (conn->isaggr || conn->isudp) {
			/* reset buffer only (UDP/aggregations) and move on */
			conn->needmore = 1;
			conn->buflen = 0;
			__sync_bool_compare_and_swap(&(conn->takenby), self->id, 0);

			return len > 0;
		} else if (conn->destlen == 0) {
			tracef("dispatcher: %d, connfd: %d, len: %d [%s], disconnecting\n",
					self->id, conn->sock, len,
					len < 0 ? strerror(errno) : "");
			__sync_add_and_fetch(&closedconnections, 1);
			conn->strm->strmclose(conn->strm);

			/* flag this connection as no longer in use, unless there is
			 * pending metrics to send */
			__sync_bool_compare_and_swap(&(conn->takenby), self->id, -1);

			return 0;
		}
	}

	/* "release" this connection again */
	__sync_bool_compare_and_swap(&(conn->takenby), self->id, 0);

	return 1;
}

/**
 * pthread compatible routine that handles connections and processes
 * whatever comes in on those.
 */
static void *
dispatch_runner(void *arg)
{
	dispatcher *self = (dispatcher *)arg;
	connection *conn;
	int c;

	if (self->type == LISTENER) {
		struct pollfd ufds[MAX_LISTENERS];
		int fds;
		int *sock;
		while (__sync_bool_compare_and_swap(&(self->keep_running), 1, 1)) {
			pthread_rwlock_rdlock(&listenerslock);
			fds = 0;
			for (c = 0; c < MAX_LISTENERS; c++) {
				if (listeners[c] == NULL)
					continue;
				for (sock = listeners[c]->socks; *sock != -1; sock++) {
					ufds[fds].fd = *sock;
					ufds[fds].events = POLLIN;
					fds++;
				}
			}
			pthread_rwlock_unlock(&listenerslock);
			if (poll(ufds, fds, 1000) > 0) {
				int f;
				for (f = fds - 1; f >= 0; f--) {
					if (ufds[f].revents & POLLIN) {
						int client;
						struct sockaddr addr;
						socklen_t addrlen = sizeof(addr);

						if ((client = accept(ufds[f].fd, &addr, &addrlen)) < 0)
						{
							logerr("dispatch: failed to "
									"accept() new connection: %s\n",
									strerror(errno));
							dispatch_check_rlimit_and_warn();
							continue;
						}
						pthread_rwlock_rdlock(&listenerslock);
						for (c = 0; c < MAX_LISTENERS; c++) {
							if (listeners[c] == NULL)
								continue;
							for (sock = listeners[c]->socks; *sock != -1; sock++) {
								if (ufds[f].fd == *sock)
									break;
							}
							if (*sock != -1)
								break;
						}
						pthread_rwlock_unlock(&listenerslock);
						if (c == MAX_LISTENERS) {
							logerr("dispatch: could not find listener for "
									"socket, rejecting connection\n");
							close(client);
							continue;
						}
						if (dispatch_addconnection(client, listeners[c]) == -1) {
							close(client);
							continue;
						}
					}
				}
			}
		}
	} else if (self->type == CONNECTION) {
		int work;
		struct timeval start, stop;

		while (__sync_bool_compare_and_swap(&(self->keep_running), 1, 1)) {
			work = 0;

			if (__sync_bool_compare_and_swap(&(self->route_refresh_pending), 1, 1)) {
				self->rtr = self->pending_rtr;
				self->pending_rtr = NULL;
				__sync_bool_compare_and_swap(&(self->route_refresh_pending), 1, 0);
				__sync_and_and_fetch(&(self->hold), 0);
			}

			gettimeofday(&start, NULL);
			pthread_rwlock_rdlock(&connectionslock);
			for (c = 0; c < connectionslen; c++) {
				conn = &(connections[c]);
				/* atomically try to "claim" this connection */
				if (!__sync_bool_compare_and_swap(
							&(conn->takenby), 0, self->id))
					continue;
				if (__sync_bool_compare_and_swap(
							&(self->hold), 1, 1) && !conn->isaggr)
				{
					__sync_bool_compare_and_swap(
							&(conn->takenby), self->id, 0);
					continue;
				}
				work += dispatch_connection(conn, self, start);
			}
			pthread_rwlock_unlock(&connectionslock);
			gettimeofday(&stop, NULL);
			__sync_add_and_fetch(&(self->ticks), timediff(start, stop));

			/* nothing done, avoid spinlocking */
			if (__sync_bool_compare_and_swap(&(self->keep_running), 1, 1) &&
					work == 0)
			{
				gettimeofday(&start, NULL);
				usleep((100 + (rand() % 200)) * 1000);  /* 100ms - 300ms */
				gettimeofday(&stop, NULL);
				__sync_add_and_fetch(&(self->sleeps), timediff(start, stop));
			}
		}
	} else {
		logerr("huh? unknown self type!\n");
	}

	return NULL;
}

/**
 * Starts a new dispatcher for the given type and with the given id.
 * Returns its handle.
 */
static dispatcher *
dispatch_new(
		unsigned char id,
		enum conntype type,
		router *r,
		char *allowed_chars,
		int maxinplen,
		int maxmetriclen
	)
{
	dispatcher *ret = malloc(sizeof(dispatcher));

	if (ret == NULL)
		return NULL;
	if (type == CONNECTION && r == NULL) {
		free(ret);
		return NULL;
	}

	ret->id = id + 1;  /* ensure > 0 */
	ret->type = type;
	ret->keep_running = 1;
	ret->rtr = r;
	ret->route_refresh_pending = 0;
	ret->hold = 0;
	ret->allowed_chars = allowed_chars;
	ret->tags_supported = 0;
	ret->maxinplen = maxinplen;
	ret->maxmetriclen = maxmetriclen;

	ret->metrics = 0;
	ret->blackholes = 0;
	ret->discards = 0;
	ret->ticks = 0;
	ret->sleeps = 0;
	ret->prevmetrics = 0;
	ret->prevblackholes = 0;
	ret->prevticks = 0;
	ret->prevsleeps = 0;

	/* switch tag support on when the user didn't allow ';' as valid
	 * character in metrics */
	if (allowed_chars != NULL && strchr(allowed_chars, ';') == NULL)
		ret->tags_supported = 1;

	if (pthread_create(&ret->tid, NULL, dispatch_runner, ret) != 0) {
		free(ret);
		return NULL;
	}

	return ret;
}

void
dispatch_set_bufsize(unsigned int nsockbufsize)
{
	sockbufsize = nsockbufsize;
}

/**
 * Initialise the listeners array.  This is a one-time allocation that
 * currently never is extended.  This code does no locking, as it
 * assumes to be run before any access to listeners occur.
 */
char
dispatch_init_listeners()
{
	int i;
	/* once all-or-nothing allocation */
	if ((listeners = malloc(sizeof(listener *) * MAX_LISTENERS)) == NULL)
		return 1;
	for (i = 0; i < MAX_LISTENERS; i++)
		listeners[i] = NULL;

	return 0;
}

/**
 * Starts a new dispatcher specialised in handling incoming connections
 * (and putting them on the queue for handling the connections).
 */
dispatcher *
dispatch_new_listener(unsigned char id)
{
	return dispatch_new(id, LISTENER, NULL, NULL, 0, 0);
}

/**
 * Starts a new dispatcher specialised in handling incoming data on
 * existing connections.
 */
dispatcher *
dispatch_new_connection(unsigned char id, router *r, char *allowed_chars,
		int maxinplen, int maxmetriclen)
{
	return dispatch_new(id, CONNECTION, r, allowed_chars,
			maxinplen, maxmetriclen);
}

/**
 * Signals this dispatcher to stop whatever it's doing.
 */
void
dispatch_stop(dispatcher *d)
{
	__sync_bool_compare_and_swap(&(d->keep_running), 1, 0);
}

/**
 * Shuts down dispatcher d.  Returns when the dispatcher has terminated.
 */
void
dispatch_shutdown(dispatcher *d)
{
	dispatch_stop(d);
	pthread_join(d->tid, NULL);
}

/**
 * Free up resources taken by dispatcher d.  The caller should make sure
 * the dispatcher has been shut down at this point.
 */
void
dispatch_free(dispatcher *d)
{
	free(d);
}

/**
 * Requests this dispatcher to stop processing connections.  As soon as
 * schedulereload finishes reloading the routes, this dispatcher will
 * un-hold and continue processing connections.
 * Returns when the dispatcher is no longer doing work.
 */

inline void
dispatch_hold(dispatcher *d)
{
	__sync_bool_compare_and_swap(&(d->hold), 0, 1);
}

/**
 * Schedules routes r to be put in place for the current routes.  The
 * replacement is performed at the next cycle of the dispatcher.
 */
inline void
dispatch_schedulereload(dispatcher *d, router *r)
{
	d->pending_rtr = r;
	__sync_bool_compare_and_swap(&(d->route_refresh_pending), 0, 1);
}

/**
 * Returns true if the routes scheduled to be reloaded by a call to
 * dispatch_schedulereload() have been activated.
 */
inline char
dispatch_reloadcomplete(dispatcher *d)
{
	return __sync_bool_compare_and_swap(&(d->route_refresh_pending), 0, 0);
}

/**
 * Returns the wall-clock time in milliseconds consumed by this dispatcher.
 */
inline size_t
dispatch_get_ticks(dispatcher *self)
{
	return __sync_add_and_fetch(&(self->ticks), 0);
}

/**
 * Returns the wall-clock time consumed since last call to this
 * function.
 */
inline size_t
dispatch_get_ticks_sub(dispatcher *self)
{
	size_t d = dispatch_get_ticks(self) - self->prevticks;
	self->prevticks += d;
	return d;
}

/**
 * Returns the wall-clock time in milliseconds consumed while sleeping
 * by this dispatcher.
 */
inline size_t
dispatch_get_sleeps(dispatcher *self)
{
	return __sync_add_and_fetch(&(self->sleeps), 0);
}

/**
 * Returns the wall-clock time consumed while sleeping since last call
 * to this function.
 */
inline size_t
dispatch_get_sleeps_sub(dispatcher *self)
{
	size_t d = dispatch_get_sleeps(self) - self->prevsleeps;
	self->prevsleeps += d;
	return d;
}

/**
 * Returns the number of metrics dispatched since start.
 */
inline size_t
dispatch_get_metrics(dispatcher *self)
{
	return __sync_add_and_fetch(&(self->metrics), 0);
}

/**
 * Returns the number of metrics dispatched since last call to this
 * function.
 */
inline size_t
dispatch_get_metrics_sub(dispatcher *self)
{
	size_t d = dispatch_get_metrics(self) - self->prevmetrics;
	self->prevmetrics += d;
	return d;
}

/**
 * Returns the number of metrics that were explicitly or implicitly
 * blackholed since start.
 */
inline size_t
dispatch_get_blackholes(dispatcher *self)
{
	return __sync_add_and_fetch(&(self->blackholes), 0);
}

/**
 * Returns the number of metrics that were blackholed since last call to
 * this function.
 */
inline size_t
dispatch_get_blackholes_sub(dispatcher *self)
{
	size_t d = dispatch_get_blackholes(self) - self->prevblackholes;
	self->prevblackholes += d;
	return d;
}

/**
 * Returns the number of metrics that were discarded since start.
 */
inline size_t
dispatch_get_discards(dispatcher *self)
{
	return __sync_add_and_fetch(&(self->discards), 0);
}

/**
 * Returns the number of metrics that were discarded since last call to
 * this function.
 */
inline size_t
dispatch_get_discards_sub(dispatcher *self)
{
	size_t d = dispatch_get_discards(self) - self->prevdiscards;
	self->prevdiscards += d;
	return d;
}

/**
 * Returns the number of accepted connections thusfar.
 */
inline size_t
dispatch_get_accepted_connections(void)
{
	return __sync_add_and_fetch(&(acceptedconnections), 0);
}

/**
 * Returns the number of closed connections thusfar.
 */
inline size_t
dispatch_get_closed_connections(void)
{
	return __sync_add_and_fetch(&(closedconnections), 0);
}
