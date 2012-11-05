# Process this file with automake to produce Makefile.in
# $Id$

SUBDIRS = gradients

uifiles = gwy.css
uixml = ui.gresource.xml
uicode = ui.c ui.h

noinst_DATA = $(uicode)

EXTRA_DIST = $(uifiles)
CLEANFILES = $(uicode) $(uixml)

gcres = $(GLIB_COMPILE_RESOURCES) --generate --manual-register
fixcgres = $(top_srcdir)/build/fix-gresource-c.sed
fixhgres = $(top_srcdir)/build/fix-gresource-h.sed

ui.gresource.xml: Makefile.am
	$(AM_V_GEN)$(top_srcdir)/build/generate-gresource-xml.sh /net/gwyddion/ui \
	    $(uifiles) >ui.gresource.xml

ui.c: ui.gresource.xml $(fixcgres) $(uifiles)
	$(AM_V_GEN)$(gcres) --c-name=_gwy_ui --target=ui.c \
	    --sourcedir=$(srcdir) $(uixml)
	$(AM_V_at)$(SED) -f $(fixcgres) -i ui.c

ui.h: ui.gresource.xml $(fixhgres) $(uifiles)
	$(AM_V_GEN)$(gcres) --c-name=_gwy_ui --target=ui.h \
	    --sourcedir=$(srcdir) $(uixml)
	$(AM_V_at)$(SED) -f $(fixhgres) -i ui.h

