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


#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <unistd.h>
#include <netdb.h>
#include <string.h>
#include <ctype.h>
#include <signal.h>
#include <time.h>
#include <sys/socket.h>
#include <errno.h>
#include <assert.h>

#if defined(__MACH__) || defined(BSD)
# include <sys/types.h>
# include <sys/sysctl.h>
#endif

#include "relay.h"
#include "server.h"
#include "router.h"
#include "receptor.h"
#include "dispatcher.h"
#include "aggregator.h"
#include "collector.h"
#include "conffile.h"
#include "posixregex.h"

unsigned char keep_running = 1;
int pending_signal = -1;
char relay_hostname[256];
unsigned char mode = 0;

static char *config = NULL;
static int batchsize = 2500;
static int queuesize = 25000;
static int maxstalls = 4;
static unsigned short iotimeout = 600;
static unsigned short listenport = GRAPHITE_PORT;
static int optimiserthreshold = 50;
static int sockbufsize = 0;
static int collector_interval = 60;
static unsigned int listenbacklog = 32;
static col_mode smode = CUM;
static dispatcher **workers = NULL;
static char workercnt = 0;
static router *rtr = NULL;
static server *internal_submission = NULL;
static char *relay_logfile = NULL;
static FILE *relay_stdout = NULL;
static FILE *relay_stderr = NULL;
static char relay_can_log = 0;


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
	struct tm tm_now;
	FILE *dst = NULL;
	char console = 0;
	int ret;

	/* briefly stall if we're swapping fds */
	while (!__sync_add_and_fetch(&relay_can_log, 0))
		usleep((100 + (rand() % 200)) * 1000);  /* 100ms - 300ms */

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
	/* NULL == /dev/null */
	if (dst == NULL)
		return 0;

	time(&now);
	localtime_r(&now, &tm_now);
	len = strftime(prefix, sizeof(prefix), "[%Y-%m-%d %H:%M:%S]", &tm_now);

	if (!console)
		(void)snprintf(prefix + len, sizeof(prefix) - len, " (%s)", dest == LOGOUT ? "MSG" : "ERR");

	fprintf(dst, "%s ", prefix);

	va_start(ap, fmt);

	ret = vfprintf(dst, fmt, ap);
	fflush(dst);

	va_end(ap);

	return ret;
}

static void
sig_handler(int sig)
{
	/* a thunder of signals will be ignored for as long as a pending
	 * signal was set, I can live with that, feels like a bad practice
	 * to bombard a process like that */
	(void)__sync_bool_compare_and_swap(&pending_signal, -1, sig);
}

