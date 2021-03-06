# @(#) $Id$

# Put sources that are for some reason disabled yet present in svn checkouts
# here.  This makes them distributed and translations of their messages are
# kept.  If we remove them completely, various bits can get lost.
disabled_sources =

# If libXML2 is disabled, put the dependent sources to EXTRA_DIST to include
# them in make dist.
if HAVE_XML2
if HAVE_ZLIB
spml_module = spml.la
else
spml_module =
disabled_sources += spml.c
endif
else
spml_module =
disabled_sources += spml.c
endif

# If libXML2 is disabled, put the dependent sources to EXTRA_DIST to include
# them in make dist.
if HAVE_XML2
if HAVE_MINIZIP
apedaxfile_module = apedaxfile.la
else
apedaxfile_module =
disabled_sources += apedaxfile.c
endif
else
apedaxfile_module =
disabled_sources += apedaxfile.c
endif

# If libz is disabled, put the dependent sources to EXTRA_DIST to include them
# in make dist.
#if HAVE_ZLIB
#matfile_module = matfile.la
#matfile_sources =
#else
matfile_module =
disabled_sources += matfile.c
#endif

# If minizip is disabled, put the dependent sources to EXTRA_DIST to include
# them in make dist.
if HAVE_MINIZIP
nanoobserver_module = nanoobserver.la
nanoscantech_module = nanoscantech.la
opengps_module = opengps.la
spmxfile_module = spmxfile.la
else
nanoobserver_module =
nanoscantech_module =
opengps_module =
spmxfile_module =
disabled_sources += nanoobserver.c nanoscantech.c opengps.c spmxfile.c
endif

# If cfitsio is disabled, put the dependent sources to EXTRA_DIST to include
# them in make dist.
if HAVE_CFITSIO
fitsfile_module = fitsfile.la
else
fitsfile_module =
disabled_sources += fitsfile.c
endif

# If C++ is not available, put the dependent sources to EXTRA_DIST to include
# them in make dist.
if HAVE_CXX
hdrimage_module = hdrimage.la
else
hdrimage_module =
disabled_sources += hdrimage.cc
endif

filedir = $(pkglibdir)/modules/file

file_LTLIBRARIES = \
	accurexii-txt.la \
	aistfile.la \
	alicona.la \
	ambfile.la \
	anfatec.la \
	apefile.la \
	$(apedaxfile_module) \
	asciiexport.la \
	assing-afm.la \
	attocube.la \
	bcrfile.la \
	burleigh.la \
	burleigh_bii.la \
	burleigh_exp.la \
	codevfile.la \
	createc.la \
	csmfile.la \
	dmefile.la \
	dm3file.la \
	dumbfile.la \
	ecsfile.la \
	ezdfile.la \
	$(fitsfile_module) \
	femtoscan.la \
	femtoscan-txt.la \
	gdeffile.la \
	gsffile.la \
	gwyfile.la \
	gxyzffile.la \
	hdf4file.la \
	$(hdrimage_module) \
	hitachi-afm.la \
	hitachi-sem.la \
	igorfile.la \
	imgexport.la \
	intematix.la \
	iso28600.la \
	jeol.la \
	jpkscan.la \
	jspmfile.la \
	lextfile.la \
	magellan.la \
	mapvue.la \
	$(matfile_module) \
	metropro.la \
	microprof.la \
	miffile.la \
	mifile.la \
	mulfile.la \
	nanoeducator.la \
	nanomagnetics.la \
	nanonics.la \
	nanonis.la \
	$(nanoobserver_module) \
	nanoscan.la \
	$(nanoscantech_module) \
	nanoscope-ii.la \
	nanoscope.la \
	nanotop.la \
	netcdf.la \
	nrrdfile.la \
	nt-mdt.la \
	nxiifile.la \
	oldmda.la \
	ols.la \
	ometiff.la \
	omicron.la \
	omicronflat.la \
	omicronmatrix.la \
	opdfile.la \
	$(opengps_module) \
	pixmap.la \
	pnifile.la \
	pltfile.la \
	psia.la \
	pt3file.la \
	quesant.la \
	rawfile.la \
	rawgraph.la \
	rawxyz.la \
	renishaw.la \
	rhk-sm3.la \
	rhk-sm4.la \
	rhk-spm32.la \
	robotics.la \
	s94file.la \
	sdfile.la \
	seiko.la \
	sensofar.la \
	sensolytics.la \
	shimadzu.la \
	sicmfile.la \
	sis.la \
	spip-asc.la \
	$(spml_module) \
	spmlab.la \
	spmlabf.la \
	$(spmxfile_module) \
	stmprg.la \
	stpfile.la \
	surffile.la \
	tescan.la \
	tiaser.la \
	unisoku.la \
	vtkfile.la \
	win_stm.la \
	wipfile.la \
	witec-asc.la \
	witfile.la \
	wsffile.la \
	wsxmfile.la \
	xyzexport.la \
	zeiss.la \
	zemax.la

noinst_HEADERS = \
	err.h \
	get.h

EXTRA_DIST = \
	$(disabled_sources) \
	1CHECKLIST

