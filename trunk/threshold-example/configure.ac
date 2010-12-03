dnl Lines you likely have to change in your own modules are marked with #####.

#####AC_INIT([Module name], [module version], [bugreport address])
AC_INIT([Threshold-example], [2.1], [yeti@gwyddion.net])
AC_PREREQ(2.60)
#####MODULE_TYPE=file, process, graph, tool, layer or leave empty
MODULE_TYPE=process
#####AC_CONFIG_SRCDIR([any-source-file-of-the-module.c])
AC_CONFIG_SRCDIR(threshold-example.c)
AC_CONFIG_MACRO_DIR(m4)
AC_CONFIG_FILES(Makefile)
AC_CONFIG_HEADER(config.h)
AC_CANONICAL_HOST
AM_INIT_AUTOMAKE([1.11 silent-rules foreign dist-bzip2])
AC_DISABLE_STATIC
AC_PROG_CC
AC_USE_SYSTEM_EXTENSIONS
AC_LIBTOOL_WIN32_DLL
AC_PROG_LIBTOOL
AC_PROG_INSTALL
#####PKG_CHECK_MODULES(GWYDDION, [gwyddion >= minimum-required-version])
PKG_CHECK_MODULES(GWYDDION, [gwyddion >= 2.8])
AC_ARG_ENABLE([home-installation],
              [Install into your personal Gwyddion module directory],
  [GWYDDION_MODULE_DIR=$HOME/.gwyddion/modules/$MODULE_TYPE],
  [GWYDDION_MODULE_DIR=`$PKG_CONFIG --variable=gwymoduledir gwyddion`])
AC_SUBST([GWYDDION_MODULE_DIR])
#############################################################################
# Win32.
AC_MSG_CHECKING([for native Win32])
case "$host_os" in
  mingw*)
    os_win32=yes
    HOST_CFLAGS="-mms-bitfields"
    HOST_LDFLAGS="-Wl,-subsystem,windows -no-undefined"
    ;;
  *)
    os_win32=no
    ;;
esac
AC_MSG_RESULT([$os_win32])
AC_SUBST([HOST_CFLAGS])
AC_SUBST([HOST_LDFLAGS])
#############################################################################
# GCC.
if test "$GCC" = yes; then
  WARNING_CFLAGS="-Wall -W"
fi
AC_SUBST([WARNING_CFLAGS])
#############################################################################
AC_OUTPUT
# vim: set ts=2 sw=2 et :
