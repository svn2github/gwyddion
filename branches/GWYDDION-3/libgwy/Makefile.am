# Process this file with automake to produce Makefile.in
# $Id$

PEDANTIC = -pedantic -Wstrict-aliasing=1 -Wstrict-overflow -Winline
WARNINGS = -Wall -Wextra -Wstrict-prototypes -Werror=implicit-function-declaration -Werror=implicit-int -Wpointer-arith -Wstrict-aliasing -Wundef -Wshadow -Wredundant-decls -Wlogical-op

library = libgwy
libsuffix = 3
libgwyincludedir = $(pkgincludedir)/$(library)
test_programs = testlibgwy
mkenum_name = types
mkenum_id = LIBGWY_TYPES
main_header = libgwy.h
mkenum_headers = \
	container.h \
	error-list.h \
	expr.h \
	fitter.h \
	fit-task.h \
	inventory.h \
	interpolation.h \
	macros.h \
	main.h \
	math.h \
	pack.h \
	resource.h \
	rgba.h \
	serializable.h \
	serializable-boxed.h \
	serialize.h \
	strfuncs.h \
	unit.h \
	value-format.h \
	version.h

# Must init
EXTRA_DIST = version.h.in
CLEANFILES = $(library)$(libsuffix).supp
DISTCLEANFILES =
MAINTAINERCLEANFILES =
BUILT_SOURCES =

lib_LTLIBRARIES = libgwy3.la
noinst_PROGRAMS = $(test_programs)
libgwyinclude_HEADERS = $(mkenum_headers) $(mkenum_name).h
pkginclude_HEADERS = $(main_header)

libgwy3_la_SOURCES = \
	container.c \
	error-list.c \
	expr.c \
	fitter.c \
	fit-task.c \
	inventory.c \
	interpolation.c \
	macros.c \
	main.c \
	math.c \
	pack.c \
	resource.c \
	rgba.c \
	serializable.c \
	serializable-boxed.c \
	serialize.c \
	strfuncs.c \
	types.c \
	unit.c \
	value-format.c \
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

libgwy3_la_LDFLAGS = @LIBGWY_DEPS_LIBS@ $(version_info)

testlibgwy_CFLAGS = $(AM_CFLAGS) @VALGRIND_CFLAGS@
testlibgwy_LDADD = libgwy3.la

include $(top_srcdir)/build/gtester.make
include $(top_srcdir)/build/library.make
include $(top_srcdir)/build/mkenum.make

# Run the test program but do not execute any tests.  This produces a list of
# ‘standard’ GLib errors we then filter out.
$(library)$(libsuffix).supp: testlibgwy$(EXEEXT)
	$(LIBTOOL) --mode=execute valgrind --log-fd=5 --gen-suppressions=all \
	    --tool=memcheck --leak-check=full --show-reachable=no \
	    ./testlibgwy -l >/dev/null 5>$(library)$(libsuffix).supp
	$(SED) -i -e '/^==/d' \
	    -e 's/<insert a suppression name here>/Suppression/' \
	    $(library)$(libsuffix).supp

# Run the test program with all tests (unless TEST_FLAGS says otherwise) under
# valgrind and report any problems.
check-valgrind: $(library)$(libsuffix).supp
	$(LIBTOOL) --mode=execute valgrind --tool=memcheck --leak-check=full \
	    --show-reachable=no --suppressions=$(library)$(libsuffix).supp \
	    ./testlibgwy $(TEST_FLAGS)

clean-local:
	rm -f core.* *~

