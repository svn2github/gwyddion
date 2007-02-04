AC_INIT([Gwyddion user guide], [2007-02-04], [klapetek@gwyddion.net])
AC_CONFIG_SRCDIR(xhtml.xsl)
AC_CONFIG_FILES(\
  Makefile \
  user-guide.iss \
  formulas/Makefile \
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
    AC_MSG_ERROR([GNU make is required to build user guide])
  fi
fi

AC_CHECK_PROGS([CONVERT],[convert],[no])
if test "$CONVERT" = no; then
  AC_MSG_ERROR([convert is required to build user guide (install ImageMagic)])
fi

AC_CHECK_PROGS([XSLTPROC],[xsltproc],[no])
if test "$XSLTPROC" = no; then
  AC_MSG_ERROR([xsltproc is required to build user guide (install libxml2)])
fi

AC_CHECK_PROGS([TEX],[tex],[no])
if test "$TEX" = no; then
  AC_MSG_ERROR([TeX is required to build user guide])
fi

AC_CHECK_PROGS([DVIPS],[dvips],[no])
if test "$DVIPS" = no; then
  AC_MSG_ERROR([dvips is required to build user guide])
fi

AC_CHECK_PROGS([GS],[gs],[no])
if test "$GS" = no; then
  AC_MSG_ERROR([gs is required to build user guide (install ghostscript)])
fi

JH_CHECK_XML_CATALOG([-//OASIS//DTD DocBook XML V4.2//EN],
                     [DocBook XML DTD V4.2])
JH_CHECK_XML_CATALOG([http://docbook.sourceforge.net/release/xsl/current/html/chunk.xsl],
                     [DocBook XSL Stylesheets])

AC_CHECK_PROGS([PDFXMLTEX],[pdfxmltex],[no])
AC_CHECK_PROGS([LYNX],[lynx],[no])

AC_OUTPUT

echo "Run '$GMAKE help' for some help."
