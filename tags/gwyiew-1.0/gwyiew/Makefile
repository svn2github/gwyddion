#!/usr/bin/make

include pkg.mak

# PREFIX is only used to search for the gwyddion.pc file.
# Keep it as it is if you have Gwyddion in a standard location.
# For non-standard locations run
#
#   make PREFIX=/foo/bar/usr install
#
PREFIX = /usr/local
PKGCONFIG = PKG_CONFIG_PATH=\${PKG_CONFIG_PATH}:$(PREFIX)/lib/pkgconfig pkg-config
GWY = gwyddion
GTK = gtk+-2.0

GWY_CFLAGS = $(shell $(PKGCONFIG) $(GWY) $(GTK) --cflags)
GWY_LDFLAGS = $(shell $(PKGCONFIG) $(GWY) $(GTK) --libs)
MODULE_DIR = $(shell $(PKGCONFIG) $(GWY) --variable=gwymoduledir)
EXTRA_CFLAGS = -Wall -O2 -march=`uname -m` -DVERSION=\"$(VERSION)\" -DPACKAGE=\"$(PACKAGE)\" -DGWY_MODULE_DIR=\"$(MODULE_DIR)\"
OBJECTS = $(SOURCES:.c=.o)
LDFLAGS = $(GWY_LDFLAGS)

bindir = $(shell $(PKGCONFIG) $(GWY) --prefix)/bin

DNAME = $(PACKAGE)-$(VERSION)
#STD_DIST = Makefile COPYING makefile.msc pkg.mak $(PACKAGE).spec $(PACKAGE).iss
STD_DIST = Makefile COPYING pkg.mak
COMPILE = gcc
LINK = gcc
INSTALL = install

all: $(PACKAGE)

clean:
	-rm -f *~ *.o *.lo *.la *.so .libs/* core core.* $(PACKAGE)

%.o: %.c $(HEADERS)
	$(COMPILE) $(GWY_CFLAGS) $(EXTRA_CFLAGS) $(CFLAGS) -c $< -o $@

$(PACKAGE): $(OBJECTS)
	$(LINK) $(LDFLAGS) -o $@ $^

install: all
	mkdir -p $(DESTDIR)$(bindir)
	$(INSTALL) -s -c $(PACKAGE) $(DESTDIR)$(bindir)

uninstall:
	-rm -f $(DESTDIR)$(bindir)/$(PACKAGE)

$(PACKAGE).spec: pkg.mak
	cp $(PACKAGE).spec $(PACKAGE).spec~
	sed -e 's/^\(Version: \).*/\1$(VERSION)/' \
	    -e 's/^\(Source: .*\)-[0-9.]\+\.tar\.bz2/\1-$(VERSION).tar.bz2/' \
	    $(PACKAGE).spec~ >$(PACKAGE).spec

$(PACKAGE).iss: pkg.mak
	cp $(PACKAGE).iss $(PACKAGE).iss~
	sed -e 's/^\(AppVersion=\).*/\1$(VERSION)/' \
	    -e 's/^\(AppVerName=.* \)[0-9.]\+/\1$(VERSION)/' \
	    -e 's/^\(OutputBaseFilename=.*\)-[0-9.]\+/\1-$(VERSION)/' \
	    $(PACKAGE).iss~ >$(PACKAGE).iss

#dist: $(PACKAGE).spec $(PACKAGE).iss
dist:
	-rm -rf $(DNAME)
	mkdir $(DNAME)
	chmod a+rx $(DNAME)
	cp $(SOURCES) $(HEADERS) $(STD_DIST) $(EXTRA_DIST) $(DNAME)
	chmod a+r $(DNAME)/*
	tar cf - $(DNAME) | gzip > $(DNAME).tar.gz
	tar cf - $(DNAME) | bzip2 > $(DNAME).tar.bz2
	rm -rf $(DNAME)

.PHONY: all clean dist install uninstall distclean

