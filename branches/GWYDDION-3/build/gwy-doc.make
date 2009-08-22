# Generic gtk-doc rules.
# $Id: gtk-doc.mk 8521 2007-09-11 21:15:47Z yeti-dn $

# The directory containing the source code. Relative to $(top_srcdir).
# gtk-doc will search all .c & .h files beneath here for inline comments
# documenting the functions and macros.
DOC_SOURCE_DIR = $(DOC_MODULE)

# The top-level SGML file. You can change this if you want to.
DOC_MAIN_SGML_FILE = $(DOC_MODULE).xml

GWY_DOC_CFLAGS = -I$(top_srcdir) -I$(top_builddir) @LIBGWY_CFLAGS@
GWY_DOC_LIBS = \
	$(top_builddir)/libgwy/libgwy.la \
	@LIBGWY_LIBS@

GTKDOC_CC = $(LIBTOOL) --mode=compile $(CC) $(GWY_DOC_CFLAGS) $(INCLUDES) $(AM_CPPFLAGS) $(CPPFLAGS) $(AM_CFLAGS) $(CFLAGS)
GTKDOC_LD = $(LIBTOOL) --mode=link $(CC) $(GWY_DOC_LIBS) $(AM_CFLAGS) $(CFLAGS) $(AM_LDFLAGS) $(LDFLAGS)
GTKDOC_RUN = $(LIBTOOL) --mode=execute

# We set GPATH here; this gives us semantics for GNU make
# which are more like other make's VPATH, when it comes to
# whether a source that is a target of one rule is then
# searched for in VPATH/GPATH.
#
GPATH = $(srcdir)

TARGET_DIR=$(HTML_DIR)/$(DOC_MODULE)
ADD_OBJECTS = $(top_srcdir)/docs/add-objects.py

EXTRA_DIST = \
	$(content_files) \
	makefile.msc \
	releaseinfo.xml.in \
	$(HTML_IMAGES) \
	$(DOC_MAIN_SGML_FILE) \
	$(DOC_MODULE)-overrides.txt

DOC_STAMPS = scan-build.stamp sgml-build.stamp html-build.stamp \
	sgml.stamp html.stamp

SCAN_FILES = \
	$(DOC_MODULE)-sections.txt \
	$(DOC_MODULE)-decl-list.txt \
	$(DOC_MODULE)-decl.txt \
	$(DOC_MODULE).types

SCANOBJ_FILES = \
	$(DOC_MODULE).args \
	$(DOC_MODULE).hierarchy \
	$(DOC_MODULE).interfaces \
	$(DOC_MODULE).prerequisites \
	$(DOC_MODULE).signals

REPORT_FILES = \
	$(DOC_MODULE)-undocumented.txt \
	$(DOC_MODULE)-undeclared.txt \
	$(DOC_MODULE)-unused.txt

CLEANFILES = $(SCAN_FILES) $(SCANOBJ_FILES) $(REPORT_FILES) $(DOC_STAMPS)

