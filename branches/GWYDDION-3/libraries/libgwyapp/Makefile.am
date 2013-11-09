# $Id$

library = libgwyapp
Library = GwyApp
library_minor_version = 0
libsuffix = $(PACKAGE_SUFFIX)
libgwyappincludedir = $(pkgincludedir)$(PACKAGE_SUFFIX)/$(library)
#test_program = ./tests/testlibgwyapp
#test_default_suppressions = libgwyapp-default.supp
mkenum_name = types
genmarshal_name = marshal
genmarshal_prefix = _gwy_cclosure_marshal
GwyApp_4_0_gir_INCLUDES = Gtk-3.0
GwyApp_4_0_gir_PACKAGES = gtk+-3.0
GwyApp_4_0_gir_SCANNERFLAGS = \
	--include-uninstalled=$(top_builddir)/libraries/libgwy/Gwy-4.0.gir \
	--include-uninstalled=$(top_builddir)/libraries/libgwyui/GwyUI-4.0.gir \
	--library=$(top_builddir)/libraries/libgwy/libgwy4.la \
	--library=$(top_builddir)/libraries/libgwyui/libgwyui4.la
INTROSPECTION_COMPILER_ARGS = \
	--includedir=$(top_builddir)/libraries/libgwy \
	--includedir=$(top_builddir)/libraries/libgwyui

public_headers = \
	$(srcdir)/data-list.h \
	$(srcdir)/data.h \
	$(srcdir)/file.h \
	$(srcdir)/module.h

public_built_headers = \
	$(mkenum_name).h

private_headers =

private_built_headers =

library_headers = $(public_headers) $(public_built_headers)
mkenum_headers = $(public_headers)
main_header = $(top_srcdir)/libraries/libgwyapp.h

# Must init
EXTRA_DIST = $(test_default_suppressions)
CLEANFILES = $(library)$(libsuffix).supp
DISTCLEANFILES = $(public_built_headers) $(private_built_headers) $(resource_files)
MAINTAINERCLEANFILES =
libgwyapp4_la_DEPENDENCIES =
BUILT_SOURCES = $(resource_files)

# lib_LTLIBRARIES must be a literal, no variables
lib_LTLIBRARIES = libgwyapp4.la
nodist_libgwyappinclude_HEADERS = $(public_built_headers)
libgwyappinclude_HEADERS = $(public_headers)
noinst_HEADERS = $(private_headers) $(private_built_headers)

libgwyapp4_la_SOURCES = \
	data-list.c \
	data.c \
	file.c \
	module.c \
	types.c

AM_CFLAGS = $(LIBGWYAPP_DEPS_CFLAGS) $(HOST_CFLAGS) $(GWY_CFLAGS)
AM_CPPFLAGS = \
	-I$(top_builddir) \
	-I$(top_builddir)/libraries \
	-I$(top_srcdir)/libraries \
	$(LIBGWYAPP_COMPAT_CPPFLAGS) \
	-DGETTEXT_PACKAGE=\"$(GETTEXT_PACKAGE)-libgwyapp\" \
	-DG_LOG_DOMAIN=\"Gwy\"

# In following order do:
# Any code change     C:   R++: A
# Any iface change    C++: 0:   A
# Adding ifaces       C:   R:   A++
# Changing ifaces     C:   R:   0
library_so_version = 0:0:0

libgwyapp4_la_LDFLAGS = \
	$(LIBGWYAPP_DEPS_LIBS) \
	$(top_builddir)/libraries/libgwyui/libgwyui4.la \
	$(top_builddir)/libraries/libgwy/libgwy4.la \
	$(HOST_LDFLAGS) \
	$(GWY_LIBRARY_LDFLAGS) \
	$(version_info) \
	$(library_host_ldflags)

#include $(top_srcdir)/build/gtester.make
include $(top_srcdir)/build/library.make
include $(top_srcdir)/build/mkenum.make
#include $(top_srcdir)/build/genmarshal.make

clean-local:
	rm -rf core.* *~ *.gcda *.gcno *.gcov gmon.out tmp-introspect*

distclean-local:
	rm -rf coverage

