# Generic library symbol rules.
# $Id$
# Variables: library
# Adds to: BUILT_SOURCES EXTRA_DIST

library_symbols = $(library)$(libsuffix).symbols
library_la = .libs/$(library)$(libsuffix).la
library_decl = $(top_builddir)/docs/$(library)/$(library)-decl.txt
library_aliases = $(library)-aliases
library_def = $(library)$(libsuffix).def

BUILT_SOURCES += $(library_aliases).c $(library_aliases).h

EXTRA_DIST += \
	$(library_symbols) \
	$(library_aliases).c \
	$(library_aliases).h

check-symbols: $(library_la) $(library_decl)
	$(PYTHON) $(top_srcdir)/build/check-library-symbols.py \
	    $(library_la) $(library_decl) $(srcdir)

$(library_symbols): $(libgwyinclude_HEADERS)
	$(AM_V_GEN)$(PYTHON) $(top_srcdir)/build/update-library-symbols.py \
	     $(library_symbols) $(libgwyinclude_HEADERS)

$(library_def): $(library_symbols)
	$(AM_V_GEN)$(PYTHON) $(top_srcdir)/build/update-library-def.py \
	     $(library_def) $(library_symbols)

$(library_aliases).h: $(library_symbols)
	$(AM_V_GEN)$(PYTHON) $(top_srcdir)/build/update-aliases.py \
	    $(library_aliases).h $(library_symbols)

$(library_aliases).c: $(library_symbols)
	$(AM_V_GEN)$(PYTHON) $(top_srcdir)/build/update-aliases.py \
	    $(library_aliases).c $(library_symbols)

.PHONY: check-symbols

# vim: set ft=make noet :
