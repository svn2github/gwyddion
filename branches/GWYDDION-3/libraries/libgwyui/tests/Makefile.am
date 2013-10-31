# $Id$

noinst_PROGRAMS = \
	graph \
	input-widgets \
	raster-view

graph_SOURCES         = graph.c
input_widgets_SOURCES = input-widgets.c
raster_view_SOURCES   = raster-view.c

AM_CFLAGS = $(LIBGWYUI_DEPS_CFLAGS) $(VALGRIND_CFLAGS) $(GWY_CFLAGS)
AM_CPPFLAGS = \
	-I$(top_builddir) \
	-I$(top_builddir)/libraries \
	-I$(top_srcdir)/libraries \
	$(LIBGWYUI_COMPAT_CPPFLAGS)
AM_LDFLAGS = \
	../libgwyui4.la \
	../../libgwy/libgwy4.la \
	$(HOST_LDFLAGS) \
	$(LIBGWYUI_DEPS_LIBS)

clean-local:
	rm -rf core.* *~ *.gcda *.gcno *.gcov gmon.out
