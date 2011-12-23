dnl Process this file with autoconf to produce a configure script.
m4_define([gwy_version_major],[2])
m4_define([gwy_version_minor],[99])
m4_define([gwy_version_micro],[0])
m4_define([gwy_version_extra],[])
m4_define([gwy_version_string],
          [gwy_version_major.gwy_version_minor.gwy_version_micro[]gwy_version_extra])
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
  libgwy/libgwy3.pc
  libgwy/tests/Makefile
  libgwy/benchmarks/Makefile
  libgwyui/Makefile
  libgwyui/libgwyui3.pc
  po/Makefile.in
  po-libgwy/Makefile.in
  po-libgwyui/Makefile.in
  docs/Makefile
  docs/libgwy/Makefile
  docs/libgwyui/Makefile
  data/Makefile
  gwyddion3.spec:data/gwyddion3.spec.in \
])

AM_INIT_AUTOMAKE([1.11 gnu -Wall -Wno-portability dist-xz no-dist-gzip std-options nostdinc silent-rules])
m4_ifdef([AM_SILENT_RULES],[AM_SILENT_RULES([yes])])

AC_SUBST([GWY_VERSION_MAJOR],[gwy_version_major])
AC_SUBST([GWY_VERSION_MINOR],[gwy_version_minor])
AC_SUBST([GWY_VERSION_MICRO],[gwy_version_micro])
AC_SUBST([GWY_VERSION_STRING],[gwy_version_string])
AC_SUBST([PACKAGE_SUFFIX],[3])
AM_CONDITIONAL([UNSTABLE_LIBRARY_RELEASE],[true])

AC_CANONICAL_HOST

AC_DISABLE_STATIC
AC_PROG_CC_C99
AC_PROG_CXX
AC_USE_SYSTEM_EXTENSIONS
AC_LIBTOOL_WIN32_DLL
AC_PROG_LIBTOOL
AC_PROG_INSTALL
GWY_CHECK_EU_NM

gwy_lib_path=`echo "$sys_lib_search_path_spec" | sed -e 's/  */:/g'`
AC_DEFINE_UNQUOTED([GWY_LIB_PATH],["$gwy_lib_path"],
                   [Define to system library search path])

PKG_PROG_PKG_CONFIG([0.22])

#############################################################################
# Win32.
AC_MSG_CHECKING([for native Win32])
case "$host_os" in
  mingw*)
    os_win32=yes
    HOST_CFLAGS="-mms-bitfields"
    HOST_LDFLAGS="-mwindows"
    ;;
  *)
    os_win32=no
    ;;
esac
AC_MSG_RESULT([$os_win32])
AM_CONDITIONAL([OS_WIN32],[test "$os_win32" = "yes"])
AC_SUBST(HOST_CFLAGS)
AC_SUBST(HOST_LDFLAGS)

#############################################################################
# Compiler

if test "x$ac_cv_prog_cc_c99" = xno; then
  AC_MSG_ERROR([C compiler conforming to the C99 standard is required])
fi

GWY_PROG_CC_OPTIONS([MATH],[-fexcess-precision=fast -fno-math-errno -fstrict-overflow])
#GWY_PROG_CC_OPTIONS([OPTIMIZE],[-flto -O3])
GWY_PROG_CC_OPTIONS([WARNINGS],[-Wall -Wextra -Wstrict-prototypes -Wpointer-arith -Wstrict-aliasing -Wundef -Wshadow -Wredundant-decls -Wredundant-decls -Wlogical-op -Werror=implicit-function-declaration -Werror=implicit-int])

GWY_CFLAGS="$GWY_PROG_CC_MATH $GWY_PROG_CC_WARNINGS $GWY_PROG_CC_OPTIMIZE"
AC_SUBST([GWY_CFLAGS])

# We may want to make this controlled by a configure option
GWY_PROG_CCLD_OPTION([SYMBOLIC_FUNCTIONS],[-Wl,-Bsymbolic-functions])
#GWY_PROG_CCLD_OPTION([OPTIMIZE],[-flto -O3])

GWY_LIBRARY_LDFLAGS="$GWY_PROG_CCLD_SYMBOLIC_FUNCTIONS $GWY_PROG_CCLD_OPTIMIZE"
AC_SUBST([GWY_LIBRARY_LDFLAGS])

