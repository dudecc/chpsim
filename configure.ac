#                                               -*- Autoconf -*-
# Process this file with autoconf to produce a configure script.

AC_PREREQ(2.61)
AC_INIT([chpsim], [2.0], [cc@async.caltech.edu])
AM_INIT_AUTOMAKE([-Wall -Werror foreign])
AC_CONFIG_SRCDIR([exec.h])
AC_CONFIG_HEADER([config.h])

# Checks for programs.
AC_PROG_CC
AC_PROG_RANLIB

# Checks for libraries.
AC_CHECK_HEADER(gmp.h)
#AC_CHECK_LIB([gmp], [mpz_add])
if test "$ac_cv_header_gmp_h" == no
then
  AC_MSG_ERROR([Chpsim requires the GMP library.])
fi

AC_CHECK_HEADER(readline/readline.h)
AC_CHECK_HEADER(readline/history.h)
if test "$ac_cv_header_readline_h" == no || "$ac_cv_header_history_h" == no
then
  AC_MSG_ERROR([Chpsim requires the Readline library.])
fi

# Checks for header files.
AC_FUNC_ALLOCA
AC_HEADER_DIRENT
AC_HEADER_STDC
AC_CHECK_HEADERS([limits.h stddef.h stdlib.h string.h])

# Checks for typedefs, structures, and compiler characteristics.
AC_C_CONST
AC_TYPE_SIZE_T
AC_C_VOLATILE

# Checks for library functions.
AC_FUNC_ERROR_AT_LINE
AC_FUNC_MALLOC
AC_FUNC_MEMCMP
AC_TYPE_SIGNAL
AC_FUNC_STAT
AC_FUNC_STRTOD
AC_FUNC_VPRINTF
AC_CHECK_FUNCS([strdup strerror strtoul])

AC_CONFIG_FILES([Makefile
                 clib/Makefile])
AC_OUTPUT
