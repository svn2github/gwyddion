# @(#) $Id$

libgwymodule2includedir = $(pkgincludedir)/libgwymodule

libgwymodule2include_HEADERS = \
	gwymodule.h \
	gwymodule-file.h \
	gwymodule-graph.h \
	gwymodule-layer.h \
	gwymodule-process.h \
	gwymodule-tool.h \
	gwymodulebrowser.h \
	gwymoduleenums.h \
	gwymoduleloader.h \
	gwymoduletypes.h

noinst_HEADERS = \
	gwymoduleinternal.h

lib_LTLIBRARIES = libgwymodule2.la

EXTRA_DIST = \
	makefile.msc \
	makefile.msc.gwt \
	libgwymodule2.def

# Must init
CLEANFILES =
MAINTAINERCLEANFILES = \
	makefile.msc \
	libgwymodule2.def
BUILT_SOURCES =

MKENUM_NAME = gwymoduletypes
MKENUM_HFILES = gwymoduleenums.h
include $(top_srcdir)/utils/mkenum.mk

AM_CFLAGS = @COMMON_CFLAGS@
AM_CPPFLAGS = -I$(top_builddir) -DG_LOG_DOMAIN=\"GwyModule\"

# In following order do:
# Any code change     C:   R++: A
# Any iface change    C++: 0:   A
# Adding ifaces       C:   R:   A++
# Changing ifaces     C:   R:   0
libgwymodule2_la_LDFLAGS = @GMODULE_LIBS@ @GTK_LIBS@ @BASIC_LIBS@ -version-info 1:0:1
#libgwymodule2_la_LDFLAGS = @GMODULE_LIBS@ @GTK_LIBS@ @BASIC_LIBS@ -release @LIBRARY_RELEASE@
libgwymodule2_la_SOURCES = \
	gwymodule-file.c \
	gwymodule-graph.c \
	gwymodule-layer.c \
	gwymodule-process.c \
	gwymodule-tool.c \
	gwymodulebrowser.c \
	gwymoduleloader.c \
	gwymoduletypes.c

clean-local:
	rm -f core.* *~
