#!/usr/bin/make
PACKAGE = gwydump
VERSION = 2.1
SOURCES = gwydump.c
HEADERS =
EXTRA_DIST = README

# Edit these for MS Windows cross-compilation, e.g.
#EXEEXT = .exe
#CROSSPREFIX = i686-pc-mingw32-
EXEEXT =
CROSSPREFIX =

PREFIX = /usr/local
PKGCONFIG = $(CROSSPREFIX)pkg-config
GLIB = glib-2.0

DEPS_CFLAGS := $(shell $(PKGCONFIG) $(GLIB) --cflags)
DEPS_LDFLAGS := $(shell $(PKGCONFIG) $(GLIB) --libs)
EXTRA_CFLAGS = -Wall -O2 -static -DVERSION=\"$(VERSION)\" -DPACKAGE=\"$(PACKAGE)\"
EXTRA_LDFLAGS =
OBJECTS = $(SOURCES:.c=.o)

bindir = $(shell $(PKGCONFIG) $(GLIB) --prefix)/bin

DNAME = $(PACKAGE)-$(VERSION)
STD_DIST = Makefile COPYING
COMPILE = $(CROSSPREFIX)gcc
LINK = $(CROSSPREFIX)gcc
INSTALL = install

all: $(PACKAGE)$(EXEEXT)

clean:
	-rm -f *~ *.o *.lo *.la *.so .libs/* core core.* $(PACKAGE)$(EXEEXT)

%.o: %.c $(HEADERS)
	$(COMPILE) $(DEPS_CFLAGS) $(EXTRA_CFLAGS) $(CFLAGS) -c $< -o $@

$(PACKAGE)$(EXEEXT): $(OBJECTS)
	$(LINK) $(EXTRA_LDFLAGS) -o $@ $^ $(DEPS_LDFLAGS) $(LDFLAGS)

install: all
	mkdir -p $(DESTDIR)$(bindir)
	$(INSTALL) -s -c $(PACKAGE)$(EXEEXT) $(DESTDIR)$(bindir)

uninstall:
	-rm -f $(DESTDIR)$(bindir)/$(PACKAGE)$(EXEEXT)

dist:
	-rm -rf $(DNAME)
	mkdir $(DNAME)
	chmod a+rx $(DNAME)
	cp $(SOURCES) $(HEADERS) $(STD_DIST) $(EXTRA_DIST) $(DNAME)
	chmod a+r $(DNAME)/*
	tar cf - $(DNAME) | xz > $(DNAME).tar.xz
	rm -rf $(DNAME)

.PHONY: all clean dist install uninstall distclean

