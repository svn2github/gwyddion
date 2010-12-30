dnl Check for eu-nm and try to figure out if it can read object files (we can
dnl have eu-nm but it might work with the binary format the compiler creates).
dnl Fall back to NM if eu-nm is not found or working, this may lead to lack of
dnl details in error messages but otherwise everything should work.
AC_DEFUN([GWY_CHECK_EU_NM],
[AC_CHECK_TOOL([EU_NM],[eu-nm],[:])
if test "$EU_NM" != :; then
AC_COMPILE_IFELSE(AC_LANG_PROGRAM([int some_variable = 0;],[]),
  [if $EU_NM -f sysv conftest.$ac_objext 2>/dev/null | $GREP '^@<:@^|@:>@*|@<:@^|@:>@*|@<:@^|@:>@*|@<:@^|@:>@*|@<:@^|@:>@*|@<:@^|@:>@*|@<:@^|@:>@*$' >/dev/null; then
     :
   else
     EU_NM=:
   fi],
 [EU_NM=:])
fi
if test "$EU_NM" = :; then
  EU_NM="$NM"
fi])
