# @(#) $Id$

SUBDIRS = \
	m4 \
	libgwyddion \
	libprocess \
	libdraw \
	libgwydgets \
	libgwymodule \
	app \
	modules \
	plugins \
	po \
	pixmaps \
	perl \
	python \
	ruby \
	data \
	utils \
	thumbnailer \
	devel-docs

EXTRA_DIST = \
	INSTALL.gwyddion \
	autogen.sh \
	gwyddion.spec

DISTCLEANFILES = gwyconfig.h

DISTCHECK_CONFIGURE_FLAGS = \
	--enable-gtk-doc \
	--enable-pygwy \
	--enable-maintainer-mode \
	--disable-desktop-file-update

ACLOCAL_AMFLAGS = -I m4

if MAINTAINER_MODE
dist-check-maintainer:
else
dist-check-maintainer:
	@echo "*** maintainer mode must be enabled in order to make dist"
	@false
endif

docs:
	(cd devel-docs && $(MAKE) $(AM_MAKEFLAGS) docs)

configexecincludedir = $(pkglibdir)/include
configexecinclude_DATA = gwyconfig.h

.PHONY: docs
