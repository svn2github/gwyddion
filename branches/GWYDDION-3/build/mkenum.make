# Generic glib-mkenum rules.
# @(#) $Id$
# Variables: mkenum_name mkenum_id mkenum_headers library
# Adds to: BUILT_SOURCES CLEANFILES DISTCLEANFILES

mkenum_built_sources = $(mkenum_name).h $(mkenum_name).c

mkenum_stamp_files = $(mkenum_name).h.stamp
mkenum_c_template = $(top_srcdir)/build/mkenum.c.template
mkenum_h_template = $(top_srcdir)/build/mkenum.h.template
# Keep the `GENERATED' string quoted to prevent match here
mkenum_fix_output = \
	| $(SED) -e 's/@'ID'@/$(mkenum_id)/g' \
	      -e 's/@'SELF'@/$(mkenum_name)/g' \
	      -e 's/@'LIBRARY'@/$(library)/g' \
	      -e '1s:.*:/* This is a 'GENERATED' file. */:'

CLEANFILES += $(mkenum_stamp_files)
DISTCLEANFILES += $(mkenum_built_sources)
BUILT_SOURCES += $(mkenum_built_sources)

$(mkenum_name).h: $(mkenum_name).h.stamp
	$(AM_V_GEN)
	@true

$(mkenum_name).h.stamp: $(mkenum_headers) $(mkenum_h_template)
	$(AM_V_at)$(GLIB_MKENUMS) --template $(mkenum_h_template) $(mkenum_headers) \
		$(mkenum_fix_output) \
		>$(mkenum_name).h.tmp \
	&& ( cmp -s $(mkenum_name).h.tmp $(mkenum_name).h \
		|| cp $(mkenum_name).h.tmp $(mkenum_name).h ) \
	&& rm -f $(mkenum_name).h.tmp \
	&& echo timestamp >$(mkenum_name).h.stamp \
	&& touch $(mkenum_name).h

$(mkenum_name).c: $(mkenum_headers) $(mkenum_c_template)
	$(AM_V_GEN) $(GLIB_MKENUMS) --template $(mkenum_c_template) $(mkenum_headers) \
		$(mkenum_fix_output) \
		>$(mkenum_name).c.tmp \
	&& cp $(mkenum_name).c.tmp $(mkenum_name).c  \
	&& rm -f $(mkenum_name).c.tmp

# vim: set ft=automake noet :
