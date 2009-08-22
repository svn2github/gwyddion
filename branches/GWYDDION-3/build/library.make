# Generic library symbol rules.
# $Id$
# Variables: library

BUILT_SOURCES += $(library)-aliases.c $(library)-aliases.h

library_symbols = $(library)$(libsuffix).symbols
library_la = .libs/$(library)$(libsuffix).la
library_decl = $(top_builddir)/docs/$(library)/$(library)-decl.txt

EXTRA_DIST += \
	$(library_symbols) \
	$(library)-aliases.c \
	$(library)-aliases.h


check-symbols: $(library_la) $(library_decl)
	$(PYTHON) $(top_srcdir)/build/check-library-symbols.py \
	    $(library_la) $(library_decl) $(srcdir)

$(library_symbols): $(libgwyinclude_HEADERS)
	$(PYTHON) $(top_srcdir)/build/update-library-symbols.py \
	     $(library_symbols) $(libgwyinclude_HEADERS)

$(library)-aliases.h: $(library_symbols)
	$(PYTHON) $(top_srcdir)/build/update-aliases.py \
	    $(library)-aliases.h $(library_symbols)

$(library)-aliases.c: $(library_symbols)
	$(PYTHON) $(top_srcdir)/build/update-aliases.py \
	    $(library)-aliases.c $(library_symbols)

.PHONY: check-symbols

# vim: set ft=make noet :
