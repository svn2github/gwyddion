AC_INIT([Gwyddion user guide], [2009-07-31], [klapetek@gwyddion.net])
AC_CONFIG_SRCDIR(xhtml.xsl)
AC_CONFIG_FILES(\
  Makefile \
  user-guide.iss \
  formulas/Makefile \
  formulas/render.sh \
)

PACKAGE_SHORTNAME=user-guide
AC_SUBST(PACKAGE_SHORTNAME)

AC_PROG_CC

AC_CHECK_PROGS([GMAKE],[gmake make],[no])
if test "$GMAKE" != no; then
  AC_MSG_CHECKING([whether $GMAKE is GNU make])
  if $GMAKE --version 2>/dev/null | grep '^GNU Make' >/dev/null; then
    AC_MSG_RESULT([seems so])
  else
    AC_MSG_RESULT([no])
    AC_MSG_ERROR([GNU make is required to build user guide
                  Get it at http://www.gnu.org/software/make/])
  fi
fi

AC_CHECK_PROGS([GSED],[gsed sed],[no])
if test "$GSED" != no; then
  AC_MSG_CHECKING([whether $GSED is GNU sed])
  if $GSED --version 2>/dev/null | grep '^GNU sed' >/dev/null; then
    AC_MSG_RESULT([seems so])
  else
    AC_MSG_RESULT([no])
    AC_MSG_ERROR([GNU sed is required to build the user guide.
                  Get it at http://ftp.gnu.org/pub/gnu/sed/])
  fi
fi

JH_CHECK_XML_CATALOG([-//OASIS//DTD DocBook XML V4.5//EN],
                     [DocBook XML DTD V4.5])

GWY_HTML_MISSING=
GWY_PDF_MISSING=
GWY_PNG_MISSING=
GWY_SVG_MISSING=
GWY_MAN_MISSING=

AC_CHECK_PROGS([CONVERT],[convert],[no])
if test "$CONVERT" = no; then
  GWY_HTML_MISSING="${GWY_HTML_MISSING}  convert, get ImageMagick from http://www.imagemagick.org/
"
fi

AC_CHECK_PROGS([XSLTPROC],[xsltproc],[no])
if test "$XSLTPROC" = no; then
  GWY_HTML_MISSING="${GWY_HTML_MISSING}  xsltproc, get libxml2 from http://xmlsoft.org/
"
  GWY_PDF_MISSING="${GWY_PDF_MISSING}  xsltproc, get libxml2 from http://xmlsoft.org/
"
  GWY_MAN_MISSING="${GWY_MAN_MISSING}  xsltproc, get libxml2 from http://xmlsoft.org/
"
fi

AC_CHECK_PROGS([TEX],[tex],[no])
if test "$TEX" = no; then
  GWY_HTML_MISSING="${GWY_HTML_MISSING}  tex, get TeX from http://tug.org/
"
fi

AC_CHECK_PROGS([PDFLATEX],[pdflatex],[no])
if test "$TEX" = no; then
  GWY_PDF_MISSING="${GWY_HTML_MISSING}  pdflatex, get pdfLaTeX from http://tug.org/
"
fi

AC_CHECK_PROGS([DVIPS],[dvips],[no])
if test "$DVIPS" = no; then
  GWY_HTML_MISSING="${GWY_HTML_MISSING}  dvips, get dvips from http://tug.org/
"
fi

AC_CHECK_PROGS([GS],[gs],[no])
if test "$GS" = no; then
  GWY_HTML_MISSING="${GWY_HTML_MISSING}  gs, get ghostscript from http://www.ghostscript.com/
"
fi

# Here we assume if someone has xhtml he has manpages too.
JH_CHECK_XML_CATALOG([http://docbook.sourceforge.net/release/xsl/current/xhtml/chunk.xsl],
                     [DocBook XSL Stylesheets],
                     [],[GWY_HTML_MISSING="${GWY_HTML_MISSING}  DocBook XSL Stylesheets, get them from http://docbook.sourceforge.net/projects/xsl/
"])

if test -z "$GWY_HTML_MISSING"; then
  AC_MSG_CHECKING([whether DocBook XSL Stylesheets version is >= 1.70])
  cat >conftest.xml <<_ACEOF
<?xml version='1.0'?>
<!DOCTYPE book PUBLIC '-//OASIS//DTD DocBook XML V4.5//EN' 'http://www.oasis-open.org/docbook/xml/4.5/docbookx.dtd'>
<book/>
_ACEOF
  if GWY_RUN_LOG($XSLTPROC --nowrite docbook-xsl-version.xsl conftest.xml); then
    AC_MSG_RESULT([yes])
  else
    AC_MSG_RESULT([no])
    GWY_HTML_MISSING="${GWY_HTML_MISSING}  DocBook XSL Stylesheets are too old, get them from http://docbook.sourceforge.net/projects/xsl/
"
  fi
  rm -f conftest.xml
fi

AC_CHECK_PROGS([DBLATEX],[dblatex],[no])
if test "$DBLATEX" = no; then
  GWY_PDF_MISSING="${GWY_PDF_MISSING}  dblatex, get DBLaTeX from http://dblatex.sourceforge.net/
"
fi

AC_CHECK_PROGS([INKSCAPE],[inkscape],[no])
if test "$INKSCAPE" = no; then
  GWY_SVG_MISSING="whatever"
  GWY_HTML_MISSING="${GWY_HTML_MISSING}  inkscape, get Inkscape from http://www.inkscape.org/
"
  GWY_PDF_MISSING="${GWY_PDF_MISSING}  inkscape, get Inkscape from http://www.inkscape.org/
"
fi

PKG_CHECK_MODULES([PNG],[libpng],,[
  GWY_PNG_MISSING="${GWY_PNG_MISSING}  libpng, get libpng from http://www.libpng.org/pub/png/
"
])

GWY_CONDITIONAL([HTML],[test x = "x$GWY_HTML_MISSING"])
GWY_CONDITIONAL([PDF],[test x = "x$GWY_PDF_MISSING"])
GWY_CONDITIONAL([PNG],[test x = "x$GWY_PNG_MISSING"])
GWY_CONDITIONAL([SVG],[test x = "x$GWY_SVG_MISSING"])
GWY_CONDITIONAL([MAN],[test x = "x$GWY_MAN_MISSING"])

AC_OUTPUT

if test x != "x$GWY_HTML_MISSING"; then
AC_MSG_WARN([Cannot build HTML, the following tools were not found:
$GWY_HTML_MISSING])
fi

if test x != "x$GWY_PDF_MISSING"; then
AC_MSG_WARN([Cannot build PDF, the following tools were not found:
$GWY_PDF_MISSING])
fi

if test x != "x$GWY_MAN_MISSING"; then
AC_MSG_WARN([Cannot build man pages, the following tools were not found:
$GWY_MAN_MISSING])
fi

if test x != "x$GWY_PNG_MISSING"; then
AC_MSG_WARN([Cannot build fixpngres, the following tools were not found:
$GWY_PNG_MISSING])
fi

echo "Run '$GMAKE help' for some help."
