# @(#) $Id$

libgwyddion = $(top_builddir)/libgwyddion/libgwyddion2.la
libgwymodule = $(top_builddir)/libgwymodule/libgwymodule2.la
libgwyprocess = $(top_builddir)/libprocess/libgwyprocess2.la
libgwydraw = $(top_builddir)/libdraw/libgwydraw2.la
libgwydgets = $(top_builddir)/libgwydgets/libgwydgets2.la

libgwyapp2includedir = $(pkgincludedir)/app

libgwyapp2include_HEADERS = \
	app.h \
	data-browser.h \
	datachooser.h \
	file.h \
	filelist.h \
	funcuse.h \
	glmaterial-editor.h \
	gradient-editor.h \
	gwyapp.h \
	gwyapptypes.h \
	gwymoduleutils.h \
	gwymoduleutils-file.h \
	gwyplaintool.h \
	gwytool.h \
	menu.h \
	resource-editor.h \
	settings.h \
	undo.h \
	wait.h

noinst_HEADERS = \
	authors.h \
	gwyappfilechooser.h \
	gwyappinternal.h \
	gwyddion.h

bin_PROGRAMS = gwyddion
lib_LTLIBRARIES = libgwyapp2.la

EXTRA_DIST = \
	makefile.msc \
	makefile.msc.gwt \
	libgwyapp2.def \
	README.objdata

BUILT_SOURCES = \
	authors.h

# Must init
CLEANFILES =
MAINTAINERCLEANFILES = \
	makefile.msc \
	libgwyapp2.def \
	authors.h

MKENUM_NAME = gwyapptypes
MKENUM_HFILES = data-browser.h menu.h
include $(top_srcdir)/utils/mkenum.mk

AM_CFLAGS = @COMMON_CFLAGS@
AM_CPPFLAGS = -I$(top_builddir)

# In following order do:
# Any code change     C:   R++: A
# Any iface change    C++: 0:   A
# Adding ifaces       C:   R:   A++
# Changing ifaces     C:   R:   0
libgwyapp2_la_LDFLAGS = @GTKGLEXT_LIBS@ @BASIC_LIBS@ -version-info 2:0:2
#libgwyapp2_la_LDFLAGS = @GTKGLEXT_LIBS@ @BASIC_LIBS@ -release @LIBRARY_RELEASE@
libgwyapp2_la_CPPFLAGS = -DG_LOG_DOMAIN=\"GwyApp\" $(AM_CPPFLAGS)

libgwyapp2_la_SOURCES = \
	app.c \
	data-browser.c \
	datachooser.c \
	file.c \
	filelist.c \
	funcuse.c \
	glmaterial-editor.c \
	gradient-editor.c \
	gwyappfilechooser.c \
	gwyapptypes.c \
	gwymoduleutils.c \
	gwymoduleutils-file.c \
	gwyplaintool.c \
	gwytool.c \
	menu.c \
	settings.c \
	resource-editor.c \
	undo.c \
	wait.c

gwyddion_SOURCES = \
	about.c \
	gwyddion.c \
	meta.c \
	remote.c \
	splash.c \
	toolbox.c

gwyddion_LDADD = @GTKGLEXT_LIBS@ @BASIC_LIBS@ $(LIBTIFF) $(LIBXMU) \
	libgwyapp2.la \
	$(libgwymodule) \
	$(libgwydgets) \
	$(libgwydraw) \
	$(libgwyprocess) \
	$(libgwyddion)

# This is ugly a bit, but uses only very basic sed constructs
authors.h: ${top_srcdir}/AUTHORS
	{ \
		echo '/* This is a GENERATED file */'; \
		echo '/*< private_header >*/'; \
		echo 'static const gchar *core_developers[] = {'; \
		sed -n -e '/=== Core developers ===/,/^$$/{' \
			-e 's/^[ =].*//' -e t -e 's/^$$//' -e t \
			-e 's/ *<.*//' -e 's/..*/    "&",/' -e p -e '}' \
			${top_srcdir}/AUTHORS; \
		echo '};'; \
		echo 'static const gchar *developers[] = {'; \
		sed -n -e '/=== Developers ===/,/^$$/{' \
			-e 's/^[ =].*//' -e t -e 's/^$$//' -e t \
			-e 's/ *<.*//' -e 's/.*/    "&",/' -e p -e '}' \
			${top_srcdir}/AUTHORS; \
		echo '};'; \
		echo 'static const gchar *translators[] = {'; \
		sed -n -e '/=== Translators ===/,/^$$/{' \
			-e 's/^[ =].*//' -e t -e 's/^$$//' -e t \
			-e 's/ *<.*//' -e 's/.*/    "&",/' -e p -e '}' \
			${top_srcdir}/AUTHORS; \
		echo '};'; \
	} >authors.h

clean-local:
	rm -f core.* *~
