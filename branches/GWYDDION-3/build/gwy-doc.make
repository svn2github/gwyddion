# Generic gtk-doc rules.
# $Id$

# The directory containing the source code. Relative to $(top_srcdir).
# gtk-doc will search all .c & .h files beneath here for inline comments
# documenting the functions and macros.
DOC_SOURCE_DIR = $(DOC_MODULE)

# The top-level SGML file. You can change this if you want to.
DOC_MAIN_SGML_FILE = $(DOC_MODULE).xml

GWY_DOC_CFLAGS = \
	-I$(top_srcdir) \
	-I$(top_builddir) \
	$(LIBGWY_DEPS_CFLAGS)
GWY_DOC_LIBS = \
	$(top_builddir)/libgwyui/libgwyui3.la \
	$(top_builddir)/libgwy/libgwy3.la \
	$(LIBGWY_DEPS_LIBS)

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
ADD_OBJECTS = $(top_srcdir)/build/gtkdoc-add-objects.py

EXTRA_DIST = \
	$(content_files) \
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

# XXX: The awk command modifying $(DOC_MODULE).interfaces fixes gtk-doc 1.17
# which thinks everything listed there is an object and does various silly
# things based on that.  Remove once not necessary.
scan-build.stamp: $(HFILE_GLOB) $(CFILE_GLOB) $(ADD_OBJECTS)
	$(AM_V_GEN)gtkdoc-scan --module=$(DOC_MODULE) \
	    --source-dir=$(top_srcdir)/$(DOC_SOURCE_DIR) \
	    --source-dir=$(top_builddir)/$(DOC_SOURCE_DIR) \
	    --ignore-headers="$(IGNORE_SRC)" \
	    --rebuild-sections --rebuild-types \
	    --deprecated-guards="GWY_DISABLE_DEPRECATED" \
	    --ignore-decorators="_GWY_STATIC_INLINE"
	$(AM_V_at)if grep -l '^..*$$' $(DOC_MODULE).types >/dev/null 2>&1 ; then \
		CC="$(GTKDOC_CC)" LD="$(GTKDOC_LD)" RUN="$(GTKDOC_RUN)" \
			gtkdoc-scangobj --module=$(DOC_MODULE) \
			        --output-dir=$(builddir); \
	else \
		rm -f $(SCANOBJ_FILES); \
		touch $(SCANOBJ_FILES); \
	fi
	$(AM_V_at)if test -s $(DOC_MODULE).hierarchy; then \
		$(PYTHON) $(ADD_OBJECTS) $(DOC_MODULE)-sections.txt $(DOC_MODULE).hierarchy $(DOC_MODULE).interfaces $(ADDOBJECTS_OPTIONS); \
		$(AWK) '/^[^ ]/{ignore=0}; /^(GFlags|GEnum|GBoxed)/{ignore=1}; {if(!ignore)print}' $(DOC_MODULE).hierarchy >$(DOC_MODULE).tmp && mv -f $(DOC_MODULE).tmp $(DOC_MODULE).hierarchy; \
	fi
	$(AM_V_at)touch scan-build.stamp

$(DOC_MODULE)-decl.txt $(SCANOBJ_FILES) $(DOC_MODULE)-sections.txt $(DOC_MODULE)-overrides.txt: scan-build.stamp
	@true

#### xml ####

sgml-build.stamp: $(CFILE_GLOB) $(DOC_MODULE)-decl.txt $(SCANOBJ_FILES) $(DOC_MODULE)-sections.txt $(DOC_MODULE)-overrides.txt $(expand_content_files)
	$(AM_V_GEN)gtkdoc-mkdb --module=$(DOC_MODULE) \
	    --source-dir=$(top_srcdir)/$(DOC_SOURCE_DIR) \
	    --source-dir=$(top_builddir)/$(DOC_SOURCE_DIR) \
	    --ignore-files="$(IGNORE_SRC)" \
	    --sgml-mode --output-format=xml \
	    --expand-content-files="$(expand_content_files)" \
	    --main-sgml-file=$(DOC_MAIN_SGML_FILE) \
	    --default-includes=$(DOC_MODULE)/$(DOC_MODULE).h
	$(AM_V_at)touch sgml-build.stamp

sgml.stamp: sgml-build.stamp
	@true

#### html ####

html-build.stamp: sgml.stamp $(srcdir)/$(DOC_MAIN_SGML_FILE) $(content_files)
	$(AM_V_GEN)rm -rf html
	$(AM_V_at)mkdir html
	$(AM_V_at)test -f $(DOC_MAIN_SGML_FILE) || cp -f $(srcdir)/$(DOC_MAIN_SGML_FILE) .
	$(AM_V_at)cd html && gtkdoc-mkhtml --path=$(abs_builddir) $(DOC_MODULE) \
	                 ../$(DOC_MAIN_SGML_FILE)
	$(AM_V_at)test "x$(HTML_IMAGES)" == x || cp -f $(HTML_IMAGES) html/
	$(AM_V_at)cp $(top_srcdir)/docs/style.css html/
	$(AM_V_at)gtkdoc-fixxref --module-dir=html --html-dir=$(HTML_DIR) \
	       --module=$(DOC_MODULE) $(FIXXREF_OPTIONS)
	$(AM_V_at)touch html-build.stamp

##############

clean-local:
	rm -rf *~ *.bak .libs $(DOC_MODULE)-scan.* xml
	test -f Makefile.am || rm -f $(DOC_MAIN_SGML_FILE)

# VPATH fix
distclean-local:
	-test . = "$(srcdir)" || rm -rf html $(DOC_MODULE)-overrides.txt

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
		gtkdoc-rebase --relative --dest-dir=$(DESTDIR) \
		         --html-dir=$(DESTDIR)/$(TARGET_DIR); \
	fi; \
	test -n "$$d"

uninstall-local:
	rm -rf $(DESTDIR)$(TARGET_DIR)

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
	cp -f $$d/* $(distdir)/html
	gtkdoc-rebase --online --relative --html-dir=$(distdir)/html

.PHONY: docs

# vim: set ft=make noet :
