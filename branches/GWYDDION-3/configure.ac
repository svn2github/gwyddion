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
  m4/Makefile
  build/Makefile
  libgwy/Makefile
  libgwy/version.h
  po/Makefile.in
  po-libgwy/Makefile.in
  docs/Makefile
  docs/libgwy/Makefile
  data/Makefile
  data/libgwy3.pc
])

AM_INIT_AUTOMAKE([1.11 gnu -Wall check-news dist-xz no-dist-gzip std-options nostdinc silent-rules])
m4_ifdef([AM_SILENT_RULES],[AM_SILENT_RULES([yes])])

AC_SUBST([GWY_VERSION_MAJOR],[gwy_version_major])
AC_SUBST([GWY_VERSION_MINOR],[gwy_version_minor])
AC_SUBST([GWY_VERSION_MICRO],[gwy_version_micro])
AC_SUBST([GWY_VERSION_STRING],[gwy_version_string])
AC_SUBST([PACKAGE],[gwyddion3])
AC_DEFINE([PACKAGE],["gwyddion3"],[Name of package])
AM_CONDITIONAL([UNSTABLE_LIBRARY_RELEASE],[true])

AC_CANONICAL_HOST

GWY_MISSING_TOOLS=

AC_DISABLE_STATIC
AM_PROG_CC_C_O
AC_PROG_CC_C99
AC_PROG_CXX
AC_USE_SYSTEM_EXTENSIONS
AC_LIBTOOL_WIN32_DLL
AC_PROG_LIBTOOL
AC_PROG_INSTALL

gwy_lib_path=`echo "$sys_lib_search_path_spec" | sed -e 's/  */:/g'`
AC_DEFINE_UNQUOTED([GWY_LIB_PATH],["$gwy_lib_path"],
                   [Define to system library search path])

PKG_PROG_PKG_CONFIG([0.22])

#############################################################################
# Compiler

if test "x$ac_cv_prog_cc_c99" = xno; then
  AC_MSG_ERROR([C99 standard conforming C compiler is required])
fi

GWY_CHECK_GNUC_VISIBILITY

#############################################################################
# Headers
AC_CHECK_HEADERS([unistd.h])

#############################################################################
# I18n
GETTEXT_PACKAGE=gwyddion3
AM_GNU_GETTEXT([external])
AM_GNU_GETTEXT_VERSION([0.17])
AC_SUBST(GETTEXT_PACKAGE)

IT_PROG_INTLTOOL([0.40],[no-xml])

#############################################################################
# Gtk-doc
GTK_DOC_CHECK([1.11])

#############################################################################
# Python
AM_PATH_PYTHON([2.2],,[GWY_MISSING_TOOLS="$GWY_MISSING_TOOLS python"])
AM_CONDITIONAL([HAVE_PYTHON],[test "$PYTHON" != :])

#############################################################################
# Math Functions
GWY_CHECK_LIBM

m4_define([gwy_math_funcs],[exp10])
GWY_CHECK_MATH_FUNCS([gwy_math_funcs])

#############################################################################
# GLIBC >= 2.1
gl_GLIBC21
if test "x$GLIBC21" = xyes; then
  AC_DEFINE([HAVE_MEMMEM],1,[Define if we have working memmem()])
fi

#############################################################################
# Base libraries
PKG_CHECK_MODULES([GIO],[gio-2.0 >= 2.22])

AC_SUBST([LIBGWY_DEPS_CFLAGS],["$GIO_CFLAGS"])
AC_SUBST([LIBGWY_DEPS_LIBS],["$GIO_LIBS"])

#############################################################################
# GLib tools
AC_MSG_CHECKING([for glib-genmarshal])
GLIB_GENMARSHAL=`$PKG_CONFIG --variable=glib_genmarshal glib-2.0`
AC_MSG_RESULT([$GLIB_GENMARSHAL])
AC_SUBST(GLIB_GENMARSHAL)

AC_MSG_CHECKING([for glib-mkenums])
GLIB_MKENUMS=`$PKG_CONFIG --variable=glib_mkenums glib-2.0`
AC_MSG_RESULT([$GLIB_MKENUMS])
AC_SUBST(GLIB_MKENUMS)

#AC_MSG_CHECKING([for gobject-query])
#GOBJECT_QUERY=`$PKG_CONFIG --variable=gobject_query glib-2.0`
#AC_MSG_RESULT([$GOBJECT_QUERY])
#AC_SUBST(GOBJECT_QUERY)

#############################################################################
# Valgrind
PKG_CHECK_EXISTS([valgrind],
                 [gwy_enable_valgrind=yes
                  PKG_CHECK_MODULES([VALGRIND],[valgrind])
                  AC_DEFINE([HAVE_VALGRIND],1,[Define if we have valgrind])],
                 [gwy_enable_valgrind=no])
AM_CONDITIONAL([HAVE_VALGRIND],[test "x$gwy_enable_valgrind" = xyes])

#############################################################################
# Write gwyconfig.h
AC_CONFIG_COMMANDS([gwyconfig.h],
[
  outfile=gwyconfig.h-tmp
  g=GENERATED
  cat > $outfile <<_______EOF
/*
 * gwyconfig.h
 * This is a $g file.  Please modify 'configure.ac'.
 */

#ifndef __GWY_CONFIG_H__
#define __GWY_CONFIG_H__
_______EOF

  GWY_OUTPUT_MATH_FUNCS([gwy_math_funcs], [$outfile])

  cat >>$outfile <<_______EOF

#endif
_______EOF

  if cmp -s $outfile gwyconfig.h; then
    AC_MSG_NOTICE([gwyconfig.h is unchanged])
    rm -f $outfile
  else
    mv $outfile gwyconfig.h
  fi
],
[
  gwy_math_exp10=$gwy_math_exp10
])

#############################################################################
# Translation Makefiles
AC_CONFIG_COMMANDS([sed-po-makefiles],
[
  for x in libgwy; do
    sed -e "/POTFILES =/r po-$x/POTFILES" po-$x/Makefile.in > po-$x/Makefile \
      && touch po-$x/stamp-it
  done
])

#############################################################################
# Output
AC_OUTPUT

