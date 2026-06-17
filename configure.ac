#                                               -*- Autoconf -*-
# Process this file with autoconf to produce a configure script.

AC_PREREQ([2.69])
AC_INIT([solidbits], 1.0)
AC_CONFIG_SRCDIR([main.c])
AC_CONFIG_HEADERS([config.h])
AC_CONFIG_FILES([Makefile])
AM_INIT_AUTOMAKE
: ${CFLAGS="-O3 -g -Wall"}
# Checks for programs.
AC_PROG_CC

# Checks for libraries.
AC_PROG_RANLIB
AC_CHECK_LIB([pthread], [pthread_create])
# libuv (TCP backend). --with-libuv=PATH extends the pkg-config search path
# so custom (non-system) installs can be found.
AC_ARG_WITH([libuv],
  [AS_HELP_STRING([--with-libuv=PATH],
    [prefix where libuv is installed; looks in PATH/lib/pkgconfig and PATH/lib64/pkgconfig])],
  [if test "$withval" != "no" -a "$withval" != "yes"; then
     for d in "$withval/lib/pkgconfig" "$withval/lib64/pkgconfig"; do
       test -d "$d" && PKG_CONFIG_PATH="$d${PKG_CONFIG_PATH:+:$PKG_CONFIG_PATH}"
     done
     export PKG_CONFIG_PATH
   fi])

PKG_CHECK_MODULES([LIBUV], [libuv >= 1.0],
  [],
  [AC_MSG_ERROR([libuv >= 1.0 not found.
    Install it: apt install libuv1-dev (Debian/Ubuntu) or libuv-devel (RHEL/Fedora),
    or point to a custom prefix: ./configure --with-libuv=/path/to/prefix])])



AC_ARG_WITH(jemalloc, AS_HELP_STRING([--with-jemalloc=PATH],
	[specify directory containing the jemalloc library]))

if test "$with_jemalloc" -a -d "$with_jemalloc"; then
	external_malloc_ldflags="-Wl,-rpath,$with_jemalloc -L$with_jemalloc"
    enable_jemalloc=1
fi

if test "$enable_jemalloc"; then
    AC_CHECK_LIB([jemalloc], [malloc])
fi

AC_SUBST(LDFLAGS, "$external_malloc_ldflags $LDFLAGS")
AC_SUBST(CFLAGS, "$CFLAGS")

# Checks for header files.
AC_CHECK_HEADERS([arpa/inet.h fcntl.h inttypes.h limits.h netinet/in.h stddef.h stdint.h stdlib.h string.h strings.h sys/socket.h sys/time.h syslog.h unistd.h])

# Checks for typedefs, structures, and compiler characteristics.
AC_CHECK_HEADER_STDBOOL
AC_TYPE_INT64_T
AC_TYPE_MODE_T
AC_TYPE_OFF_T
AC_TYPE_PID_T
AC_TYPE_SIZE_T
AC_TYPE_SSIZE_T
AC_TYPE_UINT16_T
AC_TYPE_UINT32_T
AC_TYPE_UINT64_T
AC_TYPE_UINT8_T

# Checks for library functions.
AC_FUNC_FORK
AC_FUNC_MALLOC
AC_FUNC_REALLOC
AC_CHECK_FUNCS([bzero dup2 gettimeofday inet_ntoa memset socket strcasecmp strerror])

AC_OUTPUT
