# $Id$

SUBDIRS = libgwy libgwyui
EXTRA_DIST = style.css

# Adapted from Automake's recursive rules
docs-recursive:
	@fail= failcom='exit 1'; \
	for f in x $$MAKEFLAGS; do \
	  case $$f in \
	    *=* | --[!k]*);; \
	    *k*) failcom='fail=yes';; \
	  esac; \
	done; \
	list='$(SUBDIRS)'; for subdir in $$list; do \
	  echo "Making docs in $$subdir"; \
	  ($(am__cd) $$subdir && $(MAKE) $(AM_MAKEFLAGS) docs) \
	  || eval $$failcom; \
	done; test -z "$$fail"

docs-prepare-recursive:
	@fail= failcom='exit 1'; \
	for f in x $$MAKEFLAGS; do \
	  case $$f in \
	    *=* | --[!k]*);; \
	    *k*) failcom='fail=yes';; \
	  esac; \
	done; \
	list='$(SUBDIRS)'; for subdir in $$list; do \
	  echo "Making docs-prepare in $$subdir"; \
	  ($(am__cd) $$subdir && $(MAKE) $(AM_MAKEFLAGS) docs-prepare) \
	  || eval $$failcom; \
	done; test -z "$$fail"

docs: docs-recursive

docs-prepare: docs-prepare-recursive
