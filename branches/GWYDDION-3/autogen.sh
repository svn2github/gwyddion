#!/bin/sh
# vim: set ts=2 sw=2 et:
# NB: This script may actually need bash.  You have been warned.
# You can override the default tool choice by setting AUTOCONF, etc.
# These should be bare executables, options go to AUTOCONF_FLAGS, etc.

# Avoid the exact string that identifies generated files.
generated=GENERATED

# Get required version of a maintainer tool from configure.ac.  This ensures
# the version required here and in configure.ac is the same.
# Expected format: MACRO([VERSION...]) or MACRO(VERSION...)
inherit_reqver() {
  local macro=$1
  local v
  v=$(sed "s/^$macro([[]*\([0-9]*\)\.\([0-9]*\).*/\1 \2/;t;d" configure.ac)
  if test -n "$v"; then
    echo $v
  else
    echo "ERROR: Cannot find $macro in configure.ac!"
    echo "       Check your setup and complain to $project maintainer."
    exit 1
  fi
}

# Get required version of a maintainer tool from configure.ac.  This ensures
# the version required here and in configure.ac is the same.
# Expected format: m4_define([IDENTIFIER],[VERSION...])
inherit_reqver_m4def() {
  local identifier=$1
  local v
  v=$(sed "s/^m4_define(\\[$identifier\\], *\\[\([0-9]*\)\.\([0-9]*\).*/\1 \2/;t;d" configure.ac)
  if test -n "$v"; then
    echo $v
  else
    echo "ERROR: Cannot find $identifier in configure.ac!"
    echo "       Check your setup and complain to $project maintainer."
    exit 1
  fi
}

# Get program version in the form MAJOR.MINOR.  Expected --version output:
# TOOL-NAME (SUITE NAME) VERSION(EXTRA)
# Where SUITE NAME and EXTRA parts are optional.
get_version() {
  local v
  local v2
  v=$($1 --version </dev/null | sed -e '2,$ d' -e 's/ *([^()]*)$//' -e 's/.* \(.*\)/\1/' -e 's/-p[0-9]*//')
  v2=${v#*.}
  echo ${v%%.*}.${v2%%.*}
}

# Check whether a tool is present at least in specified version.  If we do not
# find it, try also TOOL-MAJOR.MINOR (hello BSD).  Note cmd and othercmds are
# names of variables, i.e. indirect references, as we may need to change the
# variables contents to the versioned name.
# TODO: Use compgen or a similar mechanism (if available) to find also newer
# tools named TOOL-MAJOR.MINOR, not just the exact version.
check_tool() {
  local name=$1
  local cmd=$2
  local othercmds="$3"
  local reqmajor=$4
  local reqminor=$5
  local url="$6"
  local diewhy=

  local cmdcmd="${!cmd}"
  local cmdcmdver="$cmdcmd-$reqmajor.$reqminor"

  if $cmdcmd --version </dev/null >/dev/null 2>&1; then
    cmdcmdver=
  elif $cmdcmdver --version </dev/null >/dev/null 2>&1; then
    cmdcmd="$cmdcmdver"
    eval $cmd=\"$cmdcmd\"
  else
    diewhy=cmd
    cmdcmd=
  fi

  if test -n "$cmdcmd"; then
    ver=$(get_version $cmdcmd)
    vermajor=${ver%%.*}
    verminor=${ver##*.}
    if test "$vermajor" -lt $reqmajor || test "$vermajor" = $reqmajor -a "$verminor" -lt $reqminor; then
      diewhy=version
    else
      for othercmd in $othercmds; do
        othercmdcmd=${!othercmd}
        if test -n "$cmdcmdver"; then
          othercmdcmd="$othercmdcmd-$reqmajor.$reqminor"
          eval $othercmd=\"$othercmdcmd\"
        fi
        if $othercmdcmd --version </dev/null >/dev/null 2>&1; then
          otherver=$(get_version "$othercmdcmd")
          if test "$otherver" != "$ver"; then
            diewhy=otherversion
            break
          else
            :
          fi
        else
          diewhy=othercmd
          break
        fi
      done
    fi
  fi

  if test -n "$diewhy"; then
    echo "ERROR: $name at least $reqmajor.$reqminor is required to bootstrap $project."
    case $diewhy in
      version) echo "       You have only version $ver of $name installed.";;
      othercmd) echo "       It should also install command \`$othercmd' which is missing.";;
      otherversion) echo "       The version of \`$othercmd' differs from $cmd: $otherver != $ver.";;
      cmd) ;;
      *) echo "       *** If you see this, shoot the $project maintainer! ***";;
    esac
    echo "       Install the appropriate package for your operating system,"
    echo "       or get the source tarball of $name at"
    echo "       $url"
    echo
    DIE=1
  else
    echo "$name $ver: OK"
  fi
}

get_rid_of_mkdir_p() {
  echo 'Getting rid of $(mkdir_p)'
  sed -i -e 's/AM_PROG_MKDIR_P/AC_PROG_MKDIR_P/g' m4/intl.m4 m4/po.m4
  sed -i -e 's/\$(mkdir_p)/$(MKDIR_P)/g' po*/Make*
  sed -i -e 's/^mkdir_p *=.*/MKDIR_P = @MKDIR_P@/g' po*/Make*
}

if test -z "$*"; then
  echo "Note: To pass extra options to ./configure, specify them on the command line"
fi

DIE=0
project=Gwyddion
podirs="po po-libgwy po-libgwyui"

