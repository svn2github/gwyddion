dnl Copyright (C) 2000-2002, 2004 Free Software Foundation, Inc.
dnl This file is free software; the Free Software Foundation
dnl gives unlimited permission to copy and/or distribute it,
dnl with or without modifications, as long as this notice is preserved.

# Test for the GNU C Library.
# From Bruno Haible.

AC_DEFUN([GWY_GLIBC],
  [
    AC_CACHE_CHECK(whether we are using the GNU C Library,
      ac_cv_gnu_library,
      [AC_EGREP_CPP([Lucky GNU user],
	[
#include <features.h>
#ifdef __GNU_LIBRARY__
Lucky GNU user
#endif
	],
	ac_cv_gnu_library=yes,
	ac_cv_gnu_library=no)
      ]
    )
    AC_SUBST(GLIBC)
    GLIBC="$ac_cv_gnu_library"
  ]
)
