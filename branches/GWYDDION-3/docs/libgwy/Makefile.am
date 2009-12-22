# Process this file with automake to produce Makefile.in
# $Id$

# The name of the module, e.g. 'glib'.
DOC_MODULE = libgwy

# Source files to ignore, put both headers and code here.  Also directories.
IGNORE_SRC = libgwy.h tests

# Extra options to pass to add-objects.py
#ADDOBJECTS_OPTIONS = --standard-files=gwyddiontypes
ADDOBJECTS_OPTIONS = --standard-files=types

# Extra options to supply to gtkdoc-fixref. Not normally needed.
# e.g. FIXXREF_OPTIONS=--extra-dir=../gdk-pixbuf/html --extra-dir=../gdk/html
FIXXREF_OPTIONS =

# Images to copy into HTML directory.
# e.g. HTML_IMAGES=$(top_srcdir)/gtk/stock-icons/stock_about_24.png
HTML_IMAGES = \
	$(srcdir)/memory-access.png

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
EXTRA_DIST +=
