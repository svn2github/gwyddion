# Generic library symbol rules.
# $Id$
# Variables: library libsuffix main_header Library library_minor_version
#            library_so_version
# Adds to: EXTRA_DIST CLEANFILES DISTCLEANFILES

pkgconfigdatadir = $(libdir)/pkgconfig

Library_ver = $(Library)$(libsuffix)-$(libsuffix).$(library_minor_version)
Library_with_ver = $(Library)$(libsuffix)_$(libsuffix)_$(library_minor_version)
library_la = .libs/$(library)$(libsuffix).la
library_decl = $(top_builddir)/docs/$(library)/$(library)-decl.txt
library_def = $(library)$(libsuffix).def
library_objects = $($(library)$(libsuffix)_la_OBJECTS)
$(eval $(library)$(libsuffix)_la_DEPENDENCIES += $(library_def))

INTROSPECTION_GIRS = $(Library_ver).gir
INTROSPECTION_SCANNER_ARGS = --strip=gwy_ -I..
$(eval $(Library_with_ver)_gir_LIBS = $(library)$(libsuffix).la)
$(eval $(Library_with_ver)_gir_FILES = $($(library)$(libsuffix)_la_SOURCES) $(library_headers))
girdir = $(datadir)/gir-1.0
dist_gir_DATA = $(Library_ver).gir
typelibdir = $(libdir)/girepository-1.0
typelib_DATA = $(Library_ver).typelib
pkgconfigdata_DATA = $(library)$(libsuffix).pc

if OS_WIN32
library_host_ldflags = -no-undefined -export-symbols $(library)$(libsuffix).def
endif

if UNSTABLE_LIBRARY_RELEASE
version_info = -release @GWY_VERSION_STRING@
else
version_info = -version-info $(library_so_version)
endif

-include $(INTROSPECTION_MAKEFILE)

CLEANFILES += \
	$(dist_gir_DATA) \
	$(typelib_DATA) \
	$(library_def)

DISTCLEANFILES += \
	$(library)$(libsuffix).pc

EXTRA_DIST += \
	$(library)$(libsuffix).pc.in

check-symbols: $(library_la) $(library_decl)
	$(AM_V_at)$(PYTHON) $(top_srcdir)/build/check-library-symbols.py \
	    "$(NM)" $(library_la) $(library_decl) $(srcdir)

check-headers: $(library_headers)
	$(AM_V_at)result=true; \
	for x in $(library_headers); do \
	    x='#include <$(library)/'$$(basename $$x)'>'; \
	    if ! grep -qF "$$x" $(main_header); then \
	       echo "$(main_header) lacks $$x" 1>&2; \
	       result=false; \
	    fi; \
	done; \
	$$result

$(library_def): $(library_objects)
	$(AM_V_GEN)$(PYTHON) $(top_srcdir)/build/generate-library-def.py \
	     "$(NM)" $(library_objects)

.PHONY: check-symbols check-headers
# run make check-symbols as part of make check
check-local: check-symbols check-headers

# vim: set ft=automake ts=4 sw=4 noet :
