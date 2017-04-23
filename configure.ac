#                                               -*- Autoconf -*-
# Process this file with autoconf to produce a configure script.

AC_PREREQ([2.69])
AC_INIT([carbon-c-relay], [3.0], [])
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

AC_CONFIG_FILES([Makefile])
AC_OUTPUT
