# @(#) $Id$

graphdir = $(gwymoduledir)/graph

module_ldflags = -shared -Wl,-soname

graph_PROGRAMS = \
	read.so \
	graph_export_ascii.so \
	graph_zoom.so \
	graph_unzoom.so \
        graph_points.so \
	graph_fit.so
 

EXTRA_DIST = \
	makefile.msc

read_so_SOURCES = read.c
read_so_LDFLAGS = $(module_ldflags),read.so

graph_export_ascii_so_SOURCES = graph_export_ascii.c
graph_export_ascii_so_LDFLAGS = $(module_ldflags),graph_export_ascii.so

graph_zoom_so_SOURCES = graph_zoom.c
graph_zoom_so_LDFLAGS = $(module_ldflags),graph_zoom.so

graph_unzoom_so_SOURCES = graph_unzoom.c
graph_unzoom_so_LDFLAGS = $(module_ldflags),graph_unzoom.so

graph_points_so_SOURCES = graph_points.c
graph_points_so_LDFLAGS = $(module_ldflags),graph_points.so

graph_fit_so_SOURCES = graph_fit.c
graph_fit_so_LDFLAGS = $(module_ldflags),graph_fit.so

INCLUDES = -I$(top_srcdir)
AM_CPPFLAGS = -DG_LOG_DOMAIN=\"Module\"

clean-local:
	rm -f core.* *~
