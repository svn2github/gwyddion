# $Id$

bin_PROGRAMS = gwyddion4

gwyddion4_SOURCES = \
	gwyddion.c

AM_CFLAGS = $(LIBGWYAPP_DEPS_CFLAGS) $(HOST_CFLAGS) $(GWY_CFLAGS)
AM_CPPFLAGS = \
	-I$(top_builddir) \
	-I$(top_builddir)/libraries \
	-I$(top_srcdir)/libraries \
	$(LIBGWYAPP_COMPAT_CPPFLAGS) \
	-DGETTEXT_PACKAGE=\"$(GETTEXT_PACKAGE)\" \
	-DG_LOG_DOMAIN=\"Gwy\"

AM_LDFLAGS = \
	$(LIBGWYAPP_DEPS_LIBS) \
	$(HOST_LDFLAGS) \
	$(top_builddir)/libraries/libgwy/libgwy4.la \
	$(top_builddir)/libraries/libgwyui/libgwyui4.la \
	$(top_builddir)/libraries/libgwyapp/libgwyapp4.la

clean-local:
	rm -rf core.* *~ *.gcda *.gcno *.gcov gmon.out tmp-introspect*

