# Process this file with automake to produce Makefile.in
# $Id$

# The name of the module, e.g. 'glib'.
DOC_MODULE = libgwyui

# Source files to ignore, put both headers and code here.  Also directories.
IGNORE_SRC = libgwyui.h tests benchmarks

# Extra options to pass to add-objects.py
#ADDOBJECTS_OPTIONS = --standard-files=gwyddiontypes
ADDOBJECTS_OPTIONS = --standard-files=types

# Extra options to supply to gtkdoc-fixref. Not normally needed.
# e.g. FIXXREF_OPTIONS=--extra-dir=../gdk-pixbuf/html --extra-dir=../gdk/html
FIXXREF_OPTIONS = --extra-dir=../libgwy/html

# Images to copy into HTML directory.
# e.g. HTML_IMAGES=$(top_srcdir)/gtk/stock-icons/stock_about_24.png
HTML_IMAGES = \
	$(srcdir)/images/GwyAdjustBar.png \
	$(srcdir)/images/GwyColorAxis.png \
	$(srcdir)/images/GwyRasterArea.png \
	$(srcdir)/images/GwyRuler.png \
	$(srcdir)/images/GwySpinButton.png

# Extra SGML files that are included by $(DOC_MAIN_SGML_FILE).
# e.g. content_files=running.sgml building.sgml changes-2.0.sgml
content_files =

# SGML files where gtk-doc abbrevations (#GtkWidget) are expanded
# These files must be listed here *and* in content_files
# e.g. expand_content_files=running.sgml
expand_content_files=

# This includes the standard gtk-doc make rules.
include $(top_srcdir)/build/gwy-doc.make

# Other files to distribute
# e.g. EXTRA_DIST += version.xml.in
EXTRA_DIST += README.images

# Specialised introspection programs
AM_CPPFLAGS = -I$(top_builddir) -I$(top_srcdir)
AM_CFLAGS =  $(LIBGWYUI_DEPS_CFLAGS) $(CAIRO_XLIB_CFLAGS) $(HOST_CFLAGS)
AM_LDFLAGS = $(LIBGWYUI_DEPS_LIBS) $(CAIRO_XLIB_LIBS) $(HOST_LDFLAGS) $(top_builddir)/libgwy/libgwy3.la  $(top_builddir)/libgwyui/libgwyui3.la

if HAVE_CAIRO_XLIB
render_widgets = render-widgets
render_widgets_SOURCES = render-widgets.c
render_widgets_CFLAGS = $(AM_CFLAGS) $(GWY_CFLAGS)
else
EXTRA_DIST += render-widgets.c
endif

noinst_PROGRAMS = $(render_widgets)
