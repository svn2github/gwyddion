# Generic glib-genmarshal rules.
# $Id$
# Variables: genmarshal_name genmarshal_prefix library
# Adds to: EXTRA_DIST BUILT_SOURCES CLEANFILES DISTCLEANFILES

genmarshal_built_sources = $(genmarshal_name).h $(genmarshal_name).c

genmarshal_self = $(top_srcdir)/build/genmarshal.make
genmarshal_stamp_files = $(genmarshal_name).h.stamp
genmarshal_cmd = $(GLIB_GENMARSHAL) --internal --stdinc --prefix=$(genmarshal_prefix)

EXTRA_DIST += $(genmarshal_name).list
CLEANFILES += $(genmarshal_stamp_files) $(genmarshal_name).c.tmp $(genmarshal_name).h.tmp
DISTCLEANFILES += $(genmarshal_built_sources)
BUILT_SOURCES += $(genmarshal_built_sources)

$(genmarshal_name).h: $(genmarshal_name).h.stamp
	$(AM_V_GEN)
	@true

# Keep the `GENERATED' string quoted to prevent match here
$(genmarshal_name).h.stamp: $(genmarshal_name).list $(genmarshal_self)
	$(AM_V_at)echo '/* This is a 'GENERATED' file */' >$(genmarshal_name).h.tmp \
	&& $(genmarshal_cmd) --header $(srcdir)/$(genmarshal_name).list \
		>>$(genmarshal_name).h.tmp \
	&& ( cmp -s $(genmarshal_name).h.tmp $(genmarshal_name).h \
		|| cp $(genmarshal_name).h.tmp $(genmarshal_name).h ) \
	&& rm -f $(genmarshal_name).h.tmp \
	&& echo timestamp >$(genmarshal_name).h.stamp \
	&& touch $(genmarshal_name).h

# Keep the `GENERATED' string quoted to prevent match here
$(genmarshal_name).c: $(genmarshal_name).list $(genmarshal_self)
	$(AM_V_GEN)echo '/* This is a 'GENERATED' file */' >$(genmarshal_name).c.tmp \
	&& echo '#include "$(genmarshal_name).h"' >>$(genmarshal_name).c.tmp \
	&& $(genmarshal_cmd) --body $(srcdir)/$(genmarshal_name).list \
		| $(SED) -e 's/^\( *\)\(GValue *\* *return_value,\)/\1G_GNUC_UNUSED \2/' \
			     -e 's/^\( *\)\(gpointer *invocation_hint,\)/\1G_GNUC_UNUSED \2/' \
		>>$(genmarshal_name).c.tmp \
	&& cp $(genmarshal_name).c.tmp $(genmarshal_name).c \
	&& rm -f $(genmarshal_name).c.tmp

# vim: set ft=automake noet :
