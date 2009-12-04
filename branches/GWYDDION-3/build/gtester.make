# Generic gtester rules.
# $Id$
# Variables: test_program library libsuffix
# Adds to: CLEANFILES

GTESTER = gtester

CLEANFILES += \
	$(library)$(libsuffix).supp \
	full-report.xml \
	perf-report.xml \
	test-report.xml

### testing rules

# test: run all tests in cwd and subdirs
test: $(test_program)
	@$(GTESTER) --verbose $(test_program)
	@ for subdir in $(SUBDIRS) . ; do \
	    test "$$subdir" = "." -o "$$subdir" = "po" || \
	    ( cd $$subdir && $(MAKE) $(AM_MAKEFLAGS) $@ ) || exit $? ; \
	  done
# test-report: run tests in subdirs and generate report
# perf-report: run tests in subdirs with -m perf and generate report
# full-report: like test-report: with -m perf and -m slow
test-report perf-report full-report: $(test_program)
	@{ \
	  case $@ in \
	  test-report) test_options="-k";; \
	  perf-report) test_options="-k -m=perf";; \
	  full-report) test_options="-k -m=perf -m=slow";; \
	  esac ; \
	  if test -z "$$GTESTER_LOGDIR" ; then	\
	    ${GTESTER} --verbose $$test_options -o test-report.xml $(test_program) ; \
	    else \
	    ${GTESTER} --verbose $$test_options -o `mktemp "$$GTESTER_LOGDIR/log-XXXXXX"` $(test_program) ; \
	  fi ; \
	}
	@ ignore_logdir=true ; \
	  if test -z "$$GTESTER_LOGDIR" ; then \
	    GTESTER_LOGDIR=`mktemp -d "\`pwd\`/.testlogs-XXXXXX"`; export GTESTER_LOGDIR ; \
	    ignore_logdir=false ; \
	  fi ; \
	  for subdir in $(SUBDIRS) . ; do \
	    test "$$subdir" = "." -o "$$subdir" = "po" || \
	    ( cd $$subdir && $(MAKE) $(AM_MAKEFLAGS) $@ ) || exit $? ; \
	  done ; \
	  $$ignore_logdir || { \
	    echo '<?xml version="1.0"?>' > $@.xml ; \
	    echo '<report-collection>'  >> $@.xml ; \
	    for lf in `ls -L "$$GTESTER_LOGDIR"/.` ; do \
	      sed '1,1s/^<?xml\b[^>?]*?>//' <"$$GTESTER_LOGDIR"/"$$lf" >> $@.xml ; \
	    done ; \
	    echo >> $@.xml ; \
	    echo '</report-collection>' >> $@.xml ; \
	    rm -rf "$$GTESTER_LOGDIR"/ ; \
	    ${GTESTER_REPORT} --version 2>/dev/null 1>&2 ; test "$$?" != 0 || ${GTESTER_REPORT} $@.xml >$@.html ; \
	  }

# Run the test program but do not execute any tests.  This produces a list of
# ‘standard’ GLib errors we then filter out.
$(library)$(libsuffix).supp: testlibgwy$(EXEEXT)
	$(LIBTOOL) --mode=execute valgrind --log-fd=5 --gen-suppressions=all \
	    --tool=memcheck --leak-check=full --show-reachable=no \
	    $(test_program) -l >/dev/null 5>$(library)$(libsuffix).supp
	$(SED) -i -e '/^==/d' \
	    -e 's/<insert a suppression name here>/Suppression/' \
	    $(library)$(libsuffix).supp

# Run the test program with all tests (unless TEST_FLAGS says otherwise) under
# valgrind and report any problems.
test-valgrind: $(library)$(libsuffix).supp
	$(LIBTOOL) --mode=execute valgrind --tool=memcheck --leak-check=full \
	    --show-reachable=no --suppressions=$(library)$(libsuffix).supp \
	    $(test_program) $(TEST_FLAGS)

.PHONY: test test-report perf-report full-report test-valgrind
# run make test as part of make check
check-local: test

# vim: set ft=make noet :
