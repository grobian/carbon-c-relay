/*
 * Copyright 2013-2022 Fabian Groffen
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
#include <sys/time.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <netinet/in.h>
#include <errno.h>

#include "relay.h"
#include "router.h"

#ifdef HAVE_SSL
#include <openssl/ssl.h>
#include <openssl/err.h>

static char
ssllisten(listener *lsnr)
{
	/* create a auto-negotiate context */
	const SSL_METHOD *m;

#if defined(HAVE_TLS_SERVER_METHOD)
	m = TLS_server_method();
#elif defined(HAVE_SSLV23_SERVER_METHOD)
	m = SSLv23_server_method();
#else
# error "we don't understand this version of OpenSSL"
#endif
	lsnr->ctx = SSL_CTX_new(m);
	if (lsnr->ctx == NULL) {
		char *err = ERR_error_string(ERR_get_error(), NULL);
		logerr("failed to create SSL context for listener on port %d: %s\n",
				lsnr->port, err);
		return 1;
	}

#if defined(HAVE_TLS_SERVER_METHOD)
	/* handle possible restrictions to the flexible version method */
	if (lsnr->protomin != 0) {
		if (SSL_CTX_set_min_proto_version(lsnr->ctx, lsnr->protomin) == 0) {
			char *err = ERR_error_string(ERR_get_error(), NULL);
			logerr("SSL protomin version invalid: %s\n", err);
			return 1;
		}
	}
	if (lsnr->protomax != 0) {
		if (SSL_CTX_set_max_proto_version(lsnr->ctx, lsnr->protomax) == 0) {
			char *err = ERR_error_string(ERR_get_error(), NULL);
			logerr("SSL protomax version invalid: %s\n", err);
			return 1;
		}
	}
#elif defined(HAVE_SSLV23_SERVER_METHOD)
	/* handle restrictions by mapping the range of versions manually */
	if (lsnr->protomin > 0 || lsnr->protomax > 0) {
		long options = 0;

		/* this is OpenSSL <1.1 which doesn't understand TLS1.3, we have
		 * available:
		 * SSL_OP_NO_SSLv2,
		 * SSL_OP_NO_SSLv3,
		 * SSL_OP_NO_TLSv1,
		 * SSL_OP_NO_TLSv1_1 and
		 * SSL_OP_NO_TLSv1_2 */
		if (lsnr->protomax > 0) {
			switch (lsnr->protomax) {
				case SSL3_VERSION:
					options |= SSL_OP_NO_TLSv1;
				case TLS1_VERSION:
					options |= SSL_OP_NO_TLSv1_1;
				case TLS1_1_VERSION:
					options |= SSL_OP_NO_TLSv1_2;
				case TLS1_2_VERSION:
				case TLS1_3_VERSION:
					break;
			}
		}
		if (lsnr->protomin > 0) {
			switch (lsnr->protomin) {
				case TLS1_3_VERSION:
					options |= SSL_OP_NO_TLSv1_2;
				case TLS1_2_VERSION:
					options |= SSL_OP_NO_TLSv1_1;
				case TLS1_1_VERSION:
					options |= SSL_OP_NO_TLSv1;
				case TLS1_VERSION:
					options |= SSL_OP_NO_SSLv3;
				case SSL3_VERSION:
					break;
			}
		}
		SSL_CTX_set_options(lsnr->ctx, options);
	}
#else
# error "we don't understand this version of OpenSSL"
#endif
#ifdef HAVE_SSL_CTX_SET_CIPHER_LIST
	if (lsnr->ciphers != NULL) {
		if (SSL_CTX_set_cipher_list(lsnr->ctx, lsnr->ciphers) == 0) {
			char *err = ERR_error_string(ERR_get_error(), NULL);
			logerr("cannot set SSL cipher list: %s\n", err);
			return 1;
		}
	}
#endif
#ifdef HAVE_SSL_CTX_SET_CIPHERSUITES
	if (lsnr->ciphersuites != NULL) {
		if (SSL_CTX_set_ciphersuites(lsnr->ctx, lsnr->ciphersuites) == 0) {
			char *err = ERR_error_string(ERR_get_error(), NULL);
			logerr("cannot set TLS ciphersuites: %s\n", err);
			return 1;
		}
	}
