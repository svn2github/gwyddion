# $Id$

moduledir = $(pkglibdir)$(PACKAGE_SUFFIX)/modules

module_LTLIBRARIES = \
	test.la

test_la_SOURCES = test.c

AM_CFLAGS = $(LIBGWYAPP_DEPS_CFLAGS) $(HOST_CFLAGS) $(GWY_CFLAGS)
AM_CPPFLAGS = \
	-I$(top_builddir) \
	-I$(top_srcdir) \
	$(LIBGWYAPP_COMPAT_CPPFLAGS) \
	-DGETTEXT_PACKAGE=\"$(GETTEXT_PACKAGE)\" \
	-DG_LOG_DOMAIN=\"Gwy\"

AM_LDFLAGS = -avoid-version -module $(HOST_LDFLAGS) $(module_libadd)

if LINK_MODULE_DEPENDENCIES
module_libadd = \
	$(LIBGWYAPP_DEPS_LIBS) \
	$(top_builddir)/libgwy/libgwy4.la \
	$(top_builddir)/libgwyui/libgwyui4.la \
	$(top_builddir)/libgwyapp/libgwyapp4.la
endif

clean-local:
	rm -rf core.* *~ *.gcda *.gcno *.gcov gmon.out

