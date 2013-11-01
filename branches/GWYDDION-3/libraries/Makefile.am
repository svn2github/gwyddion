# $Id$

SUBDIRS = libgwy libgwyui libgwyapp

toplevelincludedir = $(pkgincludedir)$(PACKAGE_SUFFIX)

toplevelinclude_HEADERS = libgwy.h libgwyui.h libgwyapp.h

GWY_RECURSIVE_TARGETS = \
	check-headers-recursive \
	check-marshallers-recursive \
	check-symbols-recursive

check-headers: check-headers-recursive
check-marshallers: check-marshallers-recursive
check-symbols: check-symbols-recursive

check-headers-local:
check-marshallers-local:
check-symbols-local:

include $(top_srcdir)/build/recursive.make
