# @(#) $Id$

# When libTIFF is disabled, put the sources to EXTRA_DIST to include them in
# make dist.
if HAVE_TIFF
jpkscan_module = jpkscan.la
jpkscan_sources =
ols_module = ols.la
ols_sources =
else
jpkscan_module =
jpkscan_sources = jpk.h jpkscan.c
ols_module =
ols_sources = ols.c
endif

# When libXML2 is disabled, put the sources to EXTRA_DIST to include them in
# make dist.
if HAVE_XML2
spml_module = spml.la
spml_sources =
else
spml_module =
spml_sources = spml.c
endif

filedir = $(pkglibdir)/modules/file

file_LTLIBRARIES = \
	assing-afm.la \
	apefile.la \
	asciiexport.la \
	bcrfile.la \
	burleigh.la \
	burleigh_bii.la \
	burleigh_exp.la \
	createc.la \
	dmefile.la \
	ezdfile.la \
	ecsfile.la \
	gwyfile.la \
	hdf4file.la \
	hitachi-afm.la \
	intematix.la \
	jeol.la \
	$(jpkscan_module) \
	metropro.la \
	microprof.la \
	mifile.la \
	nanoscope-ii.la \
	nanoscope.la \
	nanonis.la \
	nanotop.la \
	netcdf.la \
	nt-mdt.la \
	omicron.la \
	omicronmatrix.la \
	$(ols_module) \
	opdfile.la \
	pixmap.la \
	pnifile.la \
	psia.la \
	quesant.la \
	rawfile.la \
	rawgraph.la \
	rhk-sm3.la \
	rhk-spm32.la \
	sdfile.la \
	seiko.la \
	sensofar.la \
	shimadzu.la \
	sicmfile.la \
	sis.la \
	$(spml_module) \
	spmlab.la \
	spmlabf.la \
	stmprg.la \
	stpfile.la \
	surffile.la \
	unisoku.la \
	witfile.la \
	wsxmfile.la

noinst_HEADERS = \
	err.h \
	get.h

EXTRA_DIST = \
	1CHECKLIST \
	$(jpkscan_sources) \
	$(ols_sources) \
	$(spml_sources) \
	makefile.msc \
	makefile.msc.gwt

if OS_WIN32
no_undefined = -no-undefined
endif

AM_CPPFLAGS = -I$(top_srcdir) -DG_LOG_DOMAIN=\"Module\"
AM_CFLAGS = @COMMON_CFLAGS@
AM_LDFLAGS = -avoid-version -module $(no_undefined) $(module_libadd)

if MODULE_DEPENDENCIES
MODULE_LIBTIFF = $(LIBTIFF)
module_libadd = \
	$(top_builddir)/app/libgwyapp2.la \
	$(top_builddir)/libdraw/libgwydraw2.la \
	$(top_builddir)/libgwyddion/libgwyddion2.la \
	$(top_builddir)/libgwydgets/libgwydgets2.la \
	$(top_builddir)/libgwymodule/libgwymodule2.la \
	$(top_builddir)/libprocess/libgwyprocess2.la \
	@GTK_LIBS@
else
MODULE_LIBTIFF =
endif

asciiexport_la_SOURCES   = asciiexport.c
apefile_la_SOURCES       = apefile.c
assing_afm_la_SOURCES    = assing-afm.c
bcrfile_la_SOURCES       = bcrfile.c
burleigh_la_SOURCES      = burleigh.c
burleigh_bii_la_SOURCES  = burleigh_bii.c
burleigh_exp_la_SOURCES  = burleigh_exp.c
createc_la_SOURCES       = createc.c
dmefile_la_SOURCES       = dmefile.c
ecsfile_la_SOURCES       = ecsfile.c
ezdfile_la_SOURCES       = ezdfile.c
gwyfile_la_SOURCES       = gwyfile.c
hdf4file_la_SOURCES      = hdf4file.c
hitachi_afm_la_SOURCES   = hitachi-afm.c
intematix_la_SOURCES     = intematix.c gwytiff.h
jeol_la_SOURCES          = jeol.c
metropro_la_SOURCES      = metropro.c
microprof_la_SOURCES     = microprof.c
mifile_la_SOURCES        = mifile.c
nanoscope_ii_la_SOURCES  = nanoscope-ii.c
nanoscope_la_SOURCES     = nanoscope.c
nanonis_la_SOURCES       = nanonis.c
nanotop_la_SOURCES       = nanotop.c
netcdf_la_SOURCES        = netcdf.c
nt_mdt_la_SOURCES        = nt-mdt.c
omicron_la_SOURCES       = omicron.c
omicronmatrix_la_SOURCES = omicronmatrix.c
opdfile_la_SOURCES       = opdfile.c
pnifile_la_SOURCES       = pnifile.c
psia_la_SOURCES          = psia.c gwytiff.h
quesant_la_SOURCES       = quesant.c
rawfile_la_SOURCES       = rawfile.c rawfilepreset.h
rawgraph_la_SOURCES      = rawgraph.c
rhk_sm3_la_SOURCES       = rhk-sm3.c
rhk_spm32_la_SOURCES     = rhk-spm32.c
sdfile_la_SOURCES        = sdfile.c
seiko_la_SOURCES         = seiko.c
sensofar_la_SOURCES      = sensofar.c
shimadzu_la_SOURCES      = shimadzu.c
sicmfile_la_SOURCES      = sicmfile.c
sis_la_SOURCES           = sis.c
spmlab_la_SOURCES        = spmlab.c
spmlabf_la_SOURCES       = spmlabf.c
stmprg_la_SOURCES        = stmprg.c
stpfile_la_SOURCES       = stpfile.c
surffile_la_SOURCES      = surffile.c
unisoku_la_SOURCES       = unisoku.c
witfile_la_SOURCES       = witfile.c
wsxmfile_la_SOURCES      = wsxmfile.c

# pixmap is built always, the source code branches on TIFF availability
pixmap_la_SOURCES        = pixmap.c
pixmap_la_LIBADD         = $(MODULE_LIBTIFF)

if HAVE_TIFF
jpkscan_la_SOURCES       = jpkscan.c jpk.h
jpkscan_la_LIBADD        = $(MODULE_LIBTIFF)
ols_la_SOURCES           = ols.c
ols_la_LIBADD            = $(MODULE_LIBTIFF)
endif

if HAVE_XML2
spml_la_SOURCES          = spml.c spml-utils.h
spml_la_CFLAGS           = $(AM_CFLAGS) @XML2_CFLAGS@
spml_la_LIBADD           = @XML2_LIBS@
endif

MAINTAINERCLEANFILES = makefile.msc

clean-local:
	rm -f core.* *~
