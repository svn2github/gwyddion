# @(#) $Id$

libgwyddion = $(top_builddir)/libgwyddion/libgwyddion2.la
libgwymodule = $(top_builddir)/libgwymodule/libgwymodule2.la
libgwyprocess = $(top_builddir)/libprocess/libgwyprocess2.la
libgwydraw = $(top_builddir)/libdraw/libgwydraw2.la
libgwydgets = $(top_builddir)/libgwydgets/libgwydgets2.la
libgwyapp = $(top_builddir)/app/libgwyapp2.la
moduledir = $(top_srcdir)/modules

GUIDE_MAP = user-guide-modules

EXTRA_DIST = \
	extract-file-magic.py \
	genmarshal.mk \
	mkenum.c.template \
	mkenum.h.template \
	mkenum.mk \
	modules-web.xsl \
	update-langs.py \
	update-potfiles.sh \
	$(GUIDE_MAP) \
	vim-syn-gen.py \
	gwyddion.nsi.template \
	generate-gwyddion-nsi.py \
	update-api-indices.py \
	bundler.in \
	mkosxlauncher.in

module-list: modules.xml module-list.html module-list-nocss.html

noinst_PROGRAMS = \
	dump-modules

dump_modules_SOURCES = \
	dump-modules.c

AM_CPPFLAGS = -I$(top_srcdir)
AM_CFLAGS = @COMMON_CFLAGS@

dump_modules_LDADD = @GTKGLEXT_LIBS@ @BASIC_LIBS@ \
	$(libgwyapp) \
	$(libgwymodule) \
	$(libgwydgets) \
	$(libgwydraw) \
	$(libgwyprocess) \
	$(libgwyddion)

CLEANFILES = \
	$(GUIDE_MAP).tmp \
	modules.xml \
	module-list.html \
	module-list-nocss.html

DISTCLEANFILES = \
	gwywin32.bat

modules.xml: dump-modules $(GUIDE_MAP) $(moduledir)/*.c $(moduledir)/*/*.c
	@sed -e '/^#/d' \
	  -e 's/^\([^\t]\+\)\t\([^\t]\+\)$$/\1 \2.html/' \
	  -e 's/^\([^\t]\+\)\t\([^\t]\+\)\t\([^\t]\+\)$$/\1 \2.html#\3/' \
	  $(GUIDE_MAP) >$(GUIDE_MAP).tmp
	./dump-modules $(GUIDE_MAP).tmp >modules.xml
	rm -f $(GUIDE_MAP).tmp

module-list.html: modules.xml modules-web.xsl
	$(XSLTPROC) -o module-list.html modules-web.xsl modules.xml

module-list-nocss.html: modules.xml modules-web-nocss.xsl
	$(XSLTPROC) -o module-list-nocss.html modules-web-nocss.xsl modules.xml

clean-local:
	rm -f core.* *~
