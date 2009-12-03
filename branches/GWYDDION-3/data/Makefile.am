# Process this file with automake to produce Makefile.in
# $Id$

pkgconfigdatadir = $(libdir)/pkgconfig

MAINTAINERCLEANFILES = \
	gwyddion.vim \
	libgwy3.pc

EXTRA_DIST = \
	gwyddion-vim-cfg.py \
	gwyddion.vim \
	libgwy3.pc.in

pkgconfigdata_DATA = libgwy3.pc

noinst_DATA = \
	gwyddion.vim

vim_syn_gen = $(top_srcdir)/build/vim-syn-gen.py
gwyddion_vim_cfg = $(srcdir)/gwyddion-vim-cfg.py
gwyddion_vim_sources = $(top_builddir)/docs/libgwy/libgwy-decl.txt

gwyddion.vim: $(gwyddion_vim_sources) $(gwyddion_vim_cfg) $(vim_syn_gen)
	$(PYTHON) $(vim_syn_gen) $(gwyddion_vim_cfg) gwyddion.vim
