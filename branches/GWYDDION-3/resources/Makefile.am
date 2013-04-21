# Process this file with automake to produce Makefile.in
# $Id$

SUBDIRS = gradients

uifiles = gwy.css
uixml = ui.gresource.xml
uicode = ui-gresource.c ui-gresource.h

iconsxml = icons.gresource.xml
iconscode = icons-gresource.c icons-gresource.h

noinst_DATA = $(uicode) $(iconscode)

EXTRA_DIST = $(uifiles) $(STOCK_ICONS) $(STOCK_ICON_XCFSOURCES) $(icondeps)
CLEANFILES = $(uicode) $(uixml) $(iconscode) $(iconsxml)

gcres = GDK_PIXBUF_PIXDATA=$(GDK_PIXBUF_PIXDATA) $(GLIB_COMPILE_RESOURCES) --generate --manual-register
fixcgres = $(top_srcdir)/build/fix-gresource-c.sed
fixhgres = $(top_srcdir)/build/fix-gresource-h.sed

ui.gresource.xml: Makefile.am
	$(AM_V_GEN)$(top_srcdir)/build/generate-gresource-xml.sh /net/gwyddion/ui \
	    $(uifiles) >ui.gresource.xml

ui-gresource.c: ui.gresource.xml $(fixcgres) $(uifiles)
	$(AM_V_GEN)$(gcres) --c-name=_gwy_ui --target=ui-gresource.c \
	    --sourcedir=$(srcdir) $(uixml)
	$(AM_V_at)$(SED) -f $(fixcgres) -i ui-gresource.c

ui-gresource.h: ui.gresource.xml $(fixhgres) $(uifiles)
	$(AM_V_GEN)$(gcres) --c-name=_gwy_ui --target=ui-gresource.h \
	    --sourcedir=$(srcdir) $(uixml)
	$(AM_V_at)$(SED) -f $(fixhgres) -i ui-gresource.h

icons.gresource.xml: Makefile.am
	$(AM_V_GEN)$(top_srcdir)/build/generate-gresource-xml.sh -p /net/gwyddion \
	    $(STOCK_ICONS) >icons.gresource.xml

icons-gresource.c: icons.gresource.xml $(fixcgres) $(STOCK_ICONS)
	$(AM_V_GEN)$(gcres) --c-name=_gwy_icons --target=icons-gresource.c \
	    --sourcedir=$(srcdir) $(iconsxml)
	$(AM_V_at)$(SED) -f $(fixcgres) -i icons-gresource.c

icons-gresource.h: icons.gresource.xml $(fixhgres) $(STOCK_ICONS)
	$(AM_V_GEN)$(gcres) --c-name=_gwy_icons --target=icons-gresource.h \
	    --sourcedir=$(srcdir) $(iconsxml)
	$(AM_V_at)$(SED) -f $(fixhgres) -i icons-gresource.h

STOCK_ICONS_16x21 = \
	pixmaps/16x21/color-axis.png \
	pixmaps/16x21/color-axis.p.png \
	pixmaps/16x21/gradients.png \
	pixmaps/16x21/gradients.p.png \
	pixmaps/16x21/axis-range.png \
	pixmaps/16x21/axis-range.p.png

STOCK_ICON_XCFSOURCES = \
	pixmaps/16x21/axis-buttons.xcf

STOCK_ICONS = $(STOCK_ICONS_16x21)
icondeps = pixmaps/16x21/pixmaps.deps

# This rule re-exports all PNG icons from their XCF sources.  Only to be done
# manually because we do not want GIMP as a regular build tool.  Hence we put
# the output to *srcdir*.  Keep this main rule out of the conditional to get
# a better error message without gimp-console.
icons: icons-16x21

if HAVE_GIMP_CONSOLE
icons-16x21:
	@rm -f pixmaps/16x21/*.png
	GIMP_CONSOLE=$(GIMP_CONSOLE) srcdir=$(srcdir)/pixmaps/16x21 destdir=$(srcdir)/pixmaps/16x21 $(top_srcdir)/build/export-xcf-layers.sh $(srcdir)/pixmaps/16x21/pixmaps.deps
endif

.PHONY: icons icons-16x21
