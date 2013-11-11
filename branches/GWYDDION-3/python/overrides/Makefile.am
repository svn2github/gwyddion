# $Id$

# This is where G-IR installs its overrides.  Use the same location.
overridesdir = $(pyexecdir)/gi/overrides
overridesinit = $(overridesdir)/__init__.py

overrides_PYTHON = \
	Gwy.py

# When installing to a separate prefix we must create the package __init__.py.
# But if we install to a system location we must not overwrite the G-IR's own
# __init__.py.  If we install to DESTDIR, do not create it.
install-data-hook:
	$(AM_V_AT)if test "x$(DESTDIR)" = x && test ! -f "$(overridesinit)"; then \
	    echo '# Gwyddion placeholder.' >"$(overridesinit)"; \
	fi

uninstall-hook:
	$(AM_V_AT)if test "x$(DESTDIR)" = x && grep '^# Gwyddion placeholder\.' "$(overridesinit)" >/dev/null 2>/dev/null; then \
	    rm -f "$(overridesinit)"; \
	fi
