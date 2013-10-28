# $Id$

SUBDIRS = \
	m4 \
	build \
	resources \
	libgwy \
	libgwyui \
	libgwyapp \
	gwyddion \
	po \
	po-libgwy \
	po-libgwyui \
	po-libgwyapp \
	docs \
	data

EXTRA_DIST = \
	gwyddion3.spec \
	INSTALL.gwyddion

ACLOCAL_AMFLAGS = -I m4
DISTCHECK_CONFIGURE_FLAGS = --enable-gtk-doc
MAINTAINERCLEANFILES = \
	gwyddion3.spec