static void
do_reload(void)
{
	router *newrtr;
	aggregator *newaggrs;
	int id;
	FILE *newfd;
	size_t numaggregators;
	listener *lsnrs;

	if (relay_stderr != stderr) {
		/* try to re-open the file first, so we can still try and say
		 * something if that fails */
		if ((newfd = fopen(relay_logfile, "a")) == NULL) {
			logerr("not reopening logfiles: can't open '%s': %s\n",
					relay_logfile, strerror(errno));
		} else {
			logout("closing logfile\n");
			__sync_and_and_fetch(&relay_can_log, 0);
			/* lame race avoidance for relaylog() usage */
			usleep((100 + (rand() % 200)) * 1000);  /* 100ms - 300ms */
			fclose(relay_stderr);
			relay_stdout = newfd;
			relay_stderr = newfd;
			__sync_bool_compare_and_swap(&relay_can_log, 0, 1);
			logout("reopening logfile\n");
		}
	}

	logout("reloading config from '%s'\n", config);
	if ((newrtr = router_readconfig(NULL, config, workercnt,
					queuesize, batchsize, maxstalls,
					iotimeout, sockbufsize, listenport)) == NULL)
	{
		logerr("failed to read configuration '%s', aborting reload\n", config);
		return;
	}
	router_optimise(newrtr, optimiserthreshold);

	/* compare the configs first, if there is no diff, then
	 * just refrain from reloading anything */
	if (!router_printdiffs(rtr, newrtr, relay_stdout)) {
		/* no changes, shortcut */
		logout("no config changes found\n");
		router_free(newrtr);
		logout("SIGHUP handler complete\n");
		return;
	}

	/* close connections that are not in the new config first */
	lsnrs = router_get_listeners(rtr);
	for ( ; lsnrs != NULL; lsnrs = lsnrs->next) {
		if (router_contains_listener(newrtr, lsnrs) == NULL) {
			dispatch_removelistener(lsnrs);
			shutdownclose(lsnrs);
		}
	}

	logout("reloading collector\n");
	collector_schedulereload(newrtr);
	while (!collector_reloadcomplete())
		usleep((100 + (rand() % 200)) * 1000);  /* 100ms - 300ms */

	/* During aggregator final expiry, the dispatchers should not put
	 * new metrics, so we have to temporarily stop them processing
	 * connections.  However, we still need to handle aggregations that
	 * we will stop next, so this call results in the dispatchers only
	 * dealing with connections from aggregators.  Doing this, also
	 * means that we can reload the config atomicly, which disrupts the
	 * service seemingly for a bit, but results in less confusing output
	 * in the end. */
	logout("interrupting workers\n");
	for (id = 1; id < 1 + workercnt; id++)
		dispatch_hold(workers[id]);

	numaggregators = aggregator_numaggregators(router_getaggregators(rtr));
	if (numaggregators > 0) {
		logout("expiring aggregations\n");
		aggregator_stop();  /* frees aggrs */

		/* Now the aggregator has written everything to the
		 * dispatchers, which will hand over to the servers with proper
		 * metric adjustments (stubs). */
	}
	newaggrs = router_getaggregators(newrtr);
	numaggregators = aggregator_numaggregators(newaggrs);
	if (numaggregators > 0) {
		if (!aggregator_start(newaggrs)) {
			logerr("failed to start aggregator, aggregations will no "
					"longer produce output!\n");
		}
	}

	/* Transplant listeners so their open state won't get lost.  Open up
	 * connections for new listeners. */
	lsnrs = router_get_listeners(newrtr);
	for ( ; lsnrs != NULL; lsnrs = lsnrs->next) {
		listener *olsnr;
		if ((olsnr = router_contains_listener(rtr, lsnrs)) != NULL) {
			/* ensure we copy over the opened sockets/state */
			dispatch_transplantlistener(olsnr, lsnrs, newrtr);
		} else {
			if (bindlisten(lsnrs, listenbacklog) != 0) {
				logerr("failed to setup listener, "
						"this will impact the behaviour of the relay\n");
				continue;
			}
			if (dispatch_addlistener(lsnrs) != 0) {
				logerr("failed to listen to socket, the "
						"listener will not be effective\n");
			}
		}
	}

	/* Transplant the queues for the same servers, so we keep their
	 * queues (potentially with data) around.  To do so, we need to make
	 * sure the servers aren't being operated on. */
	router_shutdown(rtr);

	/* Because we know that ip:port:prot is unique in both sets, when we
	 * find a match, we can just swap the queue.  This comes with a
	 * slight peculiarity, that if a server becomes part of an any_of
	 * cluster (or failover), its pending queue can now be processed by
	 * other servers.  While this feels wrong, it can be extremely
	 * useful in case existing data needs to be migrated to another
	 * server, e.g. the config on purpose added a failover to offload a
	 * known dead server before removing it.  This is fairly specific
	 * but in reality this happens to be desirable every once so often. */
	router_transplant_queues(newrtr, rtr);

	/* Before we start the workers sending traffic, start up the
	 * servers now queues are transplanted. */
	router_start(newrtr);

	logout("reloading workers\n");
	for (id = 1; id < 1 + workercnt; id++)
		dispatch_schedulereload(workers[id], newrtr); /* un-holds */
	for (id = 1; id < 1 + workercnt; id++) {
		while (!dispatch_reloadcomplete(workers[id + 0]))
			usleep((100 + (rand() % 200)) * 1000);  /* 100ms - 300ms */
	}

	router_free(rtr);

	rtr = newrtr;

	logout("SIGHUP handler complete\n");
}

