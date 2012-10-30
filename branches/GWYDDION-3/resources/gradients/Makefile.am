# Process this file with automake to produce Makefile.in
# $Id$

self = gradients

noinst_DATA = $(self).gresource.xml

resources = \
	DFit \
	Gwyddion.net \
	Sky \
	Spectral

$(self).gresource.xml: $(resources)
	$(AM_V_GEN)$(top_srcdir)/build/generate-gresource-xml.sh /net/gwyddion/gradients $(resources) >$(self).gresource.xml

EXTRA_DIST = $(resources)
CLEANFILES = $(self).gresource.xml