if OS_MSWIN
no_undefined = -no-undefined
endif

AM_CPPFLAGS = -I$(top_srcdir) -DG_LOG_DOMAIN=\"Module\"
AM_CFLAGS = @COMMON_CFLAGS@
AM_CXXFLAGS = @COMMON_CXXFLAGS@
AM_LDFLAGS = -avoid-version -module $(no_undefined) $(module_libadd)

if MODULE_DEPENDENCIES
module_libadd = \
	$(top_builddir)/app/libgwyapp2.la \
	$(top_builddir)/libdraw/libgwydraw2.la \
	$(top_builddir)/libgwyddion/libgwyddion2.la \
	$(top_builddir)/libgwydgets/libgwydgets2.la \
	$(top_builddir)/libgwymodule/libgwymodule2.la \
	$(top_builddir)/libprocess/libgwyprocess2.la \
	@GTK_LIBS@
endif

accurexii_txt_la_SOURCES = accurexii-txt.c
aistfile_la_SOURCES      = aistfile.c
alicona_la_SOURCES       = alicona.c
ambfile_la_SOURCES       = ambfile.c
anfatec_la_SOURCES       = anfatec.c
apefile_la_SOURCES       = apefile.c
asciiexport_la_SOURCES   = asciiexport.c
assing_afm_la_SOURCES    = assing-afm.c
attocube_la_SOURCES      = attocube.c
bcrfile_la_SOURCES       = bcrfile.c
burleigh_la_SOURCES      = burleigh.c
burleigh_bii_la_SOURCES  = burleigh_bii.c
burleigh_exp_la_SOURCES  = burleigh_exp.c
createc_la_SOURCES       = createc.c
codevfile_la_SOURCES     = codevfile.c
csmfile_la_SOURCES       = csmfile.c
dmefile_la_SOURCES       = dmefile.c
dm3file_la_SOURCES       = dm3file.c
dumbfile_la_SOURCES      = dumbfile.c
ecsfile_la_SOURCES       = ecsfile.c
ezdfile_la_SOURCES       = ezdfile.c
femtoscan_la_SOURCES     = femtoscan.c
femtoscan_txt_la_SOURCES = femtoscan-txt.c
gdeffile_la_SOURCES      = gdeffile.c
gsffile_la_SOURCES       = gsffile.c
gwyfile_la_SOURCES       = gwyfile.c
gxyzffile_la_SOURCES     = gxyzffile.c
hdf4file_la_SOURCES      = hdf4file.c
hitachi_afm_la_SOURCES   = hitachi-afm.c
hitachi_sem_la_SOURCES   = hitachi-sem.c
igorfile_la_SOURCES      = igorfile.c
imgexport_la_SOURCES     = imgexport.c imgexportpreset.h gwytiff.h image-keys.h
intematix_la_SOURCES     = intematix.c gwytiff.h
iso28600_la_SOURCES      = iso28600.c
jeol_la_SOURCES          = jeol.c
jpkscan_la_SOURCES       = jpkscan.c jpk.h gwytiff.h
jspmfile_la_SOURCES      = jspmfile.c
lextfile_la_SOURCES      = lextfile.c gwytiff.h
magellan_la_SOURCES      = magellan.c gwytiff.h
mapvue_la_SOURCES        = mapvue.c
metropro_la_SOURCES      = metropro.c
microprof_la_SOURCES     = microprof.c
miffile_la_SOURCES       = miffile.c
mifile_la_SOURCES        = mifile.c
mulfile_la_SOURCES       = mulfile.c
nanoeducator_la_SOURCES  = nanoeducator.c
nanomagnetics_la_SOURCES = nanomagnetics.c
nanonics_la_SOURCES      = nanonics.c
nanonis_la_SOURCES       = nanonis.c
nanoscan_la_SOURCES      = nanoscan.c
nanoscope_ii_la_SOURCES  = nanoscope-ii.c
nanoscope_la_SOURCES     = nanoscope.c
nanotop_la_SOURCES       = nanotop.c
netcdf_la_SOURCES        = netcdf.c
nrrdfile_la_SOURCES      = nrrdfile.c
nt_mdt_la_SOURCES        = nt-mdt.c
nxiifile_la_SOURCES      = nxiifile.c
oldmda_la_SOURCES        = oldmda.c
ols_la_SOURCES           = ols.c gwytiff.h
ometiff_la_SOURCES       = ometiff.c
omicron_la_SOURCES       = omicron.c
omicronflat_la_SOURCES   = omicronflat.c
omicronmatrix_la_SOURCES = omicronmatrix.c
opdfile_la_SOURCES       = opdfile.c
pixmap_la_SOURCES        = pixmap.c gwytiff.h image-keys.h
pnifile_la_SOURCES       = pnifile.c
pltfile_la_SOURCES       = pltfile.c
psia_la_SOURCES          = psia.c gwytiff.h
pt3file_la_SOURCES       = pt3file.c
quesant_la_SOURCES       = quesant.c
rawfile_la_SOURCES       = rawfile.c rawfilepreset.h
rawgraph_la_SOURCES      = rawgraph.c
rawxyz_la_SOURCES        = rawxyz.c
renishaw_la_SOUCES       = renishaw.c
rhk_sm3_la_SOURCES       = rhk-sm3.c
rhk_sm4_la_SOURCES       = rhk-sm4.c
rhk_spm32_la_SOURCES     = rhk-spm32.c
robotics_la_SOURCES      = robotics.c
s94file_la_SOURCES       = s94file.c
sdfile_la_SOURCES        = sdfile.c
seiko_la_SOURCES         = seiko.c
sensofar_la_SOURCES      = sensofar.c
sensolytics_la_SOURCES   = sensolytics.c
shimadzu_la_SOURCES      = shimadzu.c
sicmfile_la_SOURCES      = sicmfile.c
sis_la_SOURCES           = sis.c
spip_asc_la_SOURCES      = spip-asc.c
spmlab_la_SOURCES        = spmlab.c
spmlabf_la_SOURCES       = spmlabf.c
stmprg_la_SOURCES        = stmprg.c
stpfile_la_SOURCES       = stpfile.c
surffile_la_SOURCES      = surffile.c
tiaser_la_SOURCES        = tiaser.c
tescan_la_SOURCES        = tescan.c
unisoku_la_SOURCES       = unisoku.c
vtkfile_la_SOURCES       = vtkfile.c
win_stm_la_SOURCES       = win_stm.c
wipfile_la_SOURCES       = wipfile.c
witfile_la_SOURCES       = witfile.c
witec_asc_la_SOURCES     = witec-asc.c
wsffile_la_SOURCES       = wsffile.c
wsxmfile_la_SOURCES      = wsxmfile.c
xyzexport_la_SOURCES     = xyzexport.c
zeiss_la_SOURCES         = zeiss.c
zemax_la_SOURCES         = zemax.c