static void
handle_signal(int sig)
{
	char *sign = "unknown signal";

	switch (sig) {
		case SIGTERM:
			sign = "SIGTERM";
			break;
		case SIGINT:
			sign = "SIGINT";
			break;
		case SIGQUIT:
			sign = "SIGQUIT";
			break;
		case SIGHUP:
			sign = "SIGHUP";
			break;
	}
	if (keep_running) {
		logout("caught %s\n", sign);
	} else {
		/* also fine when SIGHUP */
		logerr("caught %s while already shutting down, "
				"forcing exit!\n", sign);
		exit(1);
	}
	if (sig == SIGHUP) {
		do_reload();
	} else {
		keep_running = 0;
	}
}

static int
get_cores(void)
{
	int cpus = 5;

#if defined(__MACH__) || defined(BSD)
	size_t len = sizeof(cpus);
	sysctlbyname("hw.ncpu", &cpus, &len, NULL, 0);
#else /* Linux and Solaris */
	cpus = sysconf(_SC_NPROCESSORS_ONLN);
#endif

	return cpus;
}

static void
do_version(void)
{
	printf("carbon-c-relay v" VERSION " (" GIT_VERSION ")\n");

	exit(0);
}

static void
do_usage(char *name, int exitcode)
{
	printf("Usage: %s -f <config> [opts]\n", name);
	printf("\n");
	printf("Options:\n");
	printf("  -v  print version and exit\n");
	printf("  -f  read <config> for clusters and routes\n");
	printf("  -p  listen on <port> for connections, defaults to %u\n",
			GRAPHITE_PORT);
	printf("  -l  write output to <file>, defaults to stdout/stderr\n");
	printf("  -w  use <workers> worker threads, defaults to %d\n", get_cores());
	printf("  -b  server send batch size, defaults to %d\n", batchsize);
	printf("  -q  server queue size, defaults to %d\n", queuesize);
	printf("  -L  server max stalls, defaults to %d\n", maxstalls);
	printf("  -S  statistics sending interval in seconds, defaults to 60\n");
	printf("  -B  connection listen backlog, defaults to 32\n");
	printf("  -U  socket receive buffer size, max/min/default values depend on OS\n");
	printf("  -T  IO timeout in milliseconds for server connections, defaults to %d\n", iotimeout);
	printf("  -m  send statistics like carbon-cache.py, e.g. not cumulative\n");
	printf("  -c  characters to allow next to [A-Za-z0-9], defaults to -_:#\n");
	printf("  -d  debug mode: currently writes statistics to log, prints hash\n"
	       "      ring contents and matching position in test mode (-t)\n");
#ifdef ENABLE_TRACE
	printf("      add another -d to enable (very noisy) trace mode\n");
#endif
	printf("  -s  submission mode: don't add any metrics to the stream like\n"
	       "      statistics, report drop counts and queue pressure to log\n");
	printf("  -t  config test mode: prints rule matches from input on stdin\n");
	printf("  -H  hostname: override hostname (used in statistics)\n");
	printf("  -D  daemonise: run in a background\n");
	printf("  -P  pidfile: write a pid to a specified pidfile\n");
	printf("  -O  minimum number of rules before optimising the ruleset, default: %d\n", optimiserthreshold); 

	exit(exitcode);
}

