# @(#) $Id$

libgwyddion = $(top_builddir)/libgwyddion/libgwyddion.la
libgwymodule = $(top_builddir)/libgwymodule/libgwymodule.la
libgwyprocess = $(top_builddir)/libprocess/libgwyprocess.la
libgwydraw = $(top_builddir)/libdraw/libgwydraw.la
libgwydgets = $(top_builddir)/libgwydgets/libgwydgets.la
libgwyapp = $(top_builddir)/app/libgwyapp.la
moduledir = $(top_srcdir)/modules

GUIDE_MAP = user-guide-modules

EXTRA_DIST = \
	modules-web.xsl \
	update-msvc.py \
	update-potfiles.sh \
	$(GUIDE_MAP) \
	vim-syn-gen.py

module-list: modules.xml module-list.html module-list-nocss.html

noinst_PROGRAMS = \
	dump-modules

dump_modules_SOURCES = \
	dump-modules.c

dump_modules_LDADD = @GTKGLEXT_LIBS@ \
	$(libgwyapp) \
	$(libgwymodule) \
	$(libgwydgets) \
	$(libgwydraw) \
	$(libgwyprocess) \
	$(libgwyddion)

CLEANFILES = \
	modules.xml \
	module-list.html \
	module-list-nocss.html

INCLUDES = -I$(top_srcdir)

modules.xml: dump-modules $(GUIDE_MAP) $(moduledir)/*.c $(moduledir)/*/*.c
	./dump-modules $(GUIDE_MAP) >modules.xml

module-list.html: modules.xml modules-web.xsl
	$(XSLTPROC) modules-web.xsl modules.xml >module-list.html

module-list-nocss.html: modules.xml modules-web-nocss.xsl
	$(XSLTPROC) modules-web-nocss.xsl modules.xml >module-list-nocss.html

clean-local:
	rm -f core.* *~