#endif

	/* load certificates */
	if (SSL_CTX_use_certificate_file(
				lsnr->ctx, lsnr->pemcert, SSL_FILETYPE_PEM) <= 0)
	{
		char *err = ERR_error_string(ERR_get_error(), NULL);
		SSL_CTX_free(lsnr->ctx);
		logerr("cannot load certificate from %s: %s\n", lsnr->pemcert, err);
		return 1;
	}
	if (SSL_CTX_use_PrivateKey_file(
				lsnr->ctx, lsnr->pemcert, SSL_FILETYPE_PEM) <= 0)
	{
		char *err = ERR_error_string(ERR_get_error(), NULL);
		SSL_CTX_free(lsnr->ctx);
		logerr("cannot load private key from %s: %s\n", lsnr->pemcert, err);
		return 1;
	}
	if (!SSL_CTX_check_private_key(lsnr->ctx)) {
		char *err = ERR_error_string(ERR_get_error(), NULL);
		SSL_CTX_free(lsnr->ctx);
		logerr("private key does not match public certificate in %s: %s\n",
				lsnr->pemcert, err);
		return 1;
	}
	if (lsnr->transport & W_MTLS && sslCA != NULL) {  /* issue #444 */
		if (SSL_CTX_load_verify_locations(lsnr->ctx,
										  sslCAisdir ? NULL : sslCA,
										  sslCAisdir ? sslCA : NULL) == 0)
		{
			char *err = ERR_error_string(ERR_get_error(), NULL);
			SSL_CTX_free(lsnr->ctx);
			logerr("failed to load SSL verify locations from %s for "
				   "mTLS connection: %s\n", sslCA, err);
			return 1;
		}
	}
	return 0;
}
#endif

static int
bindlistenip(listener *lsnr, unsigned int backlog)
{
	int sock;
	int optval;
	struct timeval tv;
	struct addrinfo *resw;
	char saddr[INET6_ADDRSTRLEN];
	int binderr = 0;
	int sockcur = 0;

#ifdef HAVE_SSL
	if (lsnr->transport & W_SSL && ssllisten(lsnr))
		return 1;
#endif

	tv.tv_sec = 0;
	tv.tv_usec = 500 * 1000;

	for (resw = lsnr->saddrs; resw != NULL; resw = resw->ai_next) {
		if (resw->ai_family != PF_INET && resw->ai_family != PF_INET6)
			continue;
		if (resw->ai_protocol != IPPROTO_TCP &&
				resw->ai_protocol != IPPROTO_UDP)
			continue;

		saddr_ntop(resw, saddr);
		if (saddr[0] == '\0')
			snprintf(saddr, sizeof(saddr), "(unknown)");

		if ((sock = socket(resw->ai_family, resw->ai_socktype,
						resw->ai_protocol)) < 0)
		{
			if (errno == EAFNOSUPPORT &&
					(sockcur > 0 || resw->ai_next != NULL))
			{
				/* ignore "address family not supported by protocol" for
				 * systems with ipv6 disabled, issue #296 */
				continue;
			}
			logerr("failed to create socket for %s%s%s: %s\n",
					resw->ai_family == PF_INET ? "[" : "",
					saddr,
					resw->ai_family == PF_INET ? "]" : "",
					strerror(errno));
			binderr = 1;
			break;
		}
		lsnr->socks[sockcur++] = sock;

		(void) setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
		optval = 1;  /* allow takeover */
		(void) setsockopt(sock, SOL_SOCKET, SO_REUSEADDR,
				&optval, sizeof(optval));
		if (resw->ai_family == PF_INET6) {
			optval = 1;
			(void) setsockopt(sock, IPPROTO_IPV6, IPV6_V6ONLY,
					&optval, sizeof(optval));
		}

		if (bind(sock, resw->ai_addr, resw->ai_addrlen) < 0) {
			logerr("failed to bind on %s%d %s port %u: %s\n",
					resw->ai_protocol == IPPROTO_TCP ? "tcp" : "udp",
					resw->ai_family == PF_INET6 ? 6 : 4,
					saddr, lsnr->port, strerror(errno));
			binderr = 1;
			break;
		}

		if (resw->ai_protocol == IPPROTO_TCP) {
			if (listen(sock, backlog) < 0) {
				logerr("failed to listen on tcp%d %s port %u: %s\n",
						resw->ai_family == PF_INET6 ? 6 : 4,
						saddr, lsnr->port, strerror(errno));
				binderr = 1;
				break;
			}
			logout("listening on tcp%d %s port %u\n",
					resw->ai_family == PF_INET6 ? 6 : 4, saddr, lsnr->port);
		} else {
			logout("listening on udp%d %s port %u\n",
					resw->ai_family == PF_INET6 ? 6 : 4, saddr, lsnr->port);
		}
	}
	if (binderr != 0) {
		/* close all opened sockets */
		for (--sockcur; sockcur >= 0; sockcur--)
			close(lsnr->socks[sockcur]);
		return 1;
	}
	lsnr->socks[sockcur] = -1;

	return 0;
}