# These compile also without.
createc_la_LIBADD        = @ZLIB@
nrrdfile_la_LIBADD       = @ZLIB@ @BZIP2@
pixmap_la_LIBADD         = @PNG_LIBS@
pixmap_la_CFLAGS         = $(AM_CFLAGS) @PNG_CFLAGS@
imgexport_la_LIBADD      = @PNG_LIBS@
imgexport_la_CFLAGS      = $(AM_CFLAGS) @PNG_CFLAGS@
rhk_sm4_la_LIBADD        = @ZLIB@
hdrimage_la_LIBADD       = @EXR_LIBS@ @PNG_LIBS@
hdrimage_la_CXXFLAGS     = $(AM_CXXFLAGS) @PNG_CFLAGS@ @EXR_CFLAGS@

# These require the respective libraries.
if HAVE_XML2
if HAVE_ZLIB
spml_la_SOURCES          = spml.c spml-utils.h
spml_la_CFLAGS           = $(AM_CFLAGS) @XML2_CFLAGS@
spml_la_LIBADD           = @XML2_LIBS@ @ZLIB@
endif
endif

# These require the respective libraries.
if HAVE_XML2
if HAVE_MINIZIP
apedaxfile_la_SOURCES    = apedaxfile.c gwyminizip.h
apedaxfile_la_CFLAGS     = $(AM_CFLAGS) @XML2_CFLAGS@ @MINIZIP_CFLAGS@
apedaxfile_la_LIBADD     = @XML2_LIBS@ @MINIZIP_LIBS@
endif
endif

if HAVE_ZLIB
#matfile_la_SOURCES       = matfile.c
#matfile_la_LIBADD        = @ZLIB@
endif

if HAVE_MINIZIP
nanoobserver_la_SOURCES  = nanoobserver.c gwyminizip.h
nanoobserver_la_CFLAGS   = $(AM_CFLAGS) @MINIZIP_CFLAGS@
nanoobserver_la_LIBADD   = @MINIZIP_LIBS@

nanoscantech_la_SOURCES  = nanoscantech.c gwyminizip.h
nanoscantech_la_CFLAGS   = $(AM_CFLAGS) @MINIZIP_CFLAGS@
nanoscantech_la_LIBADD   = @MINIZIP_LIBS@

opengps_la_SOURCES  = opengps.c gwyminizip.h
opengps_la_CFLAGS   = $(AM_CFLAGS) @MINIZIP_CFLAGS@
opengps_la_LIBADD   = @MINIZIP_LIBS@

spmxfile_la_SOURCES  = spmxfile.c gwyminizip.h
spmxfile_la_CFLAGS   = $(AM_CFLAGS) @MINIZIP_CFLAGS@
spmxfile_la_LIBADD   = @MINIZIP_LIBS@
endif

if HAVE_CFITSIO
fitsfile_la_SOURCES  = fitsfile.c
fitsfile_la_CFLAGS   = $(AM_CFLAGS) @CFITSIO_CFLAGS@
fitsfile_la_LIBADD   = @CFITSIO_LIBS@
endif

if HAVE_CXX
hdrimage_la_SOURCES  = hdrimage.cc gwytiff.h image-keys.h
endif

clean-local:
	rm -f core.* *~
