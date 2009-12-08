# $Id$

PEDANTIC = -pedantic -Wstrict-aliasing=1 -Wstrict-overflow -Winline
WARNINGS = -Wall -Wextra -Wstrict-prototypes -Werror=implicit-function-declaration -Werror=implicit-int -Wpointer-arith -Wstrict-aliasing -Wundef -Wshadow -Wredundant-decls -Wlogical-op

library = libgwy
libsuffix = $(PACKAGE_SUFFIX)
libgwyincludedir = $(pkgincludedir)$(PACKAGE_SUFFIX)/$(library)
test_program = testlibgwy
mkenum_name = types
mkenum_id = LIBGWY_TYPES
main_header = libgwy.h
mkenum_headers = \
	array.h \
	container.h \
	error-list.h \
	expr.h \
	fitter.h \
	fit-task.h \
	gradient.h \
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

library_headers = $(mkenum_headers) $(mkenum_name).h

# Must init
EXTRA_DIST = version.h.in
CLEANFILES = $(library)$(libsuffix).supp
DISTCLEANFILES =
MAINTAINERCLEANFILES =
BUILT_SOURCES =

lib_LTLIBRARIES = libgwy3.la
noinst_PROGRAMS = $(test_program)
libgwyinclude_HEADERS = $(library_headers)
pkginclude_HEADERS = $(main_header)

libgwy3_la_SOURCES = \
	array.c \
	container.c \
	error-list.c \
	expr.c \
	fitter.c \
	fit-task.c \
	gradient.c \
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
	-DG_DISABLE_DEPRECATED -DG_DISABLE_SINGLE_INCLUDES -DGSEAL_ENABLE \
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

clean-local:
	rm -f core.* *~

