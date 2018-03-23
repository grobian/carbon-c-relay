#                                               -*- Autoconf -*-
# Process this file with autoconf to produce a configure script.

AC_PREREQ([2.69])
AC_INIT([carbon-c-relay], [3.3], [])
AC_CONFIG_HEADERS([config.h])
AM_INIT_AUTOMAKE

# Checks for programs.
AC_PROG_CC
AC_PROG_MAKE_SET

# Checks for libraries.
AC_SEARCH_LIBS([sqrt], [m])
AC_SEARCH_LIBS([gethostbyname], [nsl])
AC_SEARCH_LIBS([socket], [socket])
saveLIBS="$LIBS"
LIBS=
AC_SEARCH_LIBS([pthread_create], [pthread])
if test "x$LIBS" != x ; then
	# compiler knows best, really
	LIBS="-pthread"
fi
LIBS="$saveLIBS $LIBS"

# Checks for header files.
AC_CHECK_HEADERS([arpa/inet.h \
				  assert.h \
				  errno.h \
				  fcntl.h \
				  glob.h \
				  math.h \
				  netdb.h \
				  netinet/in.h \
				  netinet/tcp.h \
				  poll.h \
				  pthread.h \
				  regex.h \
				  signal.h \
				  stdarg.h \
				  stdio.h \
				  stdlib.h \
				  string.h \
				  sys/resource.h \
				  sys/socket.h \
				  sys/stat.h \
				  sys/time.h \
				  sys/types.h \
				  sys/uio.h \
				  sys/un.h \
				  time.h \
				  unistd.h \
				  ],
				  [],
				  [AC_MSG_ERROR([required header missing])])

# Checks for typedefs, structures, and compiler characteristics.
AC_C_INLINE

# Checks for library functions.
AC_FUNC_FORK
AC_FUNC_MALLOC
AC_FUNC_REALLOC
AC_CHECK_FUNCS([gethostname \
				gettimeofday \
				localtime_r \
				memmove \
				memset \
				pow \
				regcomp \
				socket \
				sqrt \
				strchr \
				strdup \
				strerror \
				strstr \
				strtol],
				[],
				[AC_MSG_ERROR([required function missing])])

AC_ARG_WITH([gzip], [support gzip compression for sending/receiving],
			[], [with_gzip=check])
LIBGZIP=
AS_IF([test "x$with_gzip" != xno],
	  [AC_CHECK_HEADERS([zlib.h], [], [LIBGZIP=_missing_header])
	   AC_CHECK_LIB([z${LIBGZIP}], [gzopen],
					[AC_SUBST([LIBGZIP], [-lz])
					AC_DEFINE([HAVE_GZIP], [1], [Define if you have gzip])
					],
					[if test "x$with_gzip" != xcheck; then
					 AC_MSG_FAILURE(
					   [--with-gzip was given, but test for gzip failed])
					 fi
					 LIBGZIP=
					]
	)])

AC_ARG_WITH([lz4], [support lz4 compression for sending/receiving],
			[], [with_lz4=check])
LIBLZ4=
AS_IF([test "x$with_lz4" != xno],
	  [AC_CHECK_HEADERS([lz4.h], [], [LIBLZ4=_missing_header])
	   AC_CHECK_LIB([lz4${LIBLZ4}], [LZ4_createStream],
					[AC_SUBST([LIBLZ4], [-llz4])
					AC_DEFINE([HAVE_LZ4], [1], [Define if you have lz4])
					],
					[if test "x$with_lz4" != xcheck; then
					 AC_MSG_FAILURE(
					   [--with-lz4 was given, but test for lz4 failed])
					 fi
					 LIBLZ4=
					]
	)])

AC_ARG_WITH([ssl], [support ssl encryption for sending/receiving],
			[], [with_ssl=check])
LIBSSL=
AS_IF([test "x$with_ssl" != xno],
	  [AC_CHECK_HEADERS([openssl/err.h \
	                     openssl/ssl.h], [], [LIBSSL=_missing_header])
	   AC_CHECK_LIB([ssl${LIBSSL}], [SSL_connect],
					[LIBSSL="-lssl"
					AC_DEFINE([HAVE_SSL], [1], [Define if you have ssl])
					AC_CHECK_LIB([crypto],
								 [OPENSSL_add_all_algorithms_noconf],
								 [LIBSSL="${LIBSSL} -lcrypto"],
								 [])
					AC_SUBST([LIBSSL], ["${LIBSSL}"])
					],
					[if test "x$with_ssl" != xcheck; then
					 AC_MSG_FAILURE(
					   [--with-ssl was given, but test for ssl failed])
					 fi
					 LIBSSL=
					]
	)])

AC_ARG_WITH([oniguruma], [use oniguruma regex library],
			[], [with_oniguruma=check])
LIBONIGURUMA=
AS_IF([test "x$with_oniguruma" != xno],
	  [AC_CHECK_HEADERS([onigposix.h], [], [LIBONIGURUMA=_missing_header])
	   AC_CHECK_LIB([onig${LIBONIGURUMA}], [regexec],
					[LIBONIGURUMA="-lonig"
					AC_DEFINE([HAVE_ONIGURAMA], [1],
							  [Define if you have oniguruma])
					AC_SUBST([LIBONIGURUMA], ["${LIBONIGURUMA}"])
					],
					[if test "x$with_oniguruma" != xcheck; then
					 AC_MSG_FAILURE(
					   [--with-oniguruma was given, but test for oniguruma failed])
					 fi
					 LIBONIGURUMA=
					]
	)])

AC_ARG_WITH([pcre2], [use pcre2 regex library],
			[], [with_pcre2=check])
LIBPCRE2=
AS_IF([test "x${LIBONIGURUMA}" = "x" && test "x$with_pcre2" != xno],
	  [AC_CHECK_HEADERS([pcre2posix.h], [], [LIBPCRE2=_missing_header])
	   AC_CHECK_LIB([pcre2-posix${LIBPRCRE2}], [regexec],
					[LIBPCRE2="-lpcre2-posix"
					AC_DEFINE([HAVE_PCRE2], [1], [Define if you have pcre2])
					AC_SUBST([LIBPCRE2], ["${LIBPCRE2}"])
					],
					[if test "x$with_pcre2" != xcheck; then
					 AC_MSG_FAILURE(
					   [--with-pcre2 was given, but test for pcre2 failed])
					 fi
					 LIBPCRE2=
					]
	)])

AC_ARG_WITH([pcre], [use pcre regex library],
			[], [with_pcre=check])
LIBPCRE=
AS_IF([test "x${LIBONIGURUMA}${LIBPCRE2}" = "x" && test "x$with_pcre" != xno],
	  [AC_CHECK_HEADERS([pcreposix.h], [], [LIBPCRE=_missing_header])
	   AC_CHECK_LIB([pcreposix${LIBPRCRE}], [regexec],
					[LIBPCRE="-lpcreposix"
					AC_DEFINE([HAVE_PCRE], [1], [Define if you have pcre])
					AC_SUBST([LIBPCRE], ["${LIBPCRE}"])
					],
					[if test "x$with_pcre" != xcheck; then
					 AC_MSG_FAILURE(
					   [--with-pcre was given, but test for pcre failed])
					 fi
					 LIBPCRE=
					]
	)])

AC_CONFIG_FILES([Makefile])
AC_OUTPUT
