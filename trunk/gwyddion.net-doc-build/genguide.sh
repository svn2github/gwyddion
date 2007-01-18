#!/bin/bash
# $1: if equal to HEAD, generates one-level deeper PHP inclusions and adds
#     (HEAD) to titles
shopt -s nullglob
set -e

# Check wheter we are called from the right dir
test -f user-guide.xsl
test -f fixme.pl
test -f fixme.xsl

version="$1"
srcdir=$(pwd)
docbase=${docbase:-$HOME/Projects/Gwyddion/user-guide}

rm -f .releaseme

function pushd() {
  builtin pushd "$@" >/dev/null
}

function popd() {
  builtin popd "$@" >/dev/null
}

# Make symlinks to project's documentation xml dirs (i.e. DocBook sources)
function setup() {
  local name=$1
  local xmldir=$docbase
  local driverfile=$name.xml
  local x

  rm -rf $name
  mkdir $name

  pushd $name
  ln -s $xmldir/xml .
  popd
}

# Compile DocBook to XHTML
function build() {
  local name=$1
  local driverfile=$name.xml

  pushd $name

  mkdir xhtml
  pushd xhtml
  xsltproc --nonet --xinclude $srcdir/user-guide.xsl ../xml/$driverfile
  popd

  popd
}

# Fix cross-references with gtkdoc-fixxref,
# fix cross-references again to absolute WWW URLs,
# and do things that is easier to do with another processing stage than
# directly in gtk-doc.xsl
function fix() {
  local name=$1
  local x y

  pushd $name/xhtml

  for x in *.html; do
    y=$(echo "$x" | sed 's/html$/php/')
    xsltproc --nonet $srcdir/fixme.xsl $x | $srcdir/fixme.pl $y $version >$y
    rm -f $x
  done

  popd
}

# Shuffle things to gwyddion.net layout
function finalize() {
  local name=$1
  local x

  mv $name $name.tmp
  mv $name.tmp/xhtml $name
  rm -rf $name.tmp

  pushd $name
  #x=$(echo $docbase/images/*.png)
  #if test -n "$x"; then
  #  cp $x .
  #fi
  rm -f {home,up,down,left,right,prev,next}.png
  popd
}

setup user-guide
build user-guide
fix user-guide
finalize user-guide

touch .releaseme
