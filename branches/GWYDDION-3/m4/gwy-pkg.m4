dnl GWY_PKG_REQUIRE_MODULES(PREFIX, MODULES)
dnl Like PKG_CHECK_MODULES but does not fail immediately if something is
dnl missing.  Call GWY_PKG_RESULT for that at the end.
AC_DEFUN([GWY_PKG_REQUIRE_MODULES],
[AC_REQUIRE([_GWY_PKG_INIT])dnl
AC_BEFORE([$0],[GWY_PKG_FAIL])dnl
PKG_CHECK_MODULES([$1],[$2],,
[if test "x$_gwy_pkg_missing_deps" = x; then
  _gwy_pkg_missing_deps="$2"
else
  _gwy_pkg_missing_deps="$_gwy_pkg_missing_deps, $2"
fi])
])

dnl GWY_PKG_RESULT([NAME])
dnl Report all missing dependences from GWY_PKG_REQUIRE_MODULES(), if any.
dnl Since PKG_CHECK_MODULES prints a helpful message do that by reproducing the
dnl failure, this time noisily.  Unfortunately the part about setting
dnl REQUIREMENTS_CFLAGS is wrong then -- how to make it meaningful?
AC_DEFUN([GWY_PKG_RESULT],
[if test "x$_gwy_pkg_missing_deps" != x; then
  PKG_CHECK_MODULES(m4_default([$1],[REQUIREMENTS]),[$_gwy_pkg_missing_deps])
fi])

dnl Start with an empty missing requirements list.
AC_DEFUN([_GWY_PKG_INIT],[_gwy_pkg_missing_deps=])

