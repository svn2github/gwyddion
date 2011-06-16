dnl Check if the compiler supports specified option.
AC_DEFUN([GWY_PROG_CC_OPTION],
[AC_REQUIRE([AC_PROG_CC])dnl
dnl Check whether C compiler accepts option $2, set GWY_PROG_CC_$1 to either
dnl the option or empty.  On success, $3 is executed, on failure, $4.
AC_CACHE_CHECK([whether $CC knows $2],
  [ac_cv_prog_cc_option_$1],
  [GWY_PROG_CC_OPTION_cflags="$CFLAGS"
   CFLAGS="$CFLAGS $2"
   AC_COMPILE_IFELSE([AC_LANG_PROGRAM()],
     [ac_cv_prog_cc_option_$1=yes],
     [ac_cv_prog_cc_option_$1=no])
   CFLAGS="$GWY_PROG_CC_OPTION_cflags"])
if test "x$ac_cv_prog_cc_option_$1" = xyes; then
  GWY_PROG_CC_$1="$2"
  $3
else
  GWY_PROG_CC_$1=
  $4
fi
])

dnl Check if the compiler supports specified option.
AC_DEFUN([GWY_CHECK_PROG_CC_OPTION],
[AC_MSG_CHECKING([whether $CC knows $1])
GWY_PROG_CC_OPTION_cflags="$CFLAGS"
CFLAGS="$CFLAGS $1"
AC_COMPILE_IFELSE([AC_LANG_PROGRAM()],
  [AC_MSG_RESULT([yes])
   $2],
  [AC_MSG_RESULT([no])])
CFLAGS="$GWY_PROG_CC_OPTION_cflags"
])

dnl Check which options the compiler supports.
AC_DEFUN([GWY_PROG_CC_OPTIONS],
[AC_REQUIRE([AC_PROG_CC])dnl
GWY_options=
m4_foreach_w([Gwy_Opt],[$2],
  [GWY_CHECK_PROG_CC_OPTION(Gwy_Opt, [GWY_options="$GWY_options Gwy_Opt"])])
GWY_PROG_CC_$1="$GWY_options"
])
