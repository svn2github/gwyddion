# Generic library symbol rules.
# $Id$
# Variables: library

BUILT_SOURCES += $(library)-aliases.c $(library)-aliases.h

EXTRA_DIST += \
	$(library).symbols \
	$(library)-aliases.c \
	$(library)-aliases.h

library_la = .libs/$(library).la
library_decl = $(top_builddir)/docs/$(library)/$(library)-decl.txt

check-symbols: $(library_la) $(library_decl)
	$(PYTHON) $(top_srcdir)/build/check-library-symbols.py \
	    $(library_la) $(library_decl) $(srcdir)

$(library).symbols: $(libgwyinclude_HEADERS)
	$(PYTHON) $(top_srcdir)/build/update-library-symbols.py \
	    $(libgwyinclude_HEADERS) >$(library).symbols

$(library)-aliases.h: $(library).symbols
	$(PYTHON) $(top_srcdir)/build/update-aliases.py h \
	    <$(library).symbols >$(library)-aliases.h

$(library)-aliases.c: $(library).symbols
	$(PYTHON) $(top_srcdir)/build/update-aliases.py c \
	    <$(library).symbols >$(library)-aliases.c

.PHONY: check-symbols

# vim: set ft=make noet :
