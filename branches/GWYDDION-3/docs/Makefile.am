# $Id$

SUBDIRS = libgwy libgwyui libgwyapp

EXTRA_DIST = style.css

GWY_RECURSIVE_TARGETS = docs-prepare-recursive docs-recursive

docs: docs-recursive
docs-prepare: docs-prepare-recursive
docs-local:
docs-prepare-local:

include $(top_srcdir)/build/recursive.make

