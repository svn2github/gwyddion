# Process this file with automake to produce Makefile.in

PEDANTIC = -pedantic -Wstrict-aliasing=1 -Wstrict-overflow -Winline
WARNINGS = -Wall -Wextra -Wstrict-prototypes -Wpointer-arith -Wstrict-aliasing -Wundef -Wshadow -Wredundant-decls -Wlogical-op

# Must init
EXTRA_DIST =
CLEANFILES =
MAINTAINERCLEANFILES =
BUILT_SOURCES =

library = libgwy
libgwyincludedir = $(pkgincludedir)/$(library)
test_programs = testlibgwy

lib_LTLIBRARIES = libgwy.la
noinst_PROGRAMS = $(test_programs)
libgwyinclude_HEADERS = \
	container.h \
	errorlist.h \
	expr.h \
	libgwy.h \
	macros.h \
	math.h \
	pack.h \
	serializable.h \
	serialize.h \
	unit.h

libgwy_la_SOURCES = \
	container.c \
	errorlist.c \
	expr.c \
	macros.c \
	math.c \
	pack.c \
	serializable.c \
	serialize.c \
	unit.c

testlibgwy_SOURCES = testlibgwy.c

AM_CFLAGS = @LIBGWY_CFLAGS@ $(WARNINGS)
AM_CPPFLAGS = \
	-I$(top_builddir) \
	-I$(top_srcdir) \
	-DG_LOG_DOMAIN=\"Gwy\"

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

libgwy_la_LDFLAGS = @LIBGWY_LIBS@ $(version_info)
testlibgwy_LDADD = libgwy.la

include $(top_srcdir)/build/gtester.make
include $(top_srcdir)/build/library.make

clean-local:
	rm -f core.* *~

