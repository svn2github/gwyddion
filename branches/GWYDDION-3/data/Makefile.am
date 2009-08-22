# Process this file with automake to produce Makefile.in
# $Id: Makefile.am 126 2009-01-20 19:41:15Z yeti $

EXTRA_DIST = \
	gwyddion-vim-cfg.py \
	gwyddion.vim

noinst_DATA = \
	gwyddion.vim

vim_syn_gen = $(top_srcdir)/build/vim-syn-gen.py
gwyddion_vim_cfg = $(srcdir)/gwyddion-vim-cfg.py

gwyddion.vim: $(gwyddion_vim_sources) $(gwyddion_vim_cfg) $(vim_syn_gen)
	$(PYTHON) $(vim_syn_gen) $(gwyddion_vim_cfg) gwyddion.vim
