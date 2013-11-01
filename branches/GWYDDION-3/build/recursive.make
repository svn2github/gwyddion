# Generic shallow recursive rules.
# $Id: library.make 15546 2013-11-01 12:56:07Z yeti-dn $
# Variables: GWY_RECURSIVE_TARGETS

# Adapted from Automake's recursive rules
$(GWY_RECURSIVE_TARGETS):
	@fail=; \
	if $(am__make_keepgoing); then \
	  failcom='fail=yes'; \
	else \
	  failcom='exit 1'; \
	fi; \
	dot_seen=no; \
	target=`echo $@ | sed s/-recursive//`; \
	list='$(SUBDIRS)'; \
	for subdir in $$list; do \
	  echo "Making $$target in $$subdir"; \
	  if test "$$subdir" = "."; then \
	    dot_seen=yes; \
	    local_target="$$target-local"; \
	  else \
	    local_target="$$target"; \
	  fi; \
	  ($(am__cd) $$subdir && $(MAKE) $(AM_MAKEFLAGS) $$local_target) \
	  || eval $$failcom; \
	done; \
	if test "$$dot_seen" = "no"; then \
	  $(MAKE) $(AM_MAKEFLAGS) "$$target-local" || exit 1; \
	fi; test -z "$$fail"

# vim: set ft=automake ts=4 sw=4 noet :
