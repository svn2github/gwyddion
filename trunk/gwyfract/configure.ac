# @(#) $Id: configure.ac,v 1.119 2005/08/18 13:03:08 yeti-dn Exp $
AC_INIT(gwyfract, 0.8, [yeti@gwyddion.net])
AC_PREREQ(2.60)
AC_CONFIG_SRCDIR(gwyfract.c)
AC_CONFIG_MACRO_DIR(m4)
AC_CONFIG_FILES(Makefile)
AM_CONFIG_HEADER(config.h)
AM_INIT_AUTOMAKE([1.9 foreign dist-bzip2])
m4_defun([_LT_AC_LANG_CXX_CONFIG],[:])
m4_defun([_LT_AC_LANG_F77_CONFIG],[:])
AC_PROG_CC
AC_USE_SYSTEM_EXTENSIONS
AC_PROG_LIBTOOL
AM_PROG_CC_STDC
AC_PROG_INSTALL
AC_PROG_LN_S
if test "x$GCC" = xyes; then
  OPT_CFLAGS="-Wall -W -O3 -ffast-math"
fi
AC_CHECK_LIBM
LIBS="$LIBS $LIBM"
AC_CHECK_FUNCS([log2])
PKG_CHECK_MODULES(GTHREAD, [gthread-2.0])
PKG_CHECK_MODULES(GWYDDION, [gwyddion])
AC_CHECK_LIB(gmp, __gmpf_init,
  [AC_CHECK_HEADER(gmp.h,
     [LIBS="-lgmp $LIBS"],
     AC_MSG_ERROR([GNU MP arbitratry precision library headers not found]))
  ],
  AC_MSG_ERROR([GNU MP arbitratry precision library not found]))
AC_SUBST(EXTRA_CFLAGS, ["$OPT_CFLAGS"])
AC_SUBST(EXTRA_LIBS, ["$LIBS"])
AC_OUTPUT
