# XXX: needs working catalog setup, otherwise everything is fetched from
# network!

XSLTPROC = xsltproc --xinclude
DOCBKBASE = http://docbook.sourceforge.net/release/xsl/current
PDFXMLTEX = pdfxmltex
HTML2TXT = lynx -force_html -dump -nolist

PACKAGE = user-guide
DISTNAME = gwyddion-user-guide
MAIN_XML_SOURCE = $(PACKAGE).xml

XML_SOURCES = \
	xml/$(MAIN_XML_SOURCE) \
	xml/basic.xml \
	xml/develop.xml \
	xml/edit.xml \
	xml/fdl.xml \
	xml/filters.xml \
	xml/fractal.xml \
	xml/getting-started.xml \
	xml/grains.xml \
	xml/graph.xml \
	xml/install.xml \
	xml/intro.xml \
	xml/inttrans.xml \
	xml/keyboard-shortcuts.xml \
	xml/leveling.xml \
	xml/managing-files.xml \
	xml/statistical.xml \
	xml/tip.xml

XSL_STYLES = \
	fo.xsl \
	xhtml.xsl

all: xhtml

pdf: $(PACKAGE).pdf

user-guide.pdf: $(XML_SOURCES) fo.xsl
	$(XSLTPROC) -o $(PACKAGE).fo fo.xsl $(MAIN_XML_SOURCE)
	$(PDFXMLTEX) $(PACKAGE).fo

%.txt: %.html
	$(HTML2TXT) $< >$@

xhtml: xhtml.stamp

xhtml.stamp: $(XML_SOURCES) xhtml.xsl
	-rm -rf xhtml
	mkdir xhtml
	{ cd xhtml; \
	  $(XSLTPROC) ../xhtml.xsl ../xml/$(MAIN_XML_SOURCE); \
	  cp ../images/*.png .; \
	}
	touch $@

clean:
	-rm -rf xhtml *.stamp
	-rm -f *.aux *.log *.out $(PACKAGE).fo $(PACKAGE).xml

dist:
	-rm -rf $(DISTNAME)
	mkdir $(DISTNAME) $(DISTNAME)/xml $(DISTNAME)/images
	cp Makefile $(DISTNAME)
	cp $(XSL_STYLES) $(DISTNAME)
	cp images/*.png $(DISTNAME)/images
	cp xml/*.xml $(DISTNAME)/xml
	tar -zcf $(DISTNAME).tar.gz $(DISTNAME)
	rm -rf $(DISTNAME)

xhtml-dist: xhtml
	-rm -rf $(DISTNAME)-xhtml
	cp -a xhtml $(DISTNAME)-xhtml
	tar -zcf $(DISTNAME)-xhtml.tar.gz $(DISTNAME)-xhtml
	-rm -rf $(DISTNAME)-xhtml

.PHONY: all clean dist pdf xhtml
