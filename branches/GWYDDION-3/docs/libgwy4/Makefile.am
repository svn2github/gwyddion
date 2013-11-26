# Process this file with automake to produce Makefile.in
# $Id$

# The directory containing the source code. Relative to $(top_srcdir).
# gtk-doc will search all .c & .h files beneath here for inline comments
# documenting the functions and macros.
DOC_SOURCE_DIR = libgwy

# The name of the module, e.g. 'glib'.
DOC_MODULE = libgwy4

# Source files to ignore, put both headers and code here.  Also directories.
IGNORE_SRC = libgwy.h tests benchmarks

# Extra options to pass to add-objects.py
#ADDOBJECTS_OPTIONS = --standard-files=gwyddiontypes
ADDOBJECTS_OPTIONS = --standard-files=types

# Extra options to supply to gtkdoc-fixref. Not normally needed.
# e.g. FIXXREF_OPTIONS=--extra-dir=../gdk-pixbuf/html --extra-dir=../gdk/html
FIXXREF_OPTIONS = --extra-dir=../libgwyui4/html --extra-dir=../libgwyapp4/html

# Images to copy into HTML directory.
# e.g. HTML_IMAGES=$(top_srcdir)/gtk/stock-icons/stock_about_24.png
HTML_IMAGES = \
	$(srcdir)/images/field-copy-congruent.png \
	$(srcdir)/images/memory-access.png

# Extra SGML files that are included by $(DOC_MAIN_SGML_FILE).
# e.g. content_files=running.sgml building.sgml changes-2.0.sgml
content_files = builtin-fit-func.xml builtin-grain-value.xml

# SGML files where gtk-doc abbrevations (#GtkWidget) are expanded
# These files must be listed here *and* in content_files
# e.g. expand_content_files=running.sgml
expand_content_files=

# This includes the standard gtk-doc make rules.
include $(top_srcdir)/build/gwy-doc.make

# Other files to distribute
# e.g. EXTRA_DIST += version.xml.in
EXTRA_DIST += \
	$(srcdir)/images/field-copy-congruent.svg

# Specialised introspection programs
AM_CPPFLAGS = -I$(top_builddir)/libraries -I$(top_srcdir)/libraries
AM_CFLAGS =  $(LIBGWY_DEPS_CFLAGS) $(HOST_CFLAGS)
AM_LDFLAGS = \
	$(LIBGWY_DEPS_LIBS) \
	$(HOST_LDFLAGS) \
	$(top_builddir)/libraries/libgwy/libgwy4.la
CLEANFILES += builtin-fit-func.xml builtin-grain-value.xml

noinst_PROGRAMS = fit-funcs grain-values
fit_funcs_SOURCES = fit-funcs.c
fit_funcs_CFLAGS = $(AM_CFLAGS) $(GWY_CFLAGS)
grain_values_SOURCES = grain-values.c
grain_values_CFLAGS = $(AM_CFLAGS) $(GWY_CFLAGS)

builtin-fit-func.xml: fit-funcs$(EXEEXT)
	$(AM_V_GEN)$(LIBTOOL) --mode=execute ./fit-funcs$(EXEEXT) >builtin-fit-func.xml

builtin-grain-value.xml: grain-values$(EXEEXT)
	$(AM_V_GEN)$(LIBTOOL) --mode=execute ./grain-values$(EXEEXT) >builtin-grain-value.xml

# Generate images to srcdir because they are distributed and only the
# maintainer runs this rule.  Keep the main rule outside the conditional to get
# a better error message without inkscape.
images: \
	$(srcdir)/images/field-copy-congruent.png

if HAVE_INKSCAPE
$(srcdir)/images/field-copy-congruent.png: $(srcdir)/images/field-copy-congruent.svg
	$(AM_V_GEN)$(INKSCAPE) -z -D -d 90 -b white -e "$@" -f "$<" >/dev/null
endif

.PHONY: images
