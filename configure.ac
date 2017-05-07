#                                               -*- Autoconf -*-
# Process this file with autoconf to produce a configure script.

AC_PREREQ([2.69])
AC_INIT([carbon-c-relay], [3.1], [])
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
				  fcntl.h \
				  inttypes.h \
				  netdb.h \
				  netinet/in.h \
				  stddef.h \
				  stdlib.h \
				  string.h \
				  sys/socket.h \
				  sys/time.h \
				  unistd.h],
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
	  [AC_CHECK_LIB([z], [gzopen],
					[AC_SUBST([LIBGZIP], [-lz])
					AC_DEFINE([HAVE_GZIP], [1], [Define if you have gzip])
					],
					[if test "x$with_gzip" != xcheck; then
					 AC_MSG_FAILURE(
					   [--with-gzip was given, but test for gzip failed])
					 fi
					]
	)])

AC_ARG_WITH([bzip2], [support bzip2 compression for sending/receiving],
			[], [with_bzip2=check])
LIBBZIP2=
AS_IF([test "x$with_bzip2" != xno],
	  [AC_CHECK_LIB([bz2], [BZ2_bzopen],
					[AC_SUBST([LIBBZIP2], [-lbz2])
					AC_DEFINE([HAVE_BZIP2], [1], [Define if you have bzip2])
					],
					[if test "x$with_bzip2" != xcheck; then
					 AC_MSG_FAILURE(
					   [--with-bzip2 was given, but test for bzip2 failed])
					 fi
					]
	)])

AC_ARG_WITH([lzma], [support lzma compression for sending/receiving],
			[], [with_lzma=check])
LIBLZMA=
AS_IF([test "x$with_lzma" != xno],
	  [AC_CHECK_LIB([lzma], [lzma_stream_decoder],
					[AC_SUBST([LIBLZMA], [-llzma])
					AC_DEFINE([HAVE_LZMA], [1], [Define if you have lzma])
					],
					[if test "x$with_lzma" != xcheck; then
					 AC_MSG_FAILURE(
					   [--with-lzma was given, but test for lzma failed])
					 fi
					]
	)])

AC_ARG_WITH([ssl], [support ssl encryption for sending/receiving],
			[], [with_ssl=check])
LIBSSL=
AS_IF([test "x$with_ssl" != xno],
	  [AC_CHECK_LIB([ssl], [SSL_connect],
					[AC_SUBST([LIBSSL], [-lssl])
					AC_DEFINE([HAVE_SSL], [1], [Define if you have ssl])
					],
					[if test "x$with_ssl" != xcheck; then
					 AC_MSG_FAILURE(
					   [--with-ss was given, but test for ssl failed])
					 fi
					]
	)])

AC_CONFIG_FILES([Makefile])
AC_OUTPUT
