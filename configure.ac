# Process this file with autoconf to produce a configure script.
AC_INIT(morse, 1.0)
AM_INIT_AUTOMAKE
AC_CONFIG_SRCDIR([morse.cpp])
AM_CONFIG_HEADER([config.h])

AC_LANG(C++)

# Checks for programs.
AC_PROG_CXX
AC_PROG_INSTALL
AC_PROG_AWK

# Checks for libraries.
AC_CHECK_LIB([m], [sin])

# Checks for header files.
AC_CHECK_HEADER([sys/soundcard.h])

# Checks for typedefs, structures, and compiler characteristics.

# Checks for library functions.
AC_PROG_GCC_TRADITIONAL

AC_CONFIG_FILES([Makefile])
AC_OUTPUT