static int
bindlistenunix(listener *lsnr, unsigned int backlog)
{
	struct sockaddr_un srvr;
	int sock;

#ifdef HAVE_SSL
	if (lsnr->transport & W_SSL && ssllisten(lsnr))
		return 1;
#endif

#ifndef PF_LOCAL
# define PF_LOCAL PF_UNIX
#endif
	if ((sock = socket(PF_LOCAL, SOCK_STREAM, 0)) < 0) {
		logerr("failed to create socket for %s: %s\n",
				lsnr->ip, strerror(errno));
		return 1;
	}
	lsnr->socks[0] = sock;
	lsnr->socks[1] = -1;

	memset(&srvr, 0, sizeof(struct sockaddr_un));
	srvr.sun_family = PF_LOCAL;
	strncpy(srvr.sun_path, lsnr->ip, sizeof(srvr.sun_path) - 1);

	unlink(lsnr->ip);  /* avoid address already in use */
	if (bind(sock, (struct sockaddr *)&srvr,
				sizeof(struct sockaddr_un)) < 0)
	{
		logerr("failed to bind for %s: %s\n", lsnr->ip, strerror(errno));
		close(sock);
		return 1;
	}

	if (listen(sock, backlog) < 0) {
		logerr("failed to listen on %s: %s\n", lsnr->ip, strerror(errno));
		close(sock);
		return 1;
	}

	logout("listening on UNIX socket %s\n", lsnr->ip);

	return 0;
}

/**
 * Open up sockets associated with listener.  Returns 0 when opening up
 * the listener succeeded, 1 otherwise.
 */
int
bindlisten(listener *lsnr, unsigned int backlog)
{
	switch (lsnr->ctype) {
		case CON_TCP:
		case CON_UDP:
			return bindlistenip(lsnr, backlog);
		case CON_UNIX:
			return bindlistenunix(lsnr, backlog);
		default:
			logerr("unsupported listener type");
			return 1;
	}
}

static void
close_socks(listener *lsnr)
{
	int i;
	for (i = 0; lsnr->socks[i] != -1; i++)
		close(lsnr->socks[i]);
#ifdef HAVE_SSL
	if (lsnr->transport & W_SSL)
		SSL_CTX_free(lsnr->ctx);
#endif
	logout("closed listener for %s %s:%u\n",
			lsnr->ctype == CON_UDP ? "udp" : "tcp",
			lsnr->ip == NULL ? "" : lsnr->ip, lsnr->port);
}

static void
destroy_usock(listener *lsnr)
{
	close(lsnr->socks[0]);
	unlink(lsnr->ip);
#ifdef HAVE_SSL
	if (lsnr->transport & W_SSL)
		SSL_CTX_free(lsnr->ctx);
#endif
	logout("removed listener for %s\n", lsnr->ip);
}

void
shutdownclose(listener *lsnr)
{
	switch (lsnr->ctype) {
		case CON_TCP:
		case CON_UDP:
			close_socks(lsnr);
			break;
		case CON_UNIX:
			destroy_usock(lsnr);
			break;
		default:
			logerr("unsupported listener type");
			break;
	}
}
