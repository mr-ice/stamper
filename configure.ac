# Process this file with autoconf to produce a configure script.
# Copyright (C) 2025 Michael Rice <michael@ricelan.org>
# This file is part of TS. See the COPYING file for license terms.

AC_PREREQ([2.69])
AC_INIT([ts], [1.0.0], [your-email@example.com])
AC_CONFIG_SRCDIR([ts.c])
AC_CONFIG_HEADERS([config.h])
AC_CONFIG_AUX_DIR([build-aux])

# Automake initialization
AM_INIT_AUTOMAKE([foreign subdir-objects info-in-builddir])

# Check for C compiler
AC_PROG_CC

# Check for required compiler flags
AC_CANONICAL_HOST

# Set default CFLAGS if not set
if test "x$CFLAGS" = "x"; then
    CFLAGS="-Wall -Wextra -O2"
fi

# Check for specific compiler capabilities
AC_MSG_CHECKING([for C11 support])
AC_COMPILE_IFELSE([
    AC_LANG_PROGRAM([
        #include <stdbool.h>
        #include <stdarg.h>
        _Static_assert(sizeof(int) >= 4, "int must be at least 32 bits");
    ], [
        return 0;
    ])
], [
    AC_MSG_RESULT([yes])
    AC_DEFINE([HAVE_C11], [1], [Define if C11 is supported])
], [
    AC_MSG_RESULT([no])
    AC_MSG_ERROR([C11 support is required])
])

# Check for required headers
AC_CHECK_HEADERS([stdio.h stdlib.h string.h time.h unistd.h getopt.h sys/time.h regex.h errno.h assert.h stdarg.h stdbool.h])

# Check for required functions
AC_CHECK_FUNCS([clock_gettime strptime strnlen vsnprintf regcomp regexec regfree mktime localtime gmtime time])

# Check for specific time-related capabilities
AC_MSG_CHECKING([for CLOCK_REALTIME support])
AC_COMPILE_IFELSE([
    AC_LANG_PROGRAM([
        #include <time.h>
        #include <sys/time.h>
    ], [
        struct timespec ts;
        clock_gettime(CLOCK_REALTIME, &ts);
        return 0;
    ])
], [
    AC_MSG_RESULT([yes])
    AC_DEFINE([HAVE_CLOCK_REALTIME], [1], [Define if CLOCK_REALTIME is supported])
], [
    AC_MSG_RESULT([no])
    AC_MSG_ERROR([CLOCK_REALTIME support is required])
])

AC_MSG_CHECKING([for CLOCK_MONOTONIC support])
AC_COMPILE_IFELSE([
    AC_LANG_PROGRAM([
        #include <time.h>
        #include <sys/time.h>
    ], [
        struct timespec ts;
        clock_gettime(CLOCK_MONOTONIC, &ts);
        return 0;
    ])
], [
    AC_MSG_RESULT([yes])
    AC_DEFINE([HAVE_CLOCK_MONOTONIC], [1], [Define if CLOCK_MONOTONIC is supported])
], [
    AC_MSG_RESULT([no])
    AC_DEFINE([HAVE_CLOCK_MONOTONIC], [0], [Define if CLOCK_MONOTONIC is supported])
])

# Check for regex support
AC_MSG_CHECKING([for POSIX regex support])
AC_COMPILE_IFELSE([
    AC_LANG_PROGRAM([
        #include <regex.h>
    ], [
        regex_t regex;
        int result = regcomp(&regex, "test", REG_EXTENDED);
        if (result == 0) {
            regfree(&regex);
        }
        return 0;
    ])
], [
    AC_MSG_RESULT([yes])
    AC_DEFINE([HAVE_POSIX_REGEX], [1], [Define if POSIX regex is supported])
], [
    AC_MSG_RESULT([no])
    AC_MSG_ERROR([POSIX regex support is required])
])

# Check for strptime support
AC_MSG_CHECKING([for strptime support])
AC_COMPILE_IFELSE([
    AC_LANG_PROGRAM([
        #include <time.h>
    ], [
        struct tm tm;
        char *result = strptime("2023-01-01", "%Y-%m-%d", &tm);
        return result ? 0 : 1;
    ])
], [
    AC_MSG_RESULT([yes])
    AC_DEFINE([HAVE_STRPTIME], [1], [Define if strptime is supported])
], [
    AC_MSG_RESULT([no])
    AC_MSG_ERROR([strptime support is required])
])

# Check for 64-bit time_t
AC_MSG_CHECKING([for 64-bit time_t])
AC_COMPILE_IFELSE([
    AC_LANG_PROGRAM([
        #include <time.h>
        _Static_assert(sizeof(time_t) >= 8, "time_t must be at least 64 bits");
    ], [
        return 0;
    ])
], [
    AC_MSG_RESULT([yes])
    AC_DEFINE([HAVE_64BIT_TIME_T], [1], [Define if time_t is at least 64 bits])
], [
    AC_MSG_RESULT([no])
    AC_MSG_WARN([time_t is not 64 bits, this may cause issues with timestamps after 2038])
])

# Check for 64-bit long
AC_MSG_CHECKING([for 64-bit long])
AC_COMPILE_IFELSE([
    AC_LANG_PROGRAM([
        _Static_assert(sizeof(long) >= 8, "long must be at least 64 bits");
    ], [
        return 0;
    ])
], [
    AC_MSG_RESULT([yes])
    AC_DEFINE([HAVE_64BIT_LONG], [1], [Define if long is at least 64 bits])
], [
    AC_MSG_RESULT([no])
    AC_MSG_WARN([long is not 64 bits, this may cause issues with large timestamps])
])

# Set installation paths
AC_PREFIX_DEFAULT([/usr/local])

# Check for documentation tools
AC_PATH_PROG([MAKEINFO], [makeinfo], [no])
AC_PATH_PROG([INSTALL_INFO], [install-info], [no])

# Enable/disable documentation
AC_ARG_ENABLE([docs],
    [AS_HELP_STRING([--enable-docs], [build documentation (default: auto)])],
    [enable_docs=$enableval],
    [enable_docs=auto])

AS_IF([test "x$enable_docs" != xno], [
    AS_IF([test "x$MAKEINFO" != xno], [
        AC_DEFINE([HAVE_MAKEINFO], [1], [Define if makeinfo is available])
        enable_docs=yes
    ], [
        AS_IF([test "x$enable_docs" = xyes], [
            AC_MSG_ERROR([makeinfo not found but --enable-docs was specified])
        ], [
            enable_docs=no
        ])
    ])
])

AM_CONDITIONAL([BUILD_DOCS], [test "x$enable_docs" = xyes])

# Generate output files
AC_CONFIG_FILES([
    Makefile
])

# Output summary
AC_MSG_NOTICE([Configuration summary:])
AC_MSG_NOTICE([  Compiler: $CC])
AC_MSG_NOTICE([  CFLAGS: $CFLAGS])
AC_MSG_NOTICE([  Installation prefix: $prefix])

AC_OUTPUT 