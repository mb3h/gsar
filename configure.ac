#                                               -*- Autoconf -*-
# Process this file with autoconf to produce a configure script.

AC_PREREQ([2.69])
AC_INIT([gsar], [0.00])
AM_INIT_AUTOMAKE([subdir-objects foreign])
AC_CONFIG_SRCDIR([src/main.c])
AC_CONFIG_HEADERS([config.h])

# Checks for programs.
AC_PROG_CC
AC_PROG_INSTALL

# Checks for libraries.
AC_ARG_WITH([gtk],
  [AS_HELP_STRING([--with-gtk=VER],
                  [specify GTK version to use (`1', `2' or `3')])
AS_HELP_STRING([--without-gtk],
                  [do not use GTK (build command-line tools only)])],
  [gtk_version_desired="$withval"],
  [gtk_version_desired="any"])

ifdef([AM_PATH_GTK_2_0],[AM_PATH_GTK_2_0([2.0.0], [gtk=2], [])],
[AC_WARNING([generating configure script without GTK 2 autodetection])])

# Checks for header files.
AC_CHECK_HEADERS([stdint.h stdlib.h string.h])

# Checks for typedefs, structures, and compiler characteristics.
AC_CHECK_HEADER_STDBOOL
AC_TYPE_UINT16_T
AC_TYPE_UINT32_T
AC_TYPE_UINT8_T

# Checks for library functions.
AC_FUNC_ALLOCA
AC_FUNC_MALLOC
AC_CHECK_FUNCS([memmove memset])

AC_CONFIG_FILES([Makefile
                 src/Makefile])
AC_OUTPUT
