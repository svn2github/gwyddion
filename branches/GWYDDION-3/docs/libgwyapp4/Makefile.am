# Process this file with automake to produce Makefile.in
# $Id$

# The directory containing the source code. Relative to $(top_srcdir).
# gtk-doc will search all .c & .h files beneath here for inline comments
# documenting the functions and macros.
DOC_SOURCE_DIR = libgwyapp

# The name of the module, e.g. 'glib'.
DOC_MODULE = libgwyapp4

# Source files to ignore, put both headers and code here.  Also directories.
IGNORE_SRC = libgwyapp.h tests benchmarks

# Extra options to pass to add-objects.py
#ADDOBJECTS_OPTIONS = --standard-files=gwyddiontypes
ADDOBJECTS_OPTIONS = --standard-files=types

# Extra options to supply to gtkdoc-fixref. Not normally needed.
# e.g. FIXXREF_OPTIONS=--extra-dir=../gdk-pixbuf/html --extra-dir=../gdk/html
FIXXREF_OPTIONS = --extra-dir=../libgwy4/html --extra-dir=../libgwyui4/html

# Images to copy into HTML directory.
# e.g. HTML_IMAGES=$(top_srcdir)/gtk/stock-icons/stock_about_24.png
HTML_IMAGES =

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
#EXTRA_DIST +=

# Specialised introspection programs
AM_CPPFLAGS = -I$(top_builddir)/libraries -I$(top_srcdir)/libraries
AM_CFLAGS =  $(LIBGWYAPP_DEPS_CFLAGS) $(HOST_CFLAGS)
AM_LDFLAGS = \
	$(LIBGWYAPP_DEPS_LIBS) \
	$(HOST_LDFLAGS) \
	$(top_builddir)/libraries/libgwy/libgwy4.la \
	$(top_builddir)/libraries/libgwyui/libgwyui4.la \
	$(top_builddir)/libraries/libgwyapp/libgwyapp4.la

