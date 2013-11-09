# $Id$

SUBDIRS = overrides

all-local: typelibpath.py

dist_noinst_SCRIPTS = \
	constructors.py \
	field-get-set.py

EXTRA_DIST = \
	typelibpath.py.in

typelibpath.py: typelibpath.py.in $(top_builddir)/config.status
	sed -e "s#libdir = '.*'#libdir = '$(libdir)'#" <$< >$@

CLEANFILES = \
	typelibpath.py \
	typelibpath.pyc

