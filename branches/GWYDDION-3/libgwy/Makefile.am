# $Id$

SUBDIRS = . tests

PEDANTIC = -pedantic -Wstrict-aliasing=1 -Wstrict-overflow -Winline
WARNINGS = -Wall -Wextra -Wstrict-prototypes -Werror=implicit-function-declaration -Werror=implicit-int -Wpointer-arith -Wstrict-aliasing -Wundef -Wshadow -Wredundant-decls -Wlogical-op

library = libgwy
libsuffix = $(PACKAGE_SUFFIX)
libgwyincludedir = $(pkgincludedir)$(PACKAGE_SUFFIX)/$(library)
test_program = ./tests/testlibgwy
mkenum_name = types
mkenum_id = LIBGWY_TYPES
main_header = libgwy.h

public_headers = \
	$(srcdir)/array.h \
	$(srcdir)/container.h \
	$(srcdir)/curve.h \
	$(srcdir)/error-list.h \
	$(srcdir)/expr.h \
	$(srcdir)/fft.h \
	$(srcdir)/field.h \
	$(srcdir)/field-arithmetic.h \
	$(srcdir)/field-distributions.h \
	$(srcdir)/field-level.h \
	$(srcdir)/field-statistics.h \
	$(srcdir)/field-transform.h \
	$(srcdir)/fitter.h \
	$(srcdir)/fit-function.h \
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
	$(srcdir)/mask-field-transform.h \
	$(srcdir)/math.h \
	$(srcdir)/pack.h \
	$(srcdir)/resource.h \
	$(srcdir)/rgba.h \
	$(srcdir)/serializable.h \
	$(srcdir)/serializable-boxed.h \
	$(srcdir)/serialize.h \
	$(srcdir)/strfuncs.h \
	$(srcdir)/unit.h \
	$(srcdir)/user-fit-function.h \
	$(srcdir)/value-format.h

public_built_headers = \
	$(mkenum_name).h \
	version.h

private_headers = \
	curve-internal.h \
	field-internal.h \
	line-internal.h \
	mask-field-internal.h \
	math-internal.h \
	object-internal.h

library_headers = $(public_headers) $(public_built_headers)
mkenum_headers = $(public_headers)

# Must init
EXTRA_DIST = version.h.in
CLEANFILES = $(library)$(libsuffix).supp
DISTCLEANFILES = $(public_built_headers)
MAINTAINERCLEANFILES =
BUILT_SOURCES =

lib_LTLIBRARIES = libgwy3.la
nodist_libgwyinclude_HEADERS = $(public_built_headers)
libgwyinclude_HEADERS = $(public_headers)
pkginclude_HEADERS = $(main_header)
noinst_HEADERS = $(private_headers)

libgwy3_la_SOURCES = \
	array.c \
	container.c \
	curve.c \
	error-list.c \
	expr.c \
	fft.c \
	field.c \
	field-arithmetic.c \
	field-distributions.c \
	field-level.c \
	field-statistics.c \
	field-transform.c \
	fitter.c \
	fit-function.c \
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
	mask-field-transform.c \
	math.c \
	object-internal.c \
	pack.c \
	resource.c \
	rgba.c \
	serializable.c \
	serializable-boxed.c \
	serialize.c \
	strfuncs.c \
	types.c \
	unit.c \
	user-fit-function.c \
	value-format.c \
	version.c

AM_CFLAGS = @LIBGWY_DEPS_CFLAGS@ $(WARNINGS)
AM_CPPFLAGS = \
	-I$(top_builddir) \
	-I$(top_srcdir) \
	-DGETTEXT_PACKAGE=\"@GETTEXT_PACKAGE@-libgwy\" \
	-DG_DISABLE_DEPRECATED -DG_DISABLE_SINGLE_INCLUDES -DGSEAL_ENABLE \
	-DG_LOG_DOMAIN=\"Gwy\" \
	-DGWY_LIBDIR=\"$(pkglibdir)\" \
	-DGWY_DATADIR=\"$(pkgdatadir)\" \
	-DGWY_LOCALE_DIR=\"$(localedir)\"

# In following order do:
# Any code change     C:   R++: A
# Any iface change    C++: 0:   A
# Adding ifaces       C:   R:   A++
# Changing ifaces     C:   R:   0
if UNSTABLE_LIBRARY_RELEASE
version_info = -release @GWY_VERSION_STRING@
else
version_info = -version-info 0:0:0
endif

libgwy3_la_LDFLAGS = @LIBGWY_DEPS_LIBS@ $(version_info)

include $(top_srcdir)/build/gtester.make
include $(top_srcdir)/build/library.make
include $(top_srcdir)/build/mkenum.make

clean-local:
	rm -rf core.* *~ *.gcda *.gcno *.gcov gmon.out

distclean-local:
	rm -rf coverage
