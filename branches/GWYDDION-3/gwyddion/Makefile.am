# $Id$

bin_PROGRAMS = gwyddion3

gwyddion3_SOURCES = \
	gwyddion.c

AM_CFLAGS = $(LIBGWYAPP_DEPS_CFLAGS) $(HOST_CFLAGS) $(GWY_CFLAGS)
AM_CPPFLAGS = \
	-I$(top_builddir) \
	-I$(top_srcdir) \
	$(LIBGWYAPP_COMPAT_CPPFLAGS) \
	-DGETTEXT_PACKAGE=\"$(GETTEXT_PACKAGE)\" \
	-DG_LOG_DOMAIN=\"Gwy\" \
	-DGWY_LIBDIR=\"$(pkglibdir)\" \
	-DGWY_DATADIR=\"$(pkgdatadir)\" \
	-DGWY_LOCALE_DIR=\"$(localedir)\"

AM_LDFLAGS = \
	$(LIBGWYAPP_DEPS_LIBS) \
	$(HOST_LDFLAGS) \
	$(top_builddir)/libgwy/libgwy3.la \
	$(top_builddir)/libgwyui/libgwyui3.la

clean-local:
	rm -rf core.* *~ *.gcda *.gcno *.gcov gmon.out tmp-introspect*

