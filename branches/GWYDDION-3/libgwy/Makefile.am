# Process this file with automake to produce Makefile.in
# $Id$

PEDANTIC = -pedantic -Wstrict-aliasing=1 -Wstrict-overflow -Winline
WARNINGS = -Wall -Wextra -Wstrict-prototypes -Wpointer-arith -Wstrict-aliasing -Wundef -Wshadow -Wredundant-decls -Wlogical-op

library = libgwy
libgwyincludedir = $(pkgincludedir)/$(library)
test_programs = testlibgwy

# Must init
EXTRA_DIST = version.h.in
CLEANFILES = $(library).supp
MAINTAINERCLEANFILES =
BUILT_SOURCES =

lib_LTLIBRARIES = libgwy.la
noinst_PROGRAMS = $(test_programs)
libgwyinclude_HEADERS = \
	container.h \
	error-list.h \
	expr.h \
	inventory.h \
	libgwy.h \
	macros.h \
	math.h \
	pack.h \
	resource.h \
	serializable.h \
	serialize.h \
	strfuncs.h \
	unit.h \
	version.h

libgwy_la_SOURCES = \
	container.c \
	error-list.c \
	expr.c \
	inventory.c \
	libgwy.c \
	macros.c \
	math.c \
	pack.c \
	resource.c \
	serializable.c \
	serialize.c \
	strfuncs.c \
	unit.c \
	version.c

testlibgwy_SOURCES = testlibgwy.c

AM_CFLAGS = @LIBGWY_DEPS_CFLAGS@ $(WARNINGS)
AM_CPPFLAGS = \
	-I$(top_builddir) \
	-I$(top_srcdir) \
	-DGETTEXT_PACKAGE=\"@GETTEXT_PACKAGE@-libgwy\" \
	-DG_LOG_DOMAIN=\"Gwy\" \
	-DGWY_LIBDIR=\"$(pkglibdir)\" \
	-DGWY_DATADIR=\"$(pkgdatadir)\" \
	-DGWY_LOCALE_DIR=\"$(localedir)\"

# In following order do:
# Any code change     C:   R++: A
# Any iface change    C++: 0:   A
# Adding ifaces       C:   R:   A++
# Changing ifaces     C:   R:   0
if UNSTABLE_LIBRARY_RELEASE
version_info = -release @GWY_VERSION_STRING@
else
version_info = -version-info 0:0:0
endif

libgwy_la_LDFLAGS = @LIBGWY_DEPS_LIBS@ $(version_info)

testlibgwy_CFLAGS = $(AM_CFLAGS) @VALGRIND_CFLAGS@
testlibgwy_LDADD = libgwy.la

include $(top_srcdir)/build/gtester.make
include $(top_srcdir)/build/library.make

# Run the test program but do not execute any tests.  This produces a list of
# ‘standard’ GLib error we want to filter out.
$(library).supp: testlibgwy$(EXEEXT)
	$(LIBTOOL) --mode=execute valgrind --log-fd=5 --gen-suppressions=all \
	    --tool=memcheck --leak-check=full --show-reachable=yes \
	    ./testlibgwy -l >/dev/null 5>$(library).supp
	$(SED) -i -e '/^==/d' \
	    -e 's/<insert a suppression name here>/Suppression/' \
	    $(library).supp

# Run the test program with all tests (unless TEST_FLAGS says otherwise) under
# valgrind and report any problems.
check-valgrind: $(library).supp
	$(LIBTOOL) --mode=execute valgrind --tool=memcheck --leak-check=full \
	    --show-reachable=yes --suppressions=$(library).supp \
	    ./testlibgwy $(TEST_FLAGS)

clean-local:
	rm -f core.* *~

