dnl Process this file with autoconf to produce a configure script.
m4_define([gwy_version_major],[2])
m4_define([gwy_version_minor],[99])
m4_define([gwy_version_micro],[0])
m4_define([gwy_version_string],
          [gwy_version_major.gwy_version_minor.gwy_version_micro])
m4_define([gwy_url],[http://gwyddion.net/])

AC_INIT([Gwyddion], [gwy_version_string], [yeti@gwyddion.net])
AC_REVISION([$Id$])

AC_PREREQ(2.63)
AC_CONFIG_SRCDIR([libgwy/libgwy.h])
AC_CONFIG_AUX_DIR([build])
AC_CONFIG_MACRO_DIR([m4])
AC_CONFIG_HEADERS([config.h:build/config.h.in])
AC_CONFIG_FILES([
  Makefile
  po/Makefile.in
])

AM_INIT_AUTOMAKE([1.10 gnu -Wall check-news dist-lzma no-dist-gzip std-options])

AC_SUBST([GWY_VERSION_MAJOR],[gwy_version_major])
AC_SUBST([GWY_VERSION_MINOR],[gwy_version_minor])
AC_SUBST([GWY_VERSION_MICRO],[gwy_version_micro])
AC_SUBST([GWY_VERSION_STRING],[gwy_version_string])

AC_CANONICAL_HOST

GWY_MISSING_TOOLS=
AC_PROG_LIBTOOL
AC_PROG_CC
AC_PROG_CXX

AM_GNU_GETTEXT([external])
AM_GNU_GETTEXT_VERSION([0.17])
AC_DEFINE_UNQUOTED(GETTEXT_PACKAGE,"$PACKAGE_TARNAME",[Gettext package name])
AC_SUBST([GETTEXT_PACKAGE],[$PACKAGE_TARNAME])

IT_PROG_INTLTOOL([0.40],[no-xml])

GTK_DOC_CHECK([1.10])

AM_PATH_PYTHON([2.2],,[GWY_MISSING_TOOLS="$GWY_MISSING_TOOLS python"])
AM_CONDITIONAL([HAVE_PYTHON],[test "$PYTHON" != :])

AC_OUTPUT
