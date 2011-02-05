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
main_header = $(srcdir)/libgwy.h
Gwy3_3_0_gir_INCLUDES = Gio-2.0
Gwy3_3_0_gir_PACKAGES = gio-2.0

public_headers = \
	$(srcdir)/array.h \
	$(srcdir)/container.h \
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
	$(srcdir)/field-read.h \
	$(srcdir)/field-statistics.h \
	$(srcdir)/field-transform.h \
	$(srcdir)/fitter.h \
	$(srcdir)/fit-func.h \
	$(srcdir)/fit-param.h \
	$(srcdir)/fit-task.h \
	$(srcdir)/gl-material.h \
	$(srcdir)/gradient.h \
	$(srcdir)/inventory.h \
	$(srcdir)/interpolation.h \
	$(srcdir)/line.h \
	$(srcdir)/line-arithmetic.h \
	$(srcdir)/line-statistics.h \
	$(srcdir)/macros.h \
	$(srcdir)/main.h \
	$(srcdir)/mask-field.h \
	$(srcdir)/mask-field-arithmetic.h \
	$(srcdir)/mask-field-transform.h \
	$(srcdir)/math.h \
	$(srcdir)/pack.h \
	$(srcdir)/rectangle.h \
	$(srcdir)/resource.h \
	$(srcdir)/rgba.h \
	$(srcdir)/selection.h \
	$(srcdir)/serializable.h \
	$(srcdir)/serializable-boxed.h \
	$(srcdir)/serialize.h \
	$(srcdir)/strfuncs.h \
	$(srcdir)/unit.h \
	$(srcdir)/user-fit-func.h \
	$(srcdir)/value-format.h

public_built_headers = \
	$(mkenum_name).h \
	version.h

private_headers = \
	array-internal.h \
	curve-internal.h \
	field-internal.h \
	fit-func-builtin.h \
	line-internal.h \
	mask-field-internal.h \
	math-internal.h \
	object-internal.h

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
	container.c \
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
	inventory.c \
	interpolation.c \
	line.c \
	line-arithmetic.c \
	line-statistics.c \
	macros.c \
	main.c \
	mask-field.c \
	mask-field-arithmetic.c \
	mask-field-transform.c \
	math.c \
	object-internal.c \
	pack.c \
	rectangle.c \
	resource.c \
	rgba.c \
	selection.c \
	serializable.c \
	serializable-boxed.c \
	serialize.c \
	strfuncs.c \
	types.c \
	unit.c \
	user-fit-func.c \
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