# Default tools.
ACLOCAL=${ACLOCAL:-aclocal}
AUTOCONF=${AUTOCONF:-autoconf}
AUTOHEADER=${AUTOHEADER:-autoheader}
AUTOMAKE=${AUTOMAKE:-automake}
AUTOPOINT=${AUTOPOINT:-autopoint}
GTKDOCIZE=${GTKDOCIZE:-gtkdocize}
INTLTOOLIZE=${INTLTOOLIZE:-intltoolize}
LIBTOOLIZE=${LIBTOOLIZE:-libtoolize}
XGETTEXT=${XGETTEXT:-xgettext}
CONFIGURE_FLAGS="--enable-gtk-doc"

# Get required versions from configure.ac
AUTOCONF_REQVER=$(inherit_reqver AC_PREREQ)
AUTOMAKE_REQVER=$(inherit_reqver AM_INIT_AUTOMAKE)
GETTEXT_REQVER=$(inherit_reqver_m4def gwy_required_gettext)
GTKDOC_REQVER=$(inherit_reqver_m4def gwy_required_gtkdoc)
INTLTOOL_REQVER=$(inherit_reqver_m4def gwy_required_intltool)
LIBTOOL_REQVER=$(inherit_reqver LT_PREREQ)

# Check tool versions
check_tool Autoconf "AUTOCONF"    "AUTOHEADER" $AUTOCONF_REQVER ftp://ftp.gnu.org/pub/gnu/autoconf/
check_tool Automake "AUTOMAKE"    "ACLOCAL"    $AUTOMAKE_REQVER ftp://ftp.gnu.org/pub/gnu/automake/
check_tool Gettext  "XGETTEXT"    "AUTOPOINT"  $GETTEXT_REQVER  ftp://ftp.gnu.org/pub/gnu/gettext/
check_tool Gtk-doc  "GTKDOCIZE"   ""           $GTKDOC_REQVER   ftp://ftp.gnome.org/pub/gnome/sources/gtk-doc
check_tool Intltool "INTLTOOLIZE" ""           $INTLTOOL_REQVER ftp://ftp.gnome.org/pub/gnome/sources/intltool
check_tool Libtool  "LIBTOOLIZE"  ""           $LIBTOOL_REQVER  ftp://ftp.gnu.org/pub/gnu/libtool/

# XXX: Getting introspection.m4 is difficult.  Hard-depend on g-ir-devel now.
if test -n "$ACLOCAL"; then
  ac_dir="$($ACLOCAL $ACLOCAL_FLAGS --print-ac-dir)"
  if test -s "$ac_dir/introspection.m4"; then
    echo "GObject-introspection-devel: OK"
  else
    echo "ERROR: GObject-introspection-devel is required to bootstrap $project."
    echo "       Install the appropriate package for your operating system,"
    echo "       or get the source tarball of GObject-introspection at"
    echo "       http://ftp.gnome.org/pub/GNOME/sources/gobject-introspection/"
    echo "       Or complain if"
    echo "       https://bugzilla.gnome.org/show_bug.cgi?id=615518"
    echo "       has been reasonably fixed."
    echo
    DIE=1
  fi
else
  echo "GObject-introspection-devel: who knows"
fi

if test "$DIE" = 1; then
  exit 1
fi

if test -x ./build/update-POTFILES.in.sh; then
  ./build/update-POTFILES.in.sh
fi

(
  run() { echo "$@"; "$@"; }
  set -e
  run $AUTOPOINT --force $AUTOPOINT_FLAGS
  run $INTLTOOLIZE --automake --force $INTLTOOLIZE_FLAGS
  run $LIBTOOLIZE --automake --force $LIBTOOLIZE_FLAGS
  run $GTKDOCIZE --docdir docs --flavour no-tmpl $GTKDOCIZE_FLAGS
  run $ACLOCAL --force --install -I m4 $ACLOCAL_FLAGS
  get_rid_of_mkdir_p
  run $AUTOCONF --force $AUTOCONF_FLAGS
  run $AUTOHEADER --force $AUTOHEADER_FLAGS
  run $AUTOMAKE --add-missing --force-missing $AUTOMAKE_FLAGS
  ) || {
    echo "ERROR: Re-generating failed."
    echo "       See above errors and complain to $project maintainer."
    exit 1
  }

# These seem of no use.
rm -f intltool-extract.in intltool-merge.in intltool-update.in
# We have our own gtk-doc.makefile, thanks.
rm -f docs/gtk-doc.make
# autopoint also adds heaps of useless files.
for subdir in $podirs; do
  (
    cd $subdir && rm -f boldquot.sed en@boldquot.header en@quot.header \
                        insert-header.sin Makevars.template quot.sed \
                        remove-potcdate.sin Rules-quot
  )
  domain=gwyddion3${subdir#po}
  if test $subdir != po; then
    rm -f $subdir/Makefile.in.in
    sed -e "s#^\\(GETTEXT_PACKAGE = \\)@GETTEXT_PACKAGE@#\\1$domain#" \
        -e "s#^\\(subdir = \\)po#\\1$subdir#" \
        po/Makefile.in.in >$subdir/Makefile.in.in
  else
    rm -f $subdir/Makefile.in.in~
  fi
  echo "# This is a $generated file.  Created by autogen.sh" >$subdir/Makevars
  sed -e "s#@domain@#$domain#" -e "s#@subdir@#$subdir#" \
      build/Makevars.in >>$subdir/Makevars
done

# Finally we can run configure
echo ./configure $CONFIGURE_FLAGS "$@"
./configure $CONFIGURE_FLAGS "$@" || exit 1

# This has to be done somewhere.
for subdir in $podirs; do
  domain=gwyddion3${subdir#po}
  make -C $subdir $domain.pot
done
