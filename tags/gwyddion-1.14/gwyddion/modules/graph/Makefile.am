# @(#) $Id$

graphdir = $(pkglibdir)/modules/graph

graph_LTLIBRARIES = \
	read.la \
	graph_export_ascii.la \
	graph_zoom.la \
	graph_unzoom.la \
	graph_points.la \
	graph_fit.la \
	graph_cd.la
 
EXTRA_DIST = \
	makefile.msc



INCLUDES = -I$(top_srcdir)

AM_CPPFLAGS = -DG_LOG_DOMAIN=\"Module\"


if OS_WIN32
no_undefined = -no-undefined
endif

AM_LDFLAGS = -avoid-version -module $(no_undefined)

if MODULE_DEPENDENCIES
module_libadd = \
	$(top_builddir)/app/libgwyapp.la		\
	$(top_builddir)/libdraw/libgwydraw.la		\
	$(top_builddir)/libgwyddion/libgwyddion.la	\
	$(top_builddir)/libgwydgets/libgwydgets.la	\
	$(top_builddir)/libgwymodule/libgwymodule.la	\
	$(top_builddir)/libprocess/libgwyprocess.la	\
	@GTK_LIBS@
endif
read_la_SOURCES = read.c
read_la_LIBADD = $(module_libadd)

graph_export_ascii_la_SOURCES = graph_export_ascii.c
graph_export_ascii_la_LIBADD = $(module_libadd)

graph_zoom_la_SOURCES = graph_zoom.c
graph_zoom_la_LIBADD = $(module_libadd)

graph_unzoom_la_SOURCES = graph_unzoom.c
graph_unzoom_la_LIBADD = $(module_libadd)

graph_points_la_SOURCES = graph_points.c
graph_points_la_LIBADD = $(module_libadd)

graph_fit_la_SOURCES = graph_fit.c
graph_fit_la_LIBADD = $(module_libadd)

graph_cd_la_SOURCES = graph_cd.c
graph_cd_la_LIBADD = $(module_libadd)


clean-local:
	rm -f core.* *~
