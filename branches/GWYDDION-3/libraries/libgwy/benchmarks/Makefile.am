# $Id$

noinst_HEADERS = timer.h
noinst_SCRIPTS = \
	average.py \
	convolve-row.gpi

if BENCHMARKS
noinst_PROGRAMS = \
	clamp \
	convolve-rect \
	convolve-row \
	count-bits \
	distance-transform \
	memory-access \
	rand \
	sort

clamp_SOURCES = clamp.c
convolve_row_SOURCES = convolve-row.c
convolve_rect_SOURCES = convolve-rect.c
count_bits_SOURCES = count-bits.c
distance_transform_SOURCES = distance-transform.c
memory_access_SOURCES = memory-access.c
rand_SOURCES = rand.c
sort_SOURCES = sort.c
extra_sources =

else
extra_sources = \
	clamp.c \
	convolve-rect.c \
	convolve-row.c \
	count-bits.c \
	distance-transform.c \
	memory-access.c \
	rand.c \
	sort.c
endif

AM_CFLAGS = $(LIBGWY_DEPS_CFLAGS) $(VALGRIND_CFLAGS) $(GWY_CFLAGS)
AM_CPPFLAGS = -I$(top_builddir) -I$(top_srcdir) $(LIBGWY_COMPAT_CPPFLAGS)
AM_LDFLAGS = ../libgwy3.la
EXTRA_DIST = \
	$(noinst_SCRIPTS) \
	$(extra_sources)

memory-access.dat: memory-access
	for x in 0 1 2 3 4 5 6 7 8 9; do \
		echo "Run $$x"; \
		./memory-access$(EXEEXT) | tee memory-access-$$x.dat; \
	done
	$(PYTHON) average.py 1 memory-access-?.dat >memory-access.dat

clean-local:
	rm -rf core.* *~ *.gcda *.gcno *.gcov gmon.out

