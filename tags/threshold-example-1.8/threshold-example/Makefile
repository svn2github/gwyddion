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

GWY_CFLAGS = $(shell $(PKGCONFIG) $(GWY) --cflags)
MODULE_DIR = $(shell $(PKGCONFIG) $(GWY) --variable=gwymoduledir)/$(MODULE_TYPE)
HOME_MODULE_DIR = $(HOME)/.gwyddion/modules/$(MODULE_TYPE)
EXTRA_CFLAGS = -Wall -O2 -march=`uname -m` -DVERSION=\"$(VERSION)\" -DPACKAGE=\"$(PACKAGE)\"
OBJECTS = $(SOURCES:.c=.o)

DNAME = $(PACKAGE)-$(VERSION)
STD_DIST = Makefile COPYING makefile.msc pkg.mak $(PACKAGE).spec $(PACKAGE).iss
COMPILE = libtool --mode=compile gcc
LINK = libtool --mode=link gcc
INSTALL = libtool --mode=install install

all: $(PACKAGE).so

clean:
	-rm -f *~ *.o *.lo *.la *.so .libs/* core core.* $(PACKAGE)

%.o: %.c $(HEADERS)
	$(COMPILE) -shared $(GWY_CFLAGS) $(EXTRA_CFLAGS) $(CFLAGS) -c $< -o $@

$(PACKAGE).so: $(OBJECTS)
	$(LINK) -shared -export-dynamic $(LDFLAGS) -Wl,-soname,$*.so -o $@ $^

install: all
	mkdir -p $(DESTDIR)$(MODULE_DIR)
	$(INSTALL) -s -c $(PACKAGE).so $(DESTDIR)$(MODULE_DIR)

install-user:
	mkdir -p $(HOME_MODULE_DIR)
	$(INSTALL) -s -c $(PACKAGE).so $(HOME_MODULE_DIR)

uninstall:
	-rm -f $(MODULE_DIR)/$(PACKAGE).so

uninstall-user:
	-rm -f $(HOME_MODULE_DIR)/$(PACKAGE).so

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

dist: $(PACKAGE).spec $(PACKAGE).iss
	-rm -rf $(DNAME)
	mkdir $(DNAME)
	chmod a+rx $(DNAME)
	cp $(SOURCES) $(HEADERS) $(STD_DIST) $(EXTRA_DIST) $(DNAME)
	chmod a+r $(DNAME)/*
	tar cf - $(DNAME) | gzip > $(DNAME).tar.gz
	tar cf - $(DNAME) | bzip2 > $(DNAME).tar.bz2
	rm -rf $(DNAME)

.PHONY: all clean dist install uninstall distclean

