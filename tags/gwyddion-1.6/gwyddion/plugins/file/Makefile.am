# @(#) $Id$

filedir = $(libexecdir)/@PACKAGE@/plugins/file

file_PROGRAMS = \
	dump

dump_SOURCES = \
	dump.c

EXTRA_DIST = makefile.msc
INCLUDES = -I$(top_srcdir)
AM_CPPFLAGS = -DDEBUG -DG_LOG_DOMAIN=\"Plug-in\"

clean-local:
	rm -f core.* *~
