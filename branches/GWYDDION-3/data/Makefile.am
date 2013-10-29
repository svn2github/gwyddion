# Process this file with automake to produce Makefile.in
# $Id$

MAINTAINERCLEANFILES = \
	gwyddion4.vim

EXTRA_DIST = \
	gwyddion4-vim-cfg.py \
	gwyddion4.vim \
	gwyddion4.spec.in

# Cannot make dists without gtk-doc anyway.  So no problem here.
if ENABLE_GTK_DOC
gtkdoc_data = gwyddion4.vim
else
gtkdoc_data =
endif

noinst_DATA = $(gtkdoc_data)

vim_syn_gen = $(top_srcdir)/build/vim-syn-gen.py
gwyddion4_vim_cfg = $(srcdir)/gwyddion4-vim-cfg.py
gwyddion4_vim_sources = \
	$(top_builddir)/docs/libgwy/libgwy-decl.txt \
	$(top_builddir)/docs/libgwyui/libgwyui-decl.txt

gwyddion4.vim: $(gwyddion4_vim_sources) $(gwyddion4_vim_cfg) $(vim_syn_gen)
	$(AM_V_GEN)$(PYTHON) $(vim_syn_gen) $(gwyddion4_vim_cfg) gwyddion4.vim

# VPATH fix
distclean-local:
	@test '$(srcdir)' = . || rm -rf $(MAINTAINERCLEANFILES)

