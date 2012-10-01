# $Id$

SUBDIRS = . tests benchmarks

library = libgwy
Library = Gwy
library_minor_version = 0
libsuffix = $(PACKAGE_SUFFIX)
toplevelincludedir = $(pkgincludedir)$(PACKAGE_SUFFIX)
libgwyincludedir = $(pkgincludedir)$(PACKAGE_SUFFIX)/$(library)
test_program = ./tests/testlibgwy
test_default_suppressions = libgwy-default.supp
mkenum_name = types
mkenum_id = LIBGWY_TYPES
#genmarshal_name = marshal
#genmarshal_prefix = _gwy_
main_header = $(srcdir)/libgwy.h
Gwy3_3_0_gir_INCLUDES = Gio-2.0
Gwy3_3_0_gir_PACKAGES = gio-2.0

public_headers = \
	$(srcdir)/array.h \
	$(srcdir)/brick.h \
	$(srcdir)/brick-arithmetic.h \
	$(srcdir)/brick-part.h \
	$(srcdir)/brick-statistics.h \
	$(srcdir)/calc.h \
	$(srcdir)/container.h \
	$(srcdir)/coords.h \
	$(srcdir)/coords-point.h \
	$(srcdir)/curve.h \
	$(srcdir)/curve-statistics.h \
	$(srcdir)/error-list.h \
	$(srcdir)/expr.h \
	$(srcdir)/fft.h \
	$(srcdir)/field.h \
	$(srcdir)/field-arithmetic.h \
	$(srcdir)/field-correlate.h \
	$(srcdir)/field-distributions.h \
	$(srcdir)/field-filter.h \
	$(srcdir)/field-level.h \
	$(srcdir)/field-part.h \
	$(srcdir)/field-read.h \
	$(srcdir)/field-statistics.h \
	$(srcdir)/field-transform.h \
	$(srcdir)/fitter.h \
	$(srcdir)/fit-func.h \
	$(srcdir)/fit-param.h \
	$(srcdir)/fit-task.h \
	$(srcdir)/gl-material.h \
	$(srcdir)/gradient.h \
	$(srcdir)/grain-value.h \
	$(srcdir)/int-set.h \
	$(srcdir)/interpolation.h \
	$(srcdir)/inventory.h \
	$(srcdir)/line.h \
	$(srcdir)/line-arithmetic.h \
	$(srcdir)/line-distributions.h \
	$(srcdir)/line-part.h \
	$(srcdir)/line-statistics.h \
	$(srcdir)/macros.h \
	$(srcdir)/main.h \
	$(srcdir)/mask-field.h \
	$(srcdir)/mask-field-arithmetic.h \
	$(srcdir)/mask-field-grains.h \
	$(srcdir)/mask-field-transform.h \
	$(srcdir)/mask-iter.h \
	$(srcdir)/mask-line.h \
	$(srcdir)/master.h \
	$(srcdir)/math.h \
	$(srcdir)/object-utils.h \
	$(srcdir)/pack.h \
	$(srcdir)/rand.h \
	$(srcdir)/resource.h \
	$(srcdir)/rgba.h \
	$(srcdir)/serializable.h \
	$(srcdir)/serializable-boxed.h \
	$(srcdir)/serialize.h \
	$(srcdir)/strfuncs.h \
	$(srcdir)/surface.h \
	$(srcdir)/surface-statistics.h \
	$(srcdir)/unit.h \
	$(srcdir)/user-fit-func.h \
	$(srcdir)/user-grain-value.h \
	$(srcdir)/value-format.h

public_built_headers = \
	$(mkenum_name).h \
	version.h

private_headers = \
	array-internal.h \
	brick-internal.h \
	curve-internal.h \
	field-internal.h \
	fit-func-builtin.h \
	grain-value-builtin.h \
	line-internal.h \
	mask-internal.h \
	mask-field-internal.h \
	mask-line-internal.h \
	math-internal.h \
	object-internal.h \
	surface-internal.h

library_headers = $(public_headers) $(public_built_headers)
mkenum_headers = $(public_headers)

