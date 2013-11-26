# Generic library symbol rules.
# $Id$
# Variables: library libsuffix main_header Library library_minor_version
#            library_so_version
# Adds to: EXTRA_DIST CLEANFILES DISTCLEANFILES

pkgconfigdatadir = $(libdir)/pkgconfig

Library_ver = $(Library)-$(libsuffix).$(library_minor_version)
Library_with_ver = $(Library)_$(libsuffix)_$(library_minor_version)
library_la = .libs/$(library)$(libsuffix).la
library_decl = $(top_builddir)/docs/$(library)$(libsuffix)/$(library)$(libsuffix)-decl.txt
library_def = $(library)$(libsuffix).def
library_objects = $($(library)$(libsuffix)_la_OBJECTS)
library_sources = $($(library)$(libsuffix)_la_SOURCES)
$(eval $(library)$(libsuffix)_la_DEPENDENCIES += $(library_def))

INTROSPECTION_GIRS = $(Library_ver).gir
INTROSPECTION_SCANNER_ARGS = \
	--identifier-prefix=Gwy --symbol-prefix=gwy_ \
	-I$(top_srcdir)/libraries -I$(top_builddir)/libraries \
	$(GOBJECT_INTROSPECTION_SCANNER_FLAGS)
$(eval $(Library_with_ver)_gir_LIBS = $(library)$(libsuffix).la)
$(eval $(Library_with_ver)_gir_FILES = $(library_sources) $(library_headers) $(library)$(libsuffix).la)
girdir = $(datadir)/gir-1.0
typelibdir = $(libdir)/girepository-1.0
if HAVE_INTROSPECTION
dist_gir_DATA = $(Library_ver).gir
typelib_DATA = $(Library_ver).typelib
endif
pkgconfigdata_DATA = $(library)$(libsuffix).pc

if OS_WIN32
library_host_ldflags = -export-symbols $(library)$(libsuffix).def
endif

if UNSTABLE_LIBRARY_RELEASE
version_info = -release $(GWY_VERSION_STRING)
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
	    "$(EU_NM)" $(library_la) $(library_decl) $(srcdir)

check-headers: $(library_headers)
	$(AM_V_at)$(PYTHON) $(top_srcdir)/build/check-library-headers.py \
	    $(library) $(main_header) $(library_headers)

check-marshallers: $(library_sources)
	$(AM_V_at)cd $(srcdir) && \
	$(PYTHON) $(top_srcdir)/build/check-marshallers.py $(library_sources)

$(library_def): $(library_objects)
	$(AM_V_GEN)$(PYTHON) $(top_srcdir)/build/generate-library-def.py \
	     "$(EU_NM)" $(library_objects) >$(library_def)

.PHONY: check-symbols check-headers check-marshallers
# run make check-symbols as part of make check
check-local: check-symbols check-headers check-marshallers

# vim: set ft=automake ts=4 sw=4 noet :