AC_CHECK_SIZEOF([double])
AC_CHECK_SIZEOF([void*])
AC_CHECK_SIZEOF([int64_t],,[#include <stdint.h>])
if test $ac_cv_sizeof_double != $ac_cv_sizeof_int64_t; then
  AC_MSG_WARN([Sizes of double and int64 differ.  Serialisation of doubles will be broken.])
fi
if test $ac_cv_sizeof_voidp -lt $ac_cv_sizeof_double; then
  AC_MSG_WARN([Double does not fit into a pointer.  Median filter will be broken.])
fi

#############################################################################
# Headers
## All required headers appear to be standard (on Win32, at least with MinGW).

#############################################################################
# I18n
GETTEXT_PACKAGE=gwyddion3
AM_GNU_GETTEXT([external])
AM_GNU_GETTEXT_VERSION([0.17])
AC_SUBST(GETTEXT_PACKAGE)

IT_PROG_INTLTOOL([0.41],[no-xml])

#############################################################################
# Gtk-doc
GTK_DOC_CHECK([1.17])

#############################################################################
# Python
AM_PATH_PYTHON([2.2])

#############################################################################
# Math Functions
GWY_CHECK_LIBM
AC_CHECK_FUNCS([sincos])

#############################################################################
# According to the manual page, GLIBC memmem() was broken until version 2.1.
# If we detect the GNU C library check its version, otherwise assume it's OK.
AC_CHECK_FUNCS([memmem])
GWY_GLIBC
memmem_ok=yes
if test "x$GLIBC" = xyes; then
  gl_GLIBC21
  if test "x$GLIBC21" != xyes; then
     memmem_ok=no
  fi
fi
if test "x$memmem_ok" = xyes; then
  AC_DEFINE([HAVE_WORKING_MEMMEM],1,[Define if we have working memmem()])
fi

#############################################################################
# Base libraries
# All libraries that are required should be checked here.
# TODO: use macros from m4/gwy-pkg.m4 once we can make the errors meaningful.
PKG_CHECK_MODULES([GIO],[gio-2.0 >= 2.26])
PKG_CHECK_MODULES([FFTW],[fftw3 >= 3.3])
PKG_CHECK_MODULES([GDKPIXBUF],[gdk-pixbuf-2.0 >= 2.22])
PKG_CHECK_MODULES([GTK],[gtk+-3.0 >= 3.0])

# Needs to be configured out in stable versions.  Deprecations come and go.
AC_SUBST([LIBGWY_COMPAT_CPPFLAGS],["-DG_DISABLE_DEPRECATED -DG_DISABLE_SINGLE_INCLUDES -DGSEAL_ENABLE"])
AC_SUBST([LIBGWY_DEPS_CFLAGS],["$GIO_CFLAGS $FFTW_CFLAGS"])
AC_SUBST([LIBGWY_DEPS_LIBS],["$GIO_LIBS $FFTW_LIBS"])

AC_SUBST([LIBGWYUI_COMPAT_CPPFLAGS],["-DGDK_DISABLE_DEPRECATED -DGTK_DISABLE_DEPRECATED"])
AC_SUBST([LIBGWYUI_DEPS_CFLAGS],["$GTK_CFLAGS $GDKPIXBUF_CFLAGS"])
AC_SUBST([LIBGWYUI_DEPS_LIBS],["$GTK_LIBS $GDKPIXBUF_LIBS"])

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

AC_CHECK_PROGS([GTESTER],[gtester],[false])

#AC_MSG_CHECKING([for gobject-query])
#GOBJECT_QUERY=`$PKG_CONFIG --variable=gobject_query glib-2.0`
#AC_MSG_RESULT([$GOBJECT_QUERY])
#AC_SUBST(GOBJECT_QUERY)

# FIXME: Once G-IR supports boxed types change this to the required version.
GOBJECT_INTROSPECTION_CHECK([0.10.0])

#############################################################################
# Other tools
AC_CHECK_PROGS([XSLTPROC],[xsltproc],[false])

#############################################################################
# Valgrind
AC_ARG_WITH([valgrind],
            [AS_HELP_STRING([--with-valgrind=ARG],[compile with Valgrind support for testing (no)])],
            [gwy_enable_valgrind=$withval],
            [gwy_enable_valgrind=no])
if test "x$gwy_enable_valgrind" = xyes; then
  PKG_CHECK_EXISTS([valgrind],
                   [gwy_enable_valgrind=yes
                    PKG_CHECK_MODULES([VALGRIND],[valgrind])
                    AC_DEFINE([HAVE_VALGRIND],1,[Define if we have valgrind])],
                   [gwy_enable_valgrind=no])
fi
AM_CONDITIONAL([HAVE_VALGRIND],[test "x$gwy_enable_valgrind" = xyes])

#############################################################################
# Benchmarking and tuning
AC_ARG_ENABLE([benchmarks],
              [AS_HELP_STRING([--enable-benchmarks],[compile benchmarking an tuning programs (no)])],
              [gwy_enable_benchmarks=$enableval],
              [gwy_enable_benchmarks=no])
AM_CONDITIONAL([BENCHMARKS],[test "x$gwy_enable_benchmarks" = xyes])

#############################################################################
# Translation Makefiles
AC_CONFIG_COMMANDS([sed-po-makefiles],
[
  for x in libgwy libgwyui; do
    sed -e "/POTFILES =/r po-$x/POTFILES" po-$x/Makefile.in > po-$x/Makefile \
      && touch po-$x/stamp-it
  done
])

#############################################################################
# Output
AC_OUTPUT