# Must init
EXTRA_DIST = version.h.in $(test_default_suppressions)
CLEANFILES = $(library)$(libsuffix).supp
DISTCLEANFILES = $(public_built_headers)
MAINTAINERCLEANFILES =
libgwy3_la_DEPENDENCIES =
BUILT_SOURCES =

# lib_LTLIBRARIES must be a literal, no variables
lib_LTLIBRARIES = libgwy3.la
nodist_libgwyinclude_HEADERS = $(public_built_headers)
libgwyinclude_HEADERS = $(public_headers)
toplevelinclude_HEADERS = $(main_header)
noinst_HEADERS = $(private_headers)

libgwy3_la_SOURCES = \
	array.c \
	brick.c \
	brick-arithmetic.c \
	brick-part.c \
	brick-statistics.c \
	calc.c \
	container.c \
	coords.c \
	coords-point.c \
	curve.c \
	curve-statistics.c \
	error-list.c \
	expr.c \
	fft.c \
	field.c \
	field-arithmetic.c \
	field-correlate.c \
	field-distributions.c \
	field-filter.c \
	field-level.c \
	field-part.c \
	field-read.c \
	field-statistics.c \
	field-transform.c \
	fitter.c \
	fit-func.c \
	fit-func-builtin.c \
	fit-param.c \
	fit-task.c \
	gl-material.c \
	gradient.c \
	grain-value.c \
	grain-value-builtin.c \
	int-set.c \
	interpolation.c \
	inventory.c \
	line.c \
	line-arithmetic.c \
	line-distributions.c \
	line-part.c \
	line-statistics.c \
	macros.c \
	main.c \
	mask-field.c \
	mask-field-arithmetic.c \
	mask-field-grains.c \
	mask-field-transform.c \
	mask-iter.c \
	mask-line.c \
	mask-line-arithmetic.c \
	master.c \
	math.c \
	object-internal.c \
	object-utils.c \
	pack.c \
	rand.c \
	resource.c \
	rgba.c \
	serializable.c \
	serializable-boxed.c \
	serialize.c \
	strfuncs.c \
	surface.c \
	surface-statistics.c \
	types.c \
	unit.c \
	user-fit-func.c \
	user-grain-value.c \
	value-format.c \
	version.c

AM_CFLAGS = $(LIBGWY_DEPS_CFLAGS) $(HOST_CFLAGS) $(GWY_CFLAGS)
AM_CPPFLAGS = \
	-I$(top_builddir) \
	-I$(top_srcdir) \
	$(LIBGWY_COMPAT_CPPFLAGS) \
	-DGETTEXT_PACKAGE=\"$(GETTEXT_PACKAGE)-libgwy\" \
	-DG_LOG_DOMAIN=\"Gwy\" \
	-DGWY_LIBDIR=\"$(pkglibdir)\" \
	-DGWY_DATADIR=\"$(pkgdatadir)\" \
	-DGWY_LOCALE_DIR=\"$(localedir)\"

# In following order do:
# Any code change     C:   R++: A
# Any iface change    C++: 0:   A
# Adding ifaces       C:   R:   A++
# Changing ifaces     C:   R:   0
library_so_version = 0:0:0

libgwy3_la_LDFLAGS = \
	$(LIBGWY_DEPS_LIBS) \
	$(HOST_LDFLAGS) \
	$(GWY_LIBRARY_LDFLAGS) \
	$(version_info) \
	$(library_host_ldflags)

include $(top_srcdir)/build/gtester.make
include $(top_srcdir)/build/library.make
include $(top_srcdir)/build/mkenum.make

clean-local:
	rm -rf core.* *~ *.gcda *.gcno *.gcov gmon.out tmp-introspect*

distclean-local:
	rm -rf coverage

check-local: check-serializable-types

check-serializable-types: $(libgwy3_la_SOURCES)
	$(AM_V_at)cd $(srcdir) \
		&& $(PYTHON) ../build/check-serializable-types.py main.c $(libgwy3_la_SOURCES)

check-marshallers: $(libgwy3_la_SOURCES)
	$(AM_V_at)$(PYTHON) $(top_srcdir)/build/check-marshallers.py $(libgwy3_la_SOURCES)

.PHONY: check-serializable-types check-marshallers
