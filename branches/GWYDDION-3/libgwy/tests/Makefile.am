# $Id$

PEDANTIC = -pedantic -Wstrict-aliasing=1 -Wstrict-overflow -Winline
WARNINGS = -Wall -Wextra -Wstrict-prototypes -Werror=implicit-function-declaration -Werror=implicit-int -Wpointer-arith -Wstrict-aliasing -Wundef -Wshadow -Wredundant-decls -Wlogical-op

noinst_PROGRAMS = testlibgwy

testlibgwy_SOURCES = \
	testlibgwy.c

AM_CFLAGS = @LIBGWY_DEPS_CFLAGS@ @VALGRIND_CFLAGS@ $(WARNINGS)
AM_CPPFLAGS = \
	-I$(top_builddir) \
	-I$(top_srcdir) \
	-DG_DISABLE_DEPRECATED -DG_DISABLE_SINGLE_INCLUDES -DGSEAL_ENABLE
AM_LDFLAGS = ../libgwy3.la

test:

clean-local:
	rm -rf core.* *~ *.gcda *.gcno *.gcov gmon.out
