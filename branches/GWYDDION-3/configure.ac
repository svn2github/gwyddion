dnl Process this file with autoconf to produce a configure script.
m4_define([gwy_version_major],[3])
m4_define([gwy_version_minor],[0])
m4_define([gwy_version_extra],[])
m4_define([gwy_version_string],
          [gwy_version_major.gwy_version_minor[]gwy_version_extra])
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
  build/check-dep-versions.py
  resources/Makefile
  resources/gradients/Makefile
  libgwy/Makefile
  libgwy/version.h
  libgwy/libgwy3.pc
  libgwy/tests/Makefile
  libgwy/benchmarks/Makefile
  libgwyui/Makefile
  libgwyui/libgwyui3.pc
  libgwyui/tests/Makefile
  libgwyapp/Makefile
  libgwyapp/libgwyapp3.pc
  gwyddion/Makefile
  po/Makefile.in
  po-libgwy/Makefile.in
  po-libgwyui/Makefile.in
  po-libgwyapp/Makefile.in
  docs/Makefile
  docs/libgwy/Makefile
  docs/libgwyui/Makefile
  docs/libgwyapp/Makefile
  data/Makefile
  gwyddion3.spec:data/gwyddion3.spec.in
])
AC_CONFIG_COMMANDS([check-dep-versions],
                   [chmod +x build/check-dep-versions.py])

AM_INIT_AUTOMAKE([1.13 gnu -Wall -Wno-portability dist-xz no-dist-gzip std-options nostdinc])
m4_ifdef([AM_SILENT_RULES],[AM_SILENT_RULES([yes])])

AC_SUBST([GWY_VERSION_MAJOR],[gwy_version_major])
AC_SUBST([GWY_VERSION_MINOR],[gwy_version_minor])
AC_SUBST([GWY_VERSION_STRING],[gwy_version_string])
AC_SUBST([PACKAGE_SUFFIX],[3])
AM_CONDITIONAL([UNSTABLE_LIBRARY_RELEASE],[true])

AC_CANONICAL_HOST

AC_DISABLE_STATIC
AC_PROG_CC_C99
AC_PROG_CXX
AC_USE_SYSTEM_EXTENSIONS
AC_PROG_LIBTOOL
LT_PREREQ([2.0])
LT_INIT([dlopen win32-dll shared disable-static])
AC_PROG_INSTALL
GWY_CHECK_EU_NM

gwy_lib_path=`echo "$sys_lib_search_path_spec" | sed -e 's/  */:/g'`
AC_DEFINE_UNQUOTED([GWY_LIB_PATH],["$gwy_lib_path"],
                   [Define to system library search path])

m4_define([gwy_required_pkgconfig],[0.22])
AC_SUBST([GWY_REQUIRED_PKGCONFIG],[gwy_required_pkgconfig])
PKG_PROG_PKG_CONFIG([gwy_required_pkgconfig])

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
GWY_PROG_CC_OPTIONS([WARNINGS],[-Wall -Wextra -Wstrict-prototypes -Wpointer-arith -Wstrict-aliasing -Wundef -Wshadow -Wredundant-decls -Wredundant-decls -Wlogical-op -Werror=implicit-function-declaration -Werror=implicit-int -Werror=return-type])

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

#############################################################################
# Headers
## All required headers appear to be standard (on Win32, at least with MinGW).

#############################################################################
# I18n
GETTEXT_PACKAGE=gwyddion3
AM_GNU_GETTEXT([external])
m4_define([gwy_required_gettext],[0.18])
AC_SUBST([GWY_REQUIRED_GETTEXT],[gwy_required_gettext])
# Autopoint was written by monkeys.  Must spell out the version.
AM_GNU_GETTEXT_VERSION([0.18])
AC_SUBST(GETTEXT_PACKAGE)

m4_define([gwy_required_intltool],[0.41])
AC_SUBST([GWY_REQUIRED_INTLTOOL],[gwy_required_intltool])
IT_PROG_INTLTOOL([gwy_required_intltool],[no-xml])

#############################################################################
# Gtk-doc
m4_define([gwy_required_gtkdoc],[1.19])
AC_SUBST([GWY_REQUIRED_GTKDOC],[gwy_required_gtkdoc])
GTK_DOC_CHECK([gwy_required_gtkdoc])

#############################################################################
# Python
m4_define([gwy_required_python],[2.2])
AC_SUBST([GWY_REQUIRED_PYTHON],[gwy_required_python])
AM_PATH_PYTHON([gwy_required_python])

#############################################################################
# Libc functions
GWY_GLIBC
GWY_CHECK_LIBM
AC_CHECK_FUNCS([sincos])
GWY_CHECK_MEMMEM

#############################################################################
# Base libraries
# All libraries that are required should be checked here.
# TODO: use macros from m4/gwy-pkg.m4 once we can make the errors meaningful.
# XXX: Investigate if using AM PATH_GTK_3_0 and PATH_GLIB_2_0, which are
#      complicated and run test programs, would bring anything useful.
m4_define([gwy_required_glib],[2.36])
AC_SUBST([GWY_REQUIRED_GLIB],[gwy_required_glib])
PKG_CHECK_MODULES([GIO],[gio-2.0 >= gwy_required_glib])
PKG_CHECK_MODULES([GMODULE],[gmodule-2.0 >= gwy_required_glib])

