# @(#) $Id$

libgwyddion = $(top_builddir)/libgwyddion/libgwyddion.la
libgwymodule = $(top_builddir)/libgwymodule/libgwymodule.la
libgwyprocess = $(top_builddir)/libprocess/libgwyprocess.la
libgwydraw = $(top_builddir)/libdraw/libgwydraw.la
libgwydgets = $(top_builddir)/libgwydgets/libgwydgets.la

libgwyappincludedir = $(pkgincludedir)/app

libgwyappinclude_HEADERS = \
	app.h \
	data-browser.h \
	file.h \
	filelist.h \
	funcuse.h \
	glmaterial-editor.h \
	gradient-editor.h \
	gwyapp.h \
	menu.h \
	menu-windowlist.h \
	resource-editor.h \
	settings.h \
	undo.h \
	unitool.h \
	wait.h

noinst_HEADERS = \
	authors.h \
	gwyappinternal.h \
	gwyddion.h

bin_PROGRAMS = gwyddion
lib_LTLIBRARIES = libgwyapp.la

EXTRA_DIST = \
	makefile.msc \
	makefile.msc.gwt \
	libgwyapp.def \
	README.objdata

BUILT_SOURCES = \
	authors.h

MAINTAINERCLEANFILES = authors.h

# In following order do:
# Any code change     C:   R++: A
# Any iface change    C++: 0:   A
# Adding ifaces       C:   R:   A++
# Changing ifaces     C:   R:   0
#libgwyapp_la_LDFLAGS = @GTKGLEXT_LIBS@ @BASIC_LIBS@ -version-info 6:2:3
libgwyapp_la_LDFLAGS = @GTKGLEXT_LIBS@ @BASIC_LIBS@ -release @LIBRARY_RELEASE@
libgwyapp_la_CFLAGS = -DG_LOG_DOMAIN=\"GwyApp\" $(AM_CPPFLAGS)
libgwyapp_la_SOURCES = \
	app.c \
	data-browser.c \
	file.c \
	filelist.c \
	funcuse.c \
	glmaterial-editor.c \
	gradient-editor.c \
	menu.c \
	menu-windowlist.c \
	settings.c \
	resource-editor.c \
	unitool.c \
	undo.c \
	wait.c

gwyddion_SOURCES = \
	about.c \
	gwyddion.c \
	init.c \
	meta.c \
	splash.c \
	toolbox.c

gwyddion_LDADD = @GTKGLEXT_LIBS@ @BASIC_LIBS@ $(LIBTIFF) \
	libgwyapp.la \
	$(libgwymodule) \
	$(libgwydgets) \
	$(libgwydraw) \
	$(libgwyprocess) \
	$(libgwyddion)

# This is ugly a bit, but uses only very basic sed constructs
authors.h: ${top_srcdir}/AUTHORS
	{ \
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

INCLUDES = -I$(top_srcdir)

clean-local:
	rm -f core.* *~
