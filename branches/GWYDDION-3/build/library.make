# Generic library symbol rules.
# $Id$
# Variables: library main_header
# Adds to: BUILT_SOURCES EXTRA_DIST DISTCLEANFILES

library_symbols = $(library)$(libsuffix).symbols
library_la = .libs/$(library)$(libsuffix).la
library_decl = $(top_builddir)/docs/$(library)/$(library)-decl.txt
library_aliases = $(library)-aliases
library_def = $(library)$(libsuffix).def
pkgconfigdatadir = $(libdir)/pkgconfig

BUILT_SOURCES += $(library_aliases).c $(library_aliases).h

DISTCLEANFILES += \
	$(library)$(libsuffix).pc \
	$(library_aliases).c \
	$(library_aliases).h \
	$(library_symbols)

EXTRA_DIST += \
	$(library)$(libsuffix).pc.in

pkgconfigdata_DATA = $(library)$(libsuffix).pc

check-symbols: $(library_la) $(library_decl)
	$(PYTHON) $(top_srcdir)/build/check-library-symbols.py \
	    $(library_la) $(library_decl) $(srcdir)

check-headers: $(library_headers)
	@result=true; \
	for x in $(library_headers); do \
	    x='#include <$(library)/'$$(basename $$x)'>'; \
	    if ! grep -qF "$$x" $(main_header); then \
	       echo "$(main_header) lacks $$x" 1>&2; \
	       result=false; \
	    fi; \
	done; \
	$$result

# FIXME: This should depend on library sources and process only that.
# Does not work in distcheck (i.e. does not check anything) because *.c is
# elsewhere.  More importantly, it should not check files such as
# object-internal.c because they correctly include H-aliases but as they do not
# define any public symbols, they do not need to include C-aliases.
#check-aliases:
#	@result=true \
#	aliases='#include "$(library)/$(library_aliases)'; \
#	for x in *.c; do \
#	    if grep -qF "$$aliases.h" $$x; then \
#	    macro=$$(echo -n "++$(library)/$$x++" | tr -c '[:alnum:]' _ | tr '[:lower:]' '[:upper:]'); \
#	        if ! grep -qF "#define $$macro" $$x; then \
#	           echo "$$x lacks #define $$macro" 1>&2; \
#	           result=false; \
#	        fi; \
#	        if ! grep -qF "$$aliases.c" $$x; then \
#	           echo "$$x lacks $$aliases.c" 1>&2; \
#	           result=false; \
#	        fi; \
#	    fi; \
#	done; \
#	$$result

$(library_symbols): $(library_headers)
	$(AM_V_GEN) $(PYTHON) $(abs_top_srcdir)/build/update-library-symbols.py \
	        $(library_symbols) $(library_headers)

$(library_def): $(library_symbols)
	$(AM_V_GEN)$(PYTHON) $(top_srcdir)/build/update-library-def.py \
	     $(library_def) $(library_symbols)

$(library_aliases).h: $(library_symbols) $(CONFIG_HEADER)
	$(AM_V_GEN)$(PYTHON) $(top_srcdir)/build/update-aliases.py \
	    $(library_aliases).h $(library_symbols) $(CONFIG_HEADER)

$(library_aliases).c: $(library_symbols) $(CONFIG_HEADER)
	$(AM_V_GEN)$(PYTHON) $(top_srcdir)/build/update-aliases.py \
	    $(library_aliases).c $(library_symbols) $(CONFIG_HEADER)

.PHONY: check-symbols check-headers
# run make check-symbols as part of make check
check-local: check-symbols check-headers

# vim: set ft=make ts=4 sw=4 noet :
