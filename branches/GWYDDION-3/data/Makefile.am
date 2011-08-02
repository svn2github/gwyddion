# Process this file with automake to produce Makefile.in
# $Id$

MAINTAINERCLEANFILES = \
	gwyddion3.vim

EXTRA_DIST = \
	gwyddion3-vim-cfg.py \
	gwyddion3.vim \
	gwyddion3.spec.in \

noinst_DATA = \
	gwyddion3.vim

vim_syn_gen = $(top_srcdir)/build/vim-syn-gen.py
gwyddion3_vim_cfg = $(srcdir)/gwyddion3-vim-cfg.py
gwyddion3_vim_sources = $(top_builddir)/docs/libgwy/libgwy-decl.txt

gwyddion3.vim: $(gwyddion3_vim_sources) $(gwyddion3_vim_cfg) $(vim_syn_gen)
	$(AM_V_GEN)$(PYTHON) $(vim_syn_gen) $(gwyddion3_vim_cfg) gwyddion3.vim

# VPATH fix
distclean-local:
	@test '$(srcdir)' = . || rm -rf $(MAINTAINERCLEANFILES)

