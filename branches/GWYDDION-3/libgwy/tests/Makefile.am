# $Id$

noinst_HEADERS = testlibgwy.h test-list.h test-list.c
noinst_PROGRAMS = testlibgwy

testlibgwy_SOURCES = \
	array.c \
	boxed-serialization.c \
	brick.c \
	brick-part.c \
	container.c \
	curve.c \
	error-list.c \
	expr.c \
	fft.c \
	field.c \
	field-part.c \
	fitting.c \
	fit-funcs.c \
	gl-material.c \
	gradient.c \
	interpolation.c \
	inventory.c \
	line-part.c \
	line.c \
	mask-field.c \
	mask-line.c \
	math.c \
	object-utils.c \
	pack.c \
	rand.c \
	resource.c \
	rgba.c \
	serialization.c \
	strfuncs.c \
	surface.c \
	unit.c \
	user-fit-func.c \
	value-format.c \
	version.c \
	xy.c \
	testlibgwy.c

EXTRA_DIST = test-list.txt
BUILT_SOURCES = test-list.h test-list.c

expand_test_list = $(PYTHON) $(top_srcdir)/build/expand-test-list.py

test-list.h: test-list.txt
	$(AM_V_GEN) $(expand_test_list) H testlibgwy <test-list.txt >test-list.h

test-list.c: test-list.txt
	$(AM_V_GEN) $(expand_test_list) C testlibgwy <test-list.txt >test-list.c

AM_CFLAGS = $(LIBGWY_DEPS_CFLAGS) $(VALGRIND_CFLAGS) $(GWY_CFLAGS)
AM_CPPFLAGS = -I$(top_builddir) -I$(top_srcdir) $(LIBGWY_COMPAT_CPPFLAGS)
AM_LDFLAGS = ../libgwy3.la

# The tests are run in the parent directory
test:

test-report:

clean-local:
	rm -rf core.* *~ *.gcda *.gcno *.gcov gmon.out test-list.[ch]
