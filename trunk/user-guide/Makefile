# XXX: needs working catalog setup, otherwise everything is fetched from
# network!
export

XSLTPROC = xsltproc --xinclude
DOCBKBASE = http://docbook.sourceforge.net/release/xsl/current
PDFXMLTEX = pdfxmltex
HTML2TXT = lynx -force_html -dump -nolist

PACKAGE = user-guide
VERSION = 2005-07-26
BASENAME = gwyddion-user-guide
DISTNAME = $(BASENAME)-$(VERSION)
DISTNAMEXHTML = $(BASENAME)-xhtml-$(VERSION)
MAIN_XML_SOURCE = xml/$(PACKAGE).xml
XML_SOURCES = $(wildcard xml/*.xml)
FORMULA_SOURCES = $(wildcard formulas/*.tex)
SOURCES = $(XML_SOURCES) $(FORMULA_SOURCES)

XSL_STYLES = fo.xsl xhtml.xsl
CSS_STYLES = user-guide.css

all: xhtml

pdf: $(PACKAGE).pdf

user-guide.pdf: $(MAIN_XML_SOURCE) $(XML_SOURCES) fo.xsl
	$(XSLTPROC) -o $(PACKAGE).fo fo.xsl $(MAIN_XML_SOURCE)
	$(PDFXMLTEX) $(PACKAGE).fo

%.txt: %.html
	$(HTML2TXT) $< >$@

xhtml: formulas xhtml.stamp

xhtml.stamp: $(XML_SOURCES) xhtml.xsl
	test -d xhtml || mkdir xhtml
	$(XSLTPROC) xhtml.xsl $(MAIN_XML_SOURCE)
	cp images/*.png xhtml
	cp user-guide.css xhtml
	touch $@

formulas: formulas.stamp

formulas.stamp: $(FORMULA_SOURCES)
	cd ./formulas && make
	test -d xhtml || mkdir xhtml
	cp formulas/eq-*.png xhtml
	touch $@

clean:
	cd ./formulas && make clean
	-rm -rf xhtml *.stamp xml/*~ *~
	-rm -f *.aux *.log *.out $(PACKAGE).fo $(PACKAGE).xml

dist:
	-rm -rf $(DISTNAME)
	mkdir $(DISTNAME) $(DISTNAME)/xml $(DISTNAME)/images
	cp Makefile $(DISTNAME)
	cp $(XSL_STYLES) $(CSS_STYLES) $(DISTNAME)
	cp images/*.png $(DISTNAME)/images
	cp xml/*.xml $(DISTNAME)/xml
	cd ./formulas && make dist
	tar -zcf $(DISTNAME).tar.gz $(DISTNAME)
	rm -rf $(DISTNAME)

dist-xhtml: xhtml
	-rm -rf $(DISTNAMEXHTML)
	cp -a xhtml $(DISTNAMEXHTML)
	tar -zcf $(DISTNAMEXHTML).tar.gz $(DISTNAMEXHTML)
	-rm -rf $(DISTNAMEXHTML)

version:
	d=`date --iso`; \
	sed -i 's/\(VERSION = \)[0-9]\{4\}-[0-9]\{2\}-[0-9]\{2\}/\1'$$d/ Makefile

help:
	@echo "xhtml        Compile User guide to XHTML"
	@echo "all          The same"
	@echo "dist         Create tarball with source code"
	@echo "dist-xhtml   Create tarball with ready-to-read XHTML User guide"
	@echo "version      Update version number to current date"

.PHONY: all clean dist pdf xhtml dist-xhtml help version