int
main(int argc, char * const argv[])
{
	char id;
	int ch;
	size_t numaggregators;
	char *allowed_chars = NULL;
	char *pidfile = NULL;
	FILE *pidfile_handle = NULL;
	aggregator *aggrs = NULL;
	int fds[2];
	unsigned char startup_success = 0;
	listener *lsnrs;

	if (gethostname(relay_hostname, sizeof(relay_hostname)) < 0)
		snprintf(relay_hostname, sizeof(relay_hostname), "127.0.0.1");

	while ((ch = getopt(argc, argv, ":hvdstf:l:p:w:b:q:L:T:c:H:B:U:DP:O:")) != -1) {
		switch (ch) {
			case 'v':
				do_version();
				break;
			case 'd':
				/* secret support for -dd (just trace) and -ddd (debug
				 * and trace) */
				if (mode & MODE_DEBUG) {
					mode |= MODE_TRACE;
					mode &= ~MODE_DEBUG;
				} else {
					mode |= MODE_DEBUG;
				}
				break;
			case 's':
				mode |= MODE_SUBMISSION;
				break;
			case 't':
				/* -t: test interactively, -tt: test config and exit */
				if (mode & MODE_TEST) {
					mode |= MODE_CONFIGTEST;
				} else {
					mode |= MODE_TEST;
				}
				break;
			case 'f':
				config = optarg;
				break;
			case 'l':
				relay_logfile = optarg;
				break;
			case 'p':
				listenport = (unsigned short)atoi(optarg);
				if (listenport == 0) {
					fprintf(stderr, "error: port needs to be a number >0\n");
					do_usage(argv[0], 1);
				}
				break;
			case 'w':
				workercnt = (char)atoi(optarg);
				if (workercnt <= 0) {
					fprintf(stderr, "error: workers needs to be a number >0\n");
					do_usage(argv[0], 1);
				}
				break;
			case 'b':
				batchsize = atoi(optarg);
				if (batchsize <= 0) {
					fprintf(stderr, "error: batch size needs to be a number >0\n");
					do_usage(argv[0], 1);
				}
				break;
			case 'q':
				queuesize = atoi(optarg);
				if (queuesize <= 0) {
					fprintf(stderr, "error: queue size needs to be a number >0\n");
					do_usage(argv[0], 1);
				}
				break;
			case 'L':
				maxstalls = atoi(optarg);
				if (maxstalls < 0 || maxstalls >= (1 << SERVER_STALL_BITS)) {
					fprintf(stderr, "error: maximum stalls needs to be a number "
							"between 0 and %d\n", (1 << SERVER_STALL_BITS) - 1);
					do_usage(argv[0], 1);
				}
				break;
			case 'T': {
				int val = atoi(optarg);
				if (val <= 0) {
					fprintf(stderr, "error: server IO timeout needs "
							"to be a number >0\n");
					do_usage(argv[0], 1);
				} else if (val >= 60000) {
					fprintf(stderr, "error: server IO timeout needs "
							"to be less than one minute\n");
					do_usage(argv[0], 1);
				}
				iotimeout = (unsigned short)val;
			}	break;
			case 'c':
				allowed_chars = optarg;
				break;
			case 'H':
				snprintf(relay_hostname, sizeof(relay_hostname), "%s", optarg);
				break;
			case 'B': {
				int val = atoi(optarg);
				if (val <= 0) {
					fprintf(stderr, "error: backlog needs to be a number >0\n");
					do_usage(argv[0], 1);
				}
				listenbacklog = (unsigned int)val;
			}	break;
			case 'U': {
				int val = atoi(optarg);
				socklen_t len = sizeof(sockbufsize);
				int sock;

				if (val <= 0) {
					fprintf(stderr, "error: bufsize needs to be a number >0\n");
					do_usage(argv[0], 1);
				}
				sockbufsize = (unsigned int)val;

				/* test the size to see if the value is valid, since
				 * this is OS dependant, it's the only way */
				sock = socket(PF_INET, SOCK_STREAM, 0);
				if (sock == -1 || setsockopt(sock, SOL_SOCKET, SO_RCVBUF,
						&sockbufsize, sizeof(sockbufsize)) != 0)
				{
					fprintf(stderr, "failed to set socket bufsize: %s\n",
							strerror(errno));
				}
				if (sock != -1 && getsockopt(sock, SOL_SOCKET, SO_RCVBUF,
							&sockbufsize, &len) != 0)
				{
					fprintf(stderr, "failed to get socket bufsize: %s\n",
							strerror(errno));
				}
				if (len != sizeof(sockbufsize)) {
					fprintf(stderr, "getsockopt returned unexpected size: %u, "
							"expected %zu\n", len, sizeof(sockbufsize));
					exit(1);
				}
				if (sockbufsize > (unsigned int)val)
					fprintf(stderr, "warning: OS rejected socket bufsize\n");
				if (sock != -1)
					close(sock);
			}	break;
			case 'D':
				mode |= MODE_DAEMON;
				break;
			case 'P':
				pidfile = optarg;
				break;
			case 'O': {
				int val = atoi(optarg);
				optimiserthreshold = val < 0 ? -1 : val;
			}	break;
			case '?':
			case ':':
				do_usage(argv[0], 1);
				break;
			case 'h':
			default:
				do_usage(argv[0], 0);
				break;
		}
	}
	if (optind == 1 || config == NULL)
		do_usage(argv[0], 1);


	/* seed randomiser for dispatcher and aggregator "splay" */
	srand(time(NULL));

	if (workercnt == 0)
		workercnt = mode & MODE_SUBMISSION ? 2 : get_cores();

	/* disable collector for submission mode */
	if (mode & MODE_SUBMISSION)
		collector_interval = 0;

	/* any_of failover maths need batchsize to be smaller than queuesize */
	if (batchsize > queuesize) {
		fprintf(stderr, "error: batchsize must be smaller than queuesize\n");
		exit(1);
	}

	if (relay_logfile != NULL && (mode & MODE_TEST) == 0) {
		FILE *f = fopen(relay_logfile, "a");
		if (f == NULL) {
			fprintf(stderr, "error: failed to open logfile '%s': %s\n",
					relay_logfile, strerror(errno));
			exit(1);
		}
		relay_stdout = f;
		relay_stderr = f;
	} else if (mode & MODE_CONFIGTEST) {
		/* suppress stdout, just show errors */
		relay_stdout = NULL;
		relay_stderr = stderr;
	} else {
		relay_stdout = stdout;
		relay_stderr = stderr;
	}
	relay_can_log = 1;

	if (mode & MODE_DAEMON) {
		if (relay_logfile == NULL) {
			fprintf(stderr,
					"You must specify logfile if you want daemonisation!\n");
			exit(1);
		}

		if (mode & MODE_TEST) {
			fprintf(stderr,
					"You cannot use test mode if you want daemonisation!\n");
			exit(1);
		}
	}

	if (pidfile != NULL) {
		pidfile_handle = fopen(pidfile, "w");
		if (pidfile_handle == NULL) {
			fprintf(stderr, "failed to open pidfile '%s': %s\n",
					pidfile, strerror(errno));
			exit(1);
		}
	}

	if (mode & MODE_DAEMON) {
		pid_t p;

		if (pipe(fds) < 0) {
			fprintf(stderr, 
					"failed to create pipe to talk to child process: %s\n",
					strerror(errno));
			exit(1);
		}

		p = fork();
		if (p < 0) {
			fprintf(stderr, "failed to fork intermediate process: %s\n",
					strerror(errno));
			exit(1);
		} else if (p == 0) {
			char msg[1024];
			/* child, fork again so our intermediate process can die and
			 * its child becomes an orphan to init */
			p = fork();
			if (p < 0) {
				snprintf(msg, sizeof(msg),
						"failed to fork daemon process: %s\n", strerror(errno));
				/* fool compiler */
				if (write(fds[1], msg, strlen(msg) + 1) > -2)
					exit(1); /* not that retcode matters */
			} else if (p == 0) {
				/* child, this is the final daemon process */
				if (setsid() < 0) {
					snprintf(msg, sizeof(msg), "failed to setsid(): %s\n",
							strerror(errno));
					/* fool compiler */
					if (write(fds[1], msg, strlen(msg) + 1) > -2)
						exit(1); /* not that retcode matters */
				}
				/* close descriptors, we won't need them ever since
				 * we're detached from the controlling terminal */
				close(0);
				close(1);
				close(2);
			} else {
				/* parent, die */
				exit(0);
			}
		} else {
			/* parent: wait for child to report success */
			char msg[1024];
			int len = read(fds[0], msg, sizeof(msg));
			if (len < 0) {
				fprintf(stderr, "unable to read from child, "
						"assuming daemonisation failed\n");
				exit(1);
			} else if (len == 0) {
				fprintf(stderr, "got unexpected EOF from child, "
						"assuming daemonisation failed\n");
				exit(1);
			} else if (strncmp(msg, "OK", len) == 0) {
				/* child is happy, so can we */
				exit(0);
			} else {
				if (len == 1024)
					len--;
				msg[len] = '\0';
				fprintf(stderr, "%s", msg);
				exit(1);
			}
		}
	} else {
		fds[0] = fds[1] = -1;
	}
#define exit_err(...) { \
	if (fds[0] != -1) { \
		char msg[1024]; \
		snprintf(msg, sizeof(msg), __VA_ARGS__); \
		/* fool compiler */ \
		if (write(fds[1], msg, strlen(msg) + 1) > -2) \
			exit(1); /* not that retcode matters */ \
	} else { \
		logerr(__VA_ARGS__); \
		exit(1); \
	} \
}
#define exit_ok { \
	if (fds[0] != -1) { \
		/* we're fine, flag our grandparent (= the original
		 * process being called) it can terminate */ \
		if (write(fds[1], "OK", 3) < -2) \
			exit(1);  /* if our grantparent died, we better do too */ \
		close(fds[0]); \
		close(fds[1]); \
	} \
	fds[0] = fds[1] = -1; \
	startup_success = 1; \
}

	if (pidfile_handle) {
		fprintf(pidfile_handle, "%zu\n", (ssize_t)getpid());
		fclose(pidfile_handle);
	}


	logout("starting carbon-c-relay v%s (%s), pid=%d\n",
			VERSION, GIT_VERSION, getpid());
	if (relay_stdout != NULL) {
		fprintf(relay_stdout, "configuration:\n");
		fprintf(relay_stdout, "    relay hostname = %s\n", relay_hostname);
		fprintf(relay_stdout, "    workers = %d\n", workercnt);
		fprintf(relay_stdout, "    send batch size = %d\n", batchsize);
		fprintf(relay_stdout, "    server queue size = %d\n", queuesize);
		fprintf(relay_stdout, "    server max stalls = %d\n", maxstalls);
		fprintf(relay_stdout, "    listen backlog = %u\n", listenbacklog);
		if (sockbufsize > 0)
			fprintf(relay_stdout, "    socket bufsize = %u\n", sockbufsize);
		fprintf(relay_stdout, "    server connection IO timeout = %dms\n",
				iotimeout);
		if (allowed_chars != NULL)
			fprintf(relay_stdout, "    extra allowed characters = %s\n",
					allowed_chars);
		if (mode & MODE_DEBUG)
			fprintf(relay_stdout, "    debug = true\n");
#ifdef ENABLE_TRACE
		if (mode & MODE_TRACE)
			fprintf(relay_stdout, "    trace = true\n");
#endif
		else if (mode & MODE_SUBMISSION)
			fprintf(relay_stdout, "    submission = true\n");
		else if (mode & MODE_DAEMON)
			fprintf(relay_stdout, "    daemon = true\n");
		fprintf(relay_stdout, "    configuration = %s\n", config);
		fprintf(relay_stdout, "\n");
	}

