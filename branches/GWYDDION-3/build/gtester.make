# Generic gtester rules.
# $Id$
# Variables: test_program library libsuffix
# Adds to: CLEANFILES

CLEANFILES += \
	$(test_program).supp \
	test-report.html \
	test-report-brief.html \
	test-report.xml

# Run the test program.
# If it succeeds get rid of the core files from expected failures, see
# https://bugzilla.gnome.org/show_bug.cgi?id=656127
test: all $(test_program)$(EXEEXT)
	$(AM_V_at)$(GTESTER) --verbose $(test_program)$(EXEEXT) $(TEST_FLAGS)
	$(AM_V_at)rm -f core core.[0-9]*

# Produce a test report
test-report: test-report.html test-report-brief.html

test-report.html: test-report.xml $(top_srcdir)/build/test-report.xsl
	$(AM_V_GEN)$(XSLTPROC) --stringparam program $(test_program) $(top_srcdir)/build/test-report.xsl test-report.xml >test-report.html

test-report-brief.html: test-report.xml $(top_srcdir)/build/test-report-brief.xsl
	$(AM_V_GEN)$(XSLTPROC) --stringparam program $(test_program) $(top_srcdir)/build/test-report-brief.xsl test-report.xml >test-report-brief.html

test-report.xml: $(test_program)$(EXEEXT)
	$(AM_V_at)-$(GTESTER) $(test_program)$(EXEEXT) $(TEST_FLAGS) -k -o test-report.xml

# Run the test program but do not execute any tests.  This produces a list of
# ‘standard’ GLib errors we then filter out.
$(test_program).supp: $(test_program)$(EXEEXT)
	$(AM_V_GEN)$(LIBTOOL) --mode=execute valgrind --log-fd=5 --gen-suppressions=all \
	    --tool=memcheck --leak-check=full --show-reachable=no \
	    $(test_program)$(EXEEXT) -l >/dev/null 5>$(test_program).supp
	$(AM_V_AT)$(SED) -i -e '/^==/d' $(test_program).supp

# Run the test program with all tests (unless TEST_FLAGS says otherwise) under
# valgrind and report any problems.
test-valgrind: $(test_program).supp
	$(LIBTOOL) --mode=execute valgrind --tool=memcheck --leak-check=full \
	    --show-reachable=no --suppressions=$(test_program).supp \
	    --suppressions=$(srcdir)/$(test_default_suppressions) \
	    --track-origins=yes --read-var-info=yes --num-callers=20 \
	    --child-silent-after-fork=yes \
	    $(test_program)$(EXEEXT) $(TEST_FLAGS)

.PHONY: test test-report test-valgrind
# run make test as part of make check
check-local: test

# vim: set ft=automake noet :
