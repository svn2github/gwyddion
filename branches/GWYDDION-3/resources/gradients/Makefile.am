# Process this file with automake to produce Makefile.in
# $Id$

self = gradients

noinst_DATA = $(self).gresource.xml gradients.h

resources = \
	DFit \
	Gwyddion.net \
	Sky \
	Spectral

$(self).gresource.xml: $(resources)
	$(AM_V_GEN)$(top_srcdir)/build/generate-gresource-xml.sh /net/gwyddion/gradients $(resources) >$(self).gresource.xml

gradients.h: $(resources) $(srcdir)/gwy-gradients-csource.py
	$(AM_V_GEN)$(PYTHON) $(srcdir)/gwy-gradients-csource.py \
	    '$(srcdir)' gwy_gradients_builtin $(resources) >gradients.h

EXTRA_DIST = $(resources) gwy-gradients-csource.py
CLEANFILES = $(self).gresource.xml gradients.h
