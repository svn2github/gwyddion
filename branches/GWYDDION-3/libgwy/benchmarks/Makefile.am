# $Id$

noinst_HEADERS = timer.h
noinst_SCRIPTS = \
	average.py \
	convolve-row.gpi

if BENCHMARKS
noinst_PROGRAMS = \
	convolve-row \
	memory-access \
	sort

convolve_row_SOURCES = convolve-row.c
memory_access_SOURCES = memory-access.c
sort_SOURCES = sort.c
extra_sources =

else
extra_sources = \
	convolve-row.c \
	memory-access.c \
	sort.c
endif

AM_CFLAGS = $(LIBGWY_DEPS_CFLAGS) $(VALGRIND_CFLAGS) $(GWY_CFLAGS)
AM_CPPFLAGS = \
	-I$(top_builddir) \
	-I$(top_srcdir) \
	-DG_DISABLE_DEPRECATED -DG_DISABLE_SINGLE_INCLUDES -DGSEAL_ENABLE
AM_LDFLAGS = ../libgwy3.la
EXTRA_DIST = \
	$(noinst_SCRIPTS) \
	$(extra_sources)

memory-access.dat: memory-access
	for x in 0 1 2 3 4 5 6 7 8 9; do \
		echo "Run $$x"; \
		./memory-access | tee memory-access-$$x.dat; \
	done
	$(PYTHON) average.py 1 memory-access-?.dat >memory-access.dat

clean-local:
	rm -rf core.* *~ *.gcda *.gcno *.gcov gmon.out

