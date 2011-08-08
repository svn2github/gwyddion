# $Id$

library = libgwyui
Library = GwyUI
library_minor_version = 0
libsuffix = $(PACKAGE_SUFFIX)
toplevelincludedir = $(pkgincludedir)$(PACKAGE_SUFFIX)
libgwyuiincludedir = $(pkgincludedir)$(PACKAGE_SUFFIX)/$(library)
#test_program = ./tests/testlibgwyui
#test_default_suppressions = libgwyui-default.supp
mkenum_name = types
mkenum_id = LIBGWYUI_TYPES
#genmarshal_name = marshal
#genmarshal_prefix = _gwy_
main_header = $(srcdir)/libgwyui.h
GwyUI3_3_0_gir_INCLUDES = Gtk-3.0
GwyUI3_3_0_gir_PACKAGES = gtk+-3.0
GwyUI3_3_0_gir_SCANNERFLAGS = --include-uninstalled=$(top_builddir)/libgwy/Gwy3-3.0.gir --library=$(top_builddir)/libgwy/libgwy3.la
INTROSPECTION_COMPILER_ARGS = --includedir=$(top_builddir)/libgwy

public_headers = \
	$(srcdir)/field-render.h \
	$(srcdir)/raster-view.h

public_built_headers = \
	$(mkenum_name).h

private_headers =

library_headers = $(public_headers) $(public_built_headers)
mkenum_headers = $(public_headers)

# Must init
EXTRA_DIST = $(test_default_suppressions)
CLEANFILES = $(library)$(libsuffix).supp
DISTCLEANFILES = $(public_built_headers)
MAINTAINERCLEANFILES =
libgwyui3_la_DEPENDENCIES =
BUILT_SOURCES =

# lib_LTLIBRARIES must be a literal, no variables
lib_LTLIBRARIES = libgwyui3.la
nodist_libgwyuiinclude_HEADERS = $(public_built_headers)
libgwyuiinclude_HEADERS = $(public_headers)
toplevelinclude_HEADERS = $(main_header)
noinst_HEADERS = $(private_headers)

libgwyui3_la_SOURCES = \
	field-render.c \
	raster-view.c

AM_CFLAGS = $(LIBGWYUI_DEPS_CFLAGS) $(HOST_CFLAGS) $(GWY_CFLAGS)
AM_CPPFLAGS = \
	-I$(top_builddir) \
	-I$(top_srcdir) \
	$(LIBGWYUI_COMPAT_CPPFLAGS) \
	-DGETTEXT_PACKAGE=\"$(GETTEXT_PACKAGE)-libgwyui\" \
	-DG_LOG_DOMAIN=\"Gwy\" \
	-DGWY_LIBDIR=\"$(pkglibdir)\" \
	-DGWY_DATADIR=\"$(pkgdatadir)\" \
	-DGWY_LOCALE_DIR=\"$(localedir)\"

# In following order do:
# Any code change     C:   R++: A
# Any iface change    C++: 0:   A
# Adding ifaces       C:   R:   A++
# Changing ifaces     C:   R:   0
library_so_version = 0:0:0

libgwyui3_la_LDFLAGS = \
	$(LIBGWYUI_DEPS_LIBS) \
	$(top_builddir)/libgwy/libgwy3.la \
	$(HOST_LDFLAGS) \
	$(GWY_LIBRARY_LDFLAGS) \
	$(version_info) \
	$(library_host_ldflags)

#include $(top_srcdir)/build/gtester.make
include $(top_srcdir)/build/library.make
include $(top_srcdir)/build/mkenum.make

clean-local:
	rm -rf core.* *~ *.gcda *.gcno *.gcov gmon.out tmp-introspect*

distclean-local:
	rm -rf coverage

