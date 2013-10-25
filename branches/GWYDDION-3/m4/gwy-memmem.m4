dnl Check for know working memmem version.
AC_DEFUN([GWY_CHECK_MEMMEM],
[
AC_REQUIRE([GWY_GLIBC])
AC_CHECK_FUNCS([memmem])
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
])
