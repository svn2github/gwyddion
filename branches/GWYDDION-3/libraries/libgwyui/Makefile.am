# $Id$

SUBDIRS = . tests

library = libgwyui
Library = GwyUI
library_minor_version = 0
libsuffix = $(PACKAGE_SUFFIX)
libgwyuiincludedir = $(pkgincludedir)$(PACKAGE_SUFFIX)/$(library)
#test_program = ./tests/testlibgwyui
#test_default_suppressions = libgwyui-default.supp
mkenum_name = types
genmarshal_name = marshal
genmarshal_prefix = _gwy_cclosure_marshal
GwyUI_4_0_gir_INCLUDES = Gtk-3.0
GwyUI_4_0_gir_PACKAGES = gtk+-3.0
GwyUI_4_0_gir_SCANNERFLAGS = \
	--include-uninstalled=$(top_builddir)/libraries/libgwy/Gwy-4.0.gir \
	--library=$(top_builddir)/libraries/libgwy/libgwy4.la
INTROSPECTION_COMPILER_ARGS = \
	--includedir=$(top_builddir)/libraries/libgwy

public_headers = \
	$(srcdir)/adjust-bar.h \
	$(srcdir)/adjustment.h \
	$(srcdir)/axis.h \
	$(srcdir)/cairo-utils.h \
	$(srcdir)/cell-renderer-gradient.h \
	$(srcdir)/choice.h \
	$(srcdir)/color-axis.h \
	$(srcdir)/coords-view.h \
	$(srcdir)/field-render.h \
	$(srcdir)/graph.h \
	$(srcdir)/graph-area.h \
	$(srcdir)/graph-axis.h \
	$(srcdir)/graph-curve.h \
	$(srcdir)/inventory-store.h \
	$(srcdir)/list-selection-binding.h \
	$(srcdir)/list-store.h \
	$(srcdir)/main.h \
	$(srcdir)/raster-area.h \
	$(srcdir)/raster-view.h \
	$(srcdir)/resource-list.h \
	$(srcdir)/ruler.h \
	$(srcdir)/scroller.h \
	$(srcdir)/shapes.h \
	$(srcdir)/shapes-line.h \
	$(srcdir)/shapes-point.h \
	$(srcdir)/shapes-rectangle.h \
	$(srcdir)/spin-button.h \
	$(srcdir)/stock.h \
	$(srcdir)/widget-utils.h

public_built_headers = \
	$(mkenum_name).h

private_headers = \
	axis-internal.h \
	graph-internal.h \
	shapes-internal.h

private_built_headers = \
	$(genmarshal_name).h \
	icons-gresource.h \
	ui-gresource.h

library_headers = $(public_headers) $(public_built_headers)
mkenum_headers = $(public_headers)
main_header = $(top_srcdir)/libraries/libgwyui.h
resource_files = \
	icons-gresource.c \
	icons-gresource.h \
	ui-gresource.c \
	ui-gresource.h

# Must init
EXTRA_DIST = $(test_default_suppressions)
CLEANFILES = $(library)$(libsuffix).supp
DISTCLEANFILES = $(public_built_headers) $(private_built_headers) $(resource_files)
MAINTAINERCLEANFILES =
libgwyui4_la_DEPENDENCIES =
BUILT_SOURCES = $(resource_files)

# lib_LTLIBRARIES must be a literal, no variables
lib_LTLIBRARIES = libgwyui4.la
nodist_libgwyuiinclude_HEADERS = $(public_built_headers)
libgwyuiinclude_HEADERS = $(public_headers)
noinst_HEADERS = $(private_headers) $(private_built_headers)

libgwyui4_la_SOURCES = \
	adjust-bar.c \
	adjustment.c \
	axis.c \
	cairo-utils.c \
	cell-renderer-gradient.c \
	choice.c \
	color-axis.c \
	coords-view.c \
	field-render.c \
	graph.c \
	graph-area.c \
	graph-axis.c \
	graph-curve.c \
	graph-internal.c \
	icons-gresource.c \
	inventory-store.c \
	list-selection-binding.c \
	list-store.c \
	main.c \
	marshal.c \
	raster-area.c \
	raster-view.c \
	resource-list.c \
	ruler.c \
	scroller.c \
	shapes.c \
	shapes-internal.c \
	shapes-line.c \
	shapes-point.c \
	shapes-rectangle.c \
	spin-button.c \
	stock.c \
	types.c \
	ui-gresource.c \
	widget-utils.c

AM_CFLAGS = $(LIBGWYUI_DEPS_CFLAGS) $(HOST_CFLAGS) $(GWY_CFLAGS)
AM_CPPFLAGS = \
	-I$(top_builddir) \
	-I$(top_builddir)/libraries \
	-I$(top_srcdir)/libraries \
	$(LIBGWYUI_COMPAT_CPPFLAGS) \
	-DGETTEXT_PACKAGE=\"$(GETTEXT_PACKAGE)-libgwyui\" \
	-DG_LOG_DOMAIN=\"Gwy\"

# In following order do:
# Any code change     C:   R++: A
# Any iface change    C++: 0:   A
# Adding ifaces       C:   R:   A++
# Changing ifaces     C:   R:   0
library_so_version = 0:0:0

libgwyui4_la_LDFLAGS = \
	$(LIBGWYUI_DEPS_LIBS) \
	$(top_builddir)/libraries/libgwy/libgwy4.la \
	$(HOST_LDFLAGS) \
	$(GWY_LIBRARY_LDFLAGS) \
	$(version_info) \
	$(library_host_ldflags)

#include $(top_srcdir)/build/gtester.make
include $(top_srcdir)/build/library.make
include $(top_srcdir)/build/mkenum.make
include $(top_srcdir)/build/genmarshal.make

ui-gresource.c: $(top_builddir)/resources/ui-gresource.c
	$(AM_V_GEN)cp -f $(top_builddir)/resources/ui-gresource.c ui-gresource.c

ui-gresource.h: $(top_builddir)/resources/ui-gresource.h
	$(AM_V_GEN)cp -f $(top_builddir)/resources/ui-gresource.h ui-gresource.h

icons-gresource.c: $(top_builddir)/resources/icons-gresource.c
	$(AM_V_GEN)cp -f $(top_builddir)/resources/icons-gresource.c icons-gresource.c

icons-gresource.h: $(top_builddir)/resources/icons-gresource.h
	$(AM_V_GEN)cp -f $(top_builddir)/resources/icons-gresource.h icons-gresource.h

clean-local:
	rm -rf core.* *~ *.gcda *.gcno *.gcov gmon.out tmp-introspect*

distclean-local:
	rm -rf coverage

