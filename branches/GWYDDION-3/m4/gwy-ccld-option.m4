dnl Check if the linker supports specified option.
AC_DEFUN([GWY_PROG_CCLD_OPTION],
[AC_REQUIRE([AC_PROG_CC])dnl

dnl Check whether C linker accepts option $2, set GWY_PROG_CCLD_$1 to either
dnl the option or empty.  On success, $3 is executed, on failure, $4.
AC_CACHE_CHECK([whether $CC knows $2],
  [ac_cv_prog_ccld_option_$1],
  [GWY_PROG_CCLD_OPTION_cflags="$LDFLAGS"
   LDFLAGS="$LDFLAGS $2"
   AC_LINK_IFELSE([AC_LANG_PROGRAM()],
     [ac_cv_prog_ccld_option_$1=yes],
     [ac_cv_prog_ccld_option_$1=no])
   LDFLAGS="$GWY_PROG_CCLD_OPTION_cflags"])
if test "$ac_cv_prog_ccld_option_$1" = "yes"; then
  GWY_PROG_CCLD_$1="$2"
  $3
else
  GWY_PROG_CCLD_$1=
  $4
fi
])

dnl Check if the linker supports specified option.
AC_DEFUN([GWY_CHECK_PROG_CCLD_OPTION],
[AC_MSG_CHECKING([whether $CC knows $1])
GWY_PROG_CCLD_OPTION_cflags="$LDFLAGS"
LDFLAGS="$LDFLAGS $1"
AC_LINK_IFELSE(AC_LANG_PROGRAM([],[]),
  [AC_MSG_RESULT([yes])
   $2],
  [AC_MSG_RESULT([no])])
LDFLAGS="$GWY_PROG_CCLD_OPTION_cflags"
])

dnl Check which options the linker supports.
AC_DEFUN([GWY_PROG_CCLD_OPTIONS],
[AC_REQUIRE([AC_PROG_CC])dnl
GWY_options=
m4_foreach_w([Gwy_Opt],[$2],
  [GWY_CHECK_PROG_CCLD_OPTION(Gwy_Opt, [GWY_options="$GWY_options Gwy_Opt"])])
GWY_PROG_CCLD_$1="$GWY_options"
])