HFILE_GLOB = \
	$(top_srcdir)/$(DOC_SOURCE_DIR)/*.h \
	$(top_builddir)/$(DOC_SOURCE_DIR)/*.h
CFILE_GLOB = $(top_srcdir)/$(DOC_SOURCE_DIR)/*.c

if ENABLE_GTK_DOC
all-local: html-build.stamp
else
all-local:
endif

docs: html-build.stamp

#### scan ####

scan-build.stamp: $(HFILE_GLOB) $(CFILE_GLOB) $(ADD_OBJECTS)
	@echo 'gtk-doc: Scanning header files'
	gtkdoc-scan --module=$(DOC_MODULE) \
	    --source-dir=$(top_srcdir)/$(DOC_SOURCE_DIR) \
	    --source-dir=$(top_builddir)/$(DOC_SOURCE_DIR) \
	    --ignore-headers=$(top_srcdir)/$(DOC_SOURCE_DIR)/$(DOC_MODULE)-aliases.h \
	    --rebuild-sections --rebuild-types \
	    --deprecated-guards="GWY_DISABLE_DEPRECATED" \
	    --ignore-decorators="_GWY_STATIC_INLINE"
	if grep -l '^..*$$' $(DOC_MODULE).types >/dev/null 2>&1 ; then \
		CC="$(GTKDOC_CC)" LD="$(GTKDOC_LD)" RUN="$(GTKDOC_RUN)" \
			gtkdoc-scangobj --module=$(DOC_MODULE) \
			        --output-dir=$(builddir); \
	else \
		rm -f $(SCANOBJ_FILES); \
		touch $(SCANOBJ_FILES); \
	fi
	if test -s $(DOC_MODULE).hierarchy; then \
		$(PYTHON) $(ADD_OBJECTS) $(DOC_MODULE)-sections.txt $(DOC_MODULE).hierarchy $(DOC_MODULE).interfaces $(ADDOBJECTS_OPTIONS); \
	fi
	touch scan-build.stamp

$(DOC_MODULE)-decl.txt $(SCANOBJ_FILES) $(DOC_MODULE)-sections.txt $(DOC_MODULE)-overrides.txt: scan-build.stamp
	@true

#### xml ####

sgml-build.stamp: $(CFILE_GLOB) $(DOC_MODULE)-decl.txt $(SCANOBJ_FILES) $(DOC_MODULE)-sections.txt $(DOC_MODULE)-overrides.txt $(expand_content_files)
	@echo 'gtk-doc: Building XML'
	gtkdoc-mkdb --module=$(DOC_MODULE) \
	    --source-dir=$(top_srcdir)/$(DOC_SOURCE_DIR) \
	    --source-dir=$(top_builddir)/$(DOC_SOURCE_DIR) \
	    --ignore-files=$(top_srcdir)/$(DOC_SOURCE_DIR)/$(DOC_MODULE)-aliases.c \
	    --sgml-mode --output-format=xml \
	    --expand-content-files="$(expand_content_files)" \
	    --main-sgml-file=$(DOC_MAIN_SGML_FILE) \
	    --default-includes=$(DOC_MODULE)/$(DOC_MODULE).h
	touch sgml-build.stamp

sgml.stamp: sgml-build.stamp
	@true

#### html ####

html-build.stamp: sgml.stamp $(srcdir)/$(DOC_MAIN_SGML_FILE) $(content_files)
	@echo 'gtk-doc: Building HTML'
	rm -rf html
	mkdir html
	test -f $(DOC_MAIN_SGML_FILE) || cp -f $(srcdir)/$(DOC_MAIN_SGML_FILE) .
	cd html && gtkdoc-mkhtml --path=$(abs_builddir) $(DOC_MODULE) \
	                 ../$(DOC_MAIN_SGML_FILE)
	test "x$(HTML_IMAGES)" == x || cp -f $(HTML_IMAGES) html/
	cp $(top_srcdir)/docs/style.css html/
	@echo 'gtk-doc: Fixing cross-references'
	gtkdoc-fixxref --module-dir=html --html-dir=$(HTML_DIR) \
	       --module=$(DOC_MODULE) $(FIXXREF_OPTIONS)
	touch html-build.stamp

##############

clean-local:
	rm -rf *~ *.bak .libs $(DOC_MODULE)-scan.* xml
	test -f Makefile.am || rm -f $(DOC_MAIN_SGML_FILE)

maintainer-clean-local:
	rm -rf html

install-data-local:
	d=; \
	if test -s html/index.sgml; then \
		echo 'gtk-doc: Installing HTML from builddir'; \
		d=html; \
	elif test -s $(srcdir)/html/index.sgml; then \
		echo 'gtk-doc: Installing HTML from srcdir'; \
		d=$(srcdir)/html; \
	else \
		echo 'gtk-doc: Nothing to install'; \
	fi; \
	if test -n "$$d"; then \
		$(mkdir_p) $(DESTDIR)$(TARGET_DIR); \
		$(INSTALL_DATA) $$d/* $(DESTDIR)$(TARGET_DIR); \
		$(GTKDOC_REBASE) --relative --dest-dir=$(DESTDIR) \
		         --html-dir=$(DESTDIR)/$(TARGET_DIR); \
	fi; \
	test -n "$$d"

uninstall-local:
	rm -f $(DESTDIR)$(TARGET_DIR)/*
	rmdir $(DESTDIR)$(TARGET_DIR)

#
# Require gtk-doc when making dist
#
if ENABLE_GTK_DOC
dist-check-gtkdoc:
else
dist-check-gtkdoc:
	@echo "*** gtk-doc must be installed and enabled in order to make dist"
	@false
endif

dist-hook: dist-check-gtkdoc
	mkdir $(distdir)/html
	if test -s html/index.sgml; then d=html; else d=$(srcdir)/html; fi; \
	$(GTKDOC_REBASE) --online --relative --html-dir=$(distdir)/html
	cp -f $$d/* $(distdir)/html

.PHONY: docs

# vim: set ft=make noet :
