# @(#) $Id$

toolsdir = $(pkglibdir)/modules/tool

module_ldflags = -shared -Wl,-soname

tools_PROGRAMS = \
	distance.so \
	crop.so \
	filter.so \
	grain_remove_manually.so \
	icolorange.so \
	level3.so \
	maskedit.so \
	polynom.so \
	profile.so \
	readvalue.so \
	sfunctions.so \
	spotremove.so \
	stats.so

#############################################################################
# If you know what an alphabet is, please keep the alphabetic order.
# If you have no idea, please return to basic school.
#############################################################################

EXTRA_DIST = \
	makefile.msc

crop_so_SOURCES = crop.c
crop_so_LDFLAGS = $(module_ldflags),crop.so

distance_so_SOURCES = distance.c
distance_so_LDFLAGS = $(module_ldflags),distance.so

filter_so_SOURCES = filter.c
filter_so_LDFLAGS = $(module_ldflags),filter.so

grain_remove_manually_so_SOURCES = grain_remove_manually.c
grain_remove_manually_so_LDFLAGS = $(module_ldflags),grain_remove_manually.so

icolorange_so_SOURCES = icolorange.c
icolorange_so_LDFLAGS = $(module_ldflags),icolorange.so

level3_so_SOURCES = level3.c
level3_so_LDFLAGS = $(module_ldflags),level3.so

maskedit_so_SOURCES = maskedit.c
maskedit_so_LDFLAGS = $(module_ldflags),maskedit.so

polynom_so_SOURCES = polynom.c
polynom_so_LDFLAGS = $(module_ldflags),polynom.so

profile_so_SOURCES = profile.c
profile_so_LDFLAGS = $(module_ldflags),profile.so

readvalue_so_SOURCES = readvalue.c
readvalue_so_LDFLAGS = $(module_ldflags),readvalue.so

stats_so_SOURCES = stats.c
stats_so_LDFLAGS = $(module_ldflags),stats.so

sfunctions_so_SOURCES = sfunctions.c
sfunctions_so_LDFLAGS = $(module_ldflags),sfunctions.so

spotremove_so_SOURCES = spotremove.c
spotremove_so_LDFLAGS = $(module_ldflags),spotremove.so


INCLUDES = -I$(top_srcdir)
AM_CPPFLAGS = -DG_LOG_DOMAIN=\"Module\" -fPIC

clean-local:
	rm -f core.* *~