#ifdef HAVE_SSL
	/* this needs to happen once, globally */
	SSL_library_init();
	SSL_load_error_strings();
	OpenSSL_add_all_algorithms();
#endif

	if ((rtr = router_readconfig(NULL, config, workercnt,
					queuesize, batchsize, maxstalls,
					iotimeout, sockbufsize, listenport)) == NULL)
	{
		exit_err("failed to read configuration '%s'\n", config);
	}
	router_optimise(rtr, optimiserthreshold);

	/* we're done if all we wanted was to test the config */
	if (mode & MODE_CONFIGTEST)
		exit(0);

	aggrs = router_getaggregators(rtr);
	numaggregators = aggregator_numaggregators(aggrs);
	if (numaggregators > 10 && !(mode & MODE_DEBUG)) {
		fprintf(relay_stdout, "parsed configuration follows:\n"
				"(%zu aggregations with %zu computations omitted "
				"for brevity)\n",
				numaggregators, aggregator_numcomputes(aggrs));
		router_printconfig(rtr, relay_stdout, PMODE_NORM);
	} else {
		fprintf(relay_stdout, "parsed configuration follows:\n");
		router_printconfig(rtr, relay_stdout,
				PMODE_AGGR + (mode & MODE_DEBUG ?
					PMODE_DEBUG | PMODE_PEMT : PMODE_NORM));
	}
	fprintf(relay_stdout, "\n");

	/* shortcut for rule testing mode */
	if (mode & MODE_TEST) {
		char metricbuf[METRIC_BUFSIZ];
		char *p;

		fflush(relay_stdout);
		while (fgets(metricbuf, sizeof(metricbuf), stdin) != NULL) {
			if ((p = strchr(metricbuf, '\n')) != NULL)
				*p = '\0';
			p = metricbuf;
			while (isspace(*p))
				p++;
			if (*p == '\0')
				continue;
			router_test(rtr, metricbuf);
		}

		exit(0);
	}

	if (signal(SIGINT, sig_handler) == SIG_ERR) {
		exit_err("failed to create SIGINT handler: %s\n", strerror(errno));
	}
	if (signal(SIGTERM, sig_handler) == SIG_ERR) {
		exit_err("failed to create SIGTERM handler: %s\n", strerror(errno));
	}
	if (signal(SIGQUIT, sig_handler) == SIG_ERR) {
		exit_err("failed to create SIGQUIT handler: %s\n", strerror(errno));
	}
	if (signal(SIGHUP, sig_handler) == SIG_ERR) {
		exit_err("failed to create SIGHUP handler: %s\n", strerror(errno));
	}
	if (signal(SIGPIPE, SIG_IGN) == SIG_ERR) {
		exit_err("failed to ignore SIGPIPE: %s\n", strerror(errno));
	}

	workers = malloc(sizeof(dispatcher *) *
			(1/*lsnr*/ + workercnt + 1/*sentinel*/));
	if (workers == NULL) {
		exit_err("failed to allocate memory for workers\n");
	}

	dispatch_set_bufsize(sockbufsize);
	if (dispatch_init_listeners() != 0) {
		exit_err("failed to allocate listeners\n");
	}

	lsnrs = router_get_listeners(rtr);
	for ( ; lsnrs != NULL; lsnrs = lsnrs->next) {
		if (bindlisten(lsnrs, listenbacklog) != 0) {
			exit_err("failed to setup listener\n");
		}
		if (dispatch_addlistener(lsnrs) != 0) {
			exit_err("failed to add listener\n");
		}
	}
	/* ensure the listener id is at the end for regex_t array hack */
	if ((workers[0] = dispatch_new_listener((unsigned char)workercnt)) == NULL)
		logerr("failed to add listener dispatcher\n");

	if (allowed_chars == NULL)
		allowed_chars = "-_:#";
	logout("starting %d workers\n", workercnt);
	for (id = 0; id < workercnt; id++) {
		workers[id + 1] = dispatch_new_connection(
				(unsigned char)id, rtr, allowed_chars);
		if (workers[id + 1] == NULL) {
			logerr("failed to add worker %d\n", id);
			break;
		}
	}
	workers[id + 1] = NULL;  /* sentinel */
	if (id < workercnt) {
		logerr("shutting down due to errors\n");
		keep_running = 0;
	}

	/* server used for delivering metrics produced inside the relay,
	 * that is, the collector (statistics) */
	if ((internal_submission = server_new(
					"internal", listenport, T_LINEMODE, W_PLAIN, CON_PIPE,
					NULL, NULL, 3000,
					batchsize, maxstalls, iotimeout, sockbufsize)) == NULL)
	{
		logerr("failed to create internal submission queue, shutting down\n");
		keep_running = 0;
	}
	if (internal_submission != NULL && server_start(internal_submission) != 0) {
		logerr("failed to start submission queue thread, shutting down\n");
		server_free(internal_submission);
		internal_submission = NULL;
		keep_running = 0;
	}

	if (numaggregators > 0) {
		logout("starting aggregator\n");
		if (!aggregator_start(aggrs)) {
			logerr("shutting down due to failure to start aggregator\n");
			keep_running = 0;
		}
	}

	logout("starting statistics collector\n");
	if (internal_submission != NULL)
		collector_start(&workers[1], rtr, internal_submission, smode == CUM);

	logout("starting servers\n");
	if (router_start(rtr) != 0) {
		/* router_start logerrs itself */
		keep_running = 0;
	}

	if (keep_running != 0) {
		logout("startup sequence complete\n");
		exit_ok;
	}

	/* workers do the work, just wait */
	while (keep_running) {
		sleep(1);
		if (pending_signal != -1) {
			handle_signal(pending_signal);
			pending_signal = -1;
		}
	}

	logout("shutting down...\n");
	/* make sure we don't accept anything new anymore */
	lsnrs = router_get_listeners(rtr);
	for ( ; lsnrs != NULL; lsnrs = lsnrs->next) {
		dispatch_removelistener(lsnrs);
		shutdownclose(lsnrs);
	}
	/* since workers will be freed, stop querying the structures */
	collector_stop();
	server_shutdown(internal_submission);
	server_free(internal_submission);
	logout("stopped collector\n");
	if (numaggregators > 0) {
		aggregator_stop();
		logout("stopped aggregator\n");
	}
	/* give a little time for whatever the collector/aggregator wrote,
	 * to be delivered by the dispatchers */
	usleep(500 * 1000);  /* 500ms */
	/* make sure we don't write to our servers any more */
	logout("stopped worker");
	for (id = 0; id < 1 + workercnt; id++)
		dispatch_stop(workers[id + 0]);
	for (id = 0; id < 1 + workercnt; id++) {
		dispatch_shutdown(workers[id + 0]);
		dispatch_free(workers[id + 0]);
		fprintf(relay_stdout, " %d", id + 1);
		fflush(relay_stdout);
	}
	fprintf(relay_stdout, "\n");
	fflush(relay_stdout);
	free(workers);

	router_shutdown(rtr);
	router_free(rtr);
	logout("stopped servers\n");

	if (pidfile != NULL)
		unlink(pidfile);

	logout("stopped carbon-c-relay v%s (%s)\n", VERSION, GIT_VERSION);

	/* if we never called exit_ok before, it means we shut down due to
	 * errors, so exit with an error */
	if (startup_success == 0) {
		/* this message will never reach the log/screen when not
		 * daemonised, because the logger already shut down */
		exit_err("startup failed, review the logs for details\n");
	}
	return 0;
}