m4_define([gwy_required_fftw],[3.3])
AC_SUBST([GWY_REQUIRED_FFTW],[gwy_required_fftw])
PKG_CHECK_MODULES([FFTW],[fftw3 >= gwy_required_fftw])

m4_define([gwy_required_gdkpixbuf],[2.22])
AC_SUBST([GWY_REQUIRED_GDKPIXBUF],[gwy_required_gdkpixbuf])
PKG_CHECK_MODULES([GDKPIXBUF],[gdk-pixbuf-2.0 >= gwy_required_gdkpixbuf])

m4_define([gwy_required_gtk],[3.6])
AC_SUBST([GWY_REQUIRED_GTK],[gwy_required_gtk])
PKG_CHECK_MODULES([GTK],[gtk+-3.0 >= gwy_required_gtk])

# Needs to be configured out in stable versions.  Deprecations come and go.
AC_SUBST([LIBGWY_COMPAT_CPPFLAGS],["-DG_DISABLE_DEPRECATED -DG_DISABLE_SINGLE_INCLUDES -DGSEAL_ENABLE"])
AC_SUBST([LIBGWY_DEPS_CFLAGS],["$GIO_CFLAGS $FFTW_CFLAGS"])
AC_SUBST([LIBGWY_DEPS_LIBS],["$GIO_LIBS $FFTW_LIBS $LIBM"])

AC_SUBST([LIBGWYUI_COMPAT_CPPFLAGS],["$LIBGWY_COMPAT_CPPFLAGS -DGDK_DISABLE_DEPRECATED -DGTK_DISABLE_DEPRECATED"])
AC_SUBST([LIBGWYUI_DEPS_CFLAGS],["$GTK_CFLAGS $GDKPIXBUF_CFLAGS"])
AC_SUBST([LIBGWYUI_DEPS_LIBS],["$GTK_LIBS $GDKPIXBUF_LIBS"])

AC_SUBST([LIBGWYAPP_COMPAT_CPPFLAGS],["$LIBGWYUI_COMPAT_CPPFLAGS"])
AC_SUBST([LIBGWYAPP_DEPS_CFLAGS],["$GTK_CFLAGS $GDKPIXBUF_CFLAGS $GMODULE_CFLAGS"])
AC_SUBST([LIBGWYAPP_DEPS_LIBS],["$GTK_LIBS $GDKPIXBUF_LIBS $GMODULE_LIBS"])

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

AC_CHECK_PROGS([GLIB_COMPILE_RESOURCES],[glib-compile-resources],[false])
if test "x$GLIB_COMPILE_RESOURCES" = xfalse; then
  AC_MSG_ERROR([glib-compile-resources is required to compile Gwyddion])
fi

AC_CHECK_PROGS([GTESTER],[gtester],[false])

AC_PATH_PROG([GDK_PIXBUF_PIXDATA],[gdk-pixbuf-pixdata],[`which cp`])

# FIXME: Once G-IR supports boxed types change this to the required version.
m4_define([gwy_required_gir],[0.10.0])
AC_SUBST([GWY_REQUIRED_GIR],[gwy_required_gir])
GOBJECT_INTROSPECTION_CHECK([gwy_required_gir])

#############################################################################
# Other tools

# cairo-xlib is used for widget screenshots (render-widgets) in documentation.
PKG_CHECK_MODULES([CAIRO_XLIB],[cairo-xlib],
                  [gwy_enable_cairo_xlib=yes],[gwy_enable_cairo_xlib=no])
AM_CONDITIONAL([HAVE_CAIRO_XLIB],[test "x$gwy_enable_cairo_xlib" = xyes])

# xsltproc is used to generate test reports
AC_CHECK_PROGS([XSLTPROC],[xsltproc],[false])
AM_CONDITIONAL([HAVE_XSLTPROC],[test "x$XSLTPROC" != xfalse])

# gimp-console is used to export PNG icons from XCF
AC_CHECK_PROGS([GIMP_CONSOLE],[gimp-console],[false])
AM_CONDITIONAL([HAVE_GIMP_CONSOLE],[test "x$GIMP_CONSOLE" != xfalse])

# inkscape is used to export PNG from SVG drawings in documentation
AC_CHECK_PROGS([INKSCAPE],[inkscape],[false])
AM_CONDITIONAL([HAVE_INKSCAPE],[test "x$INKSCAPE" != xfalse])

#############################################################################
# Valgrind
AC_ARG_WITH([valgrind],
            [AS_HELP_STRING([--with-valgrind=ARG],[compile with Valgrind support for testing (no)])],
            [gwy_enable_valgrind=$withval],
            [gwy_enable_valgrind=yes])
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
  for x in libgwy libgwyui libgwyapp; do
    sed -e "/POTFILES =/r po-$x/POTFILES" po-$x/Makefile.in > po-$x/Makefile \
      && touch po-$x/stamp-it
  done
])

#############################################################################
# Output
AC_OUTPUT

