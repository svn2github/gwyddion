# @(#) $Id$

filedir = $(gwyplugindir)/file

file_SCRIPTS = \
	dump.sh

EXTRA_DIST = $(file_SCRIPTS)
INCLUDES = -I$(top_srcdir)
AM_CPPFLAGS = -DDEBUG -DG_LOG_DOMAIN=\"Plug-in\"

clean-local:
	rm -f core.* *~
