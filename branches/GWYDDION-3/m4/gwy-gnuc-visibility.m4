dnl Check for GNUC visibility support
dnl UNUSED
AC_DEFUN([GWY_CHECK_GNUC_VISIBILITY],
[
AC_MSG_CHECKING([for GNUC visibility attribute])
gwy_werror_flag="$ac_[]_AC_LANG_ABBREV[]_werror_flag"
ac_[]_AC_LANG_ABBREV[]_werror_flag=yes
AC_COMPILE_IFELSE([AC_LANG_SOURCE([[
void
__attribute__ ((visibility ("hidden")))
     f_hidden (void)
{
}
void
__attribute__ ((visibility ("internal")))
     f_internal (void)
{
}
void
__attribute__ ((visibility ("protected")))
     f_protected (void)
{
}
void
__attribute__ ((visibility ("default")))
     f_default (void)
{
}
int main (int argc, char **argv)
{
	f_hidden();
	f_internal();
	f_protected();
	f_default();
	return 0;
}
]])],
[gwy_have_gnuc_visibility=yes
AC_DEFINE([HAVE_GNUC_VISIBILITY],[1],[Define if GNU C visibility is supported.])
],[gwy_have_gnuc_visibility=no])
AC_MSG_RESULT([$gwy_have_gnuc_visibility])
AM_CONDITIONAL(HAVE_GNUC_VISIBILITY,[test x$gwy_have_gnuc_visibility = xyes])
ac_[]_AC_LANG_ABBREV[]_werror_flag="$GWY_werror_flag"
])
