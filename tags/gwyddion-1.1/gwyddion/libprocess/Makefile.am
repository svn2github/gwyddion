# @(#) $Id$

libgwyddion = $(top_builddir)/libgwyddion/libgwyddion.la
libgwydraw = $(top_builddir)/libdraw/libgwydraw.la

libgwyprocessincludedir = $(includedir)/gwyddion/libprocess
libgwyprocessinclude_HEADERS = \
	cwt.h \
	datafield.h \
	dataline.h \
	interpolation.h \
	libgwyprocess.h \
	simplefft.h 

lib_LTLIBRARIES = libgwyprocess.la

EXTRA_DIST = \
	makefile.msc \
	libgwyprocess.def

#noinst_PROGRAMS = test

# In following order do:
# Any code change     C:   R++: A
# Any iface change    C++: 0:   A
# Adding ifaces       C:   R:   A++
# Chaning ifraces     C:   R:   0
libgwyprocess_la_LDFLAGS = -version-info 1:0:1
libgwyprocess_la_SOURCES = \
	cwt.c \
	datafield.c \
	dataline.c \
	interpolation.c \
	simplefft.c \
	grains.c \
	fractals.c \
	correct.c

#test_SOURCES = \
#	test.c
#test_LDADD = libgwyprocess.la \
#	$(libgwyddion)

INCLUDES = -I$(top_srcdir)
AM_CPPFLAGS = -DG_LOG_DOMAIN=\"GwyProcess\"

clean-local:
	rm -f core.* *~

