#!/bin/bash
# $1: if equal to HEAD, generates one-level deeper PHP inclusions and adds
#     (HEAD) to titles
export PATH="$HOME/bin:$PATH"
shopt -s nullglob
set -e

# Check wheter we are called from the right dir
test -f gtk-doc.xsl
test -f fixme.pl
test -f fixme.xsl

version="$1"
srcdir=$(pwd)
docbase=${docbase:-$HOME/Projects/Gwyddion/gwyddion-trunk/devel-docs}
libs="libgwyddion libgwyprocess libgwydraw libgwydgets libgwymodule libgwyapp"

rm -f .releaseme

xrefdirs=$(echo $libs | sed 's#[^ ]\+#--extra-dir=../\0/xhtml#g')

function pushd() {
  builtin pushd "$@" >/dev/null
}

function popd() {
  builtin popd "$@" >/dev/null
}

# Make symlinks to project's documentation xml dirs (i.e. DocBook sources)
function setup() {
  local name=$1
  local xmldir=$docbase/$name
  local driverfile=$name-docs.sgml
  local x

  rm -rf $name
  mkdir $name

  pushd $name
  ln -s $xmldir/xml .
  ln -s $xmldir/$driverfile .
  ln -s $xmldir/$name-sections.txt .
  for x in $xmldir/*.xml; do
    ln -s $x .
  done
  popd
}

# Compile DocBook to XHTML
function build() {
  local name=$1
  local driverfile=$name-docs.sgml

  pushd $name

  mkdir xhtml
  pushd xhtml
  xsltproc --nonet --xinclude --stringparam gtkdoc.bookname $name \
    $srcdir/gtk-doc.xsl ../$driverfile
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

  pushd $name
  gtkdoc-fixxref --module=$name --module-dir=xhtml $xrefdirs

  pushd xhtml
  for x in *.html; do
    y=$(echo "$x" | sed 's/html$/php/')
    xsltproc --nonet $srcdir/fixme.xsl $x | $srcdir/fixme.pl $y $version >$y
    rm -f $x
  done
  popd

  popd
}

# Shuffle things to gwyddion.net layout
function finalize() {
  local name=$1
  local xmldir=$docbase/$name
  local x

  mv $name $name.tmp
  mv $name.tmp/xhtml $name
  rm -rf $name/index.sgml $name.tmp

  pushd $name
  x=$(echo $xmldir/html/*.png)
  if test -n "$x"; then
    cp $x .
  fi
  rm -f {home,up,down,left,right,prev,next}.png
  popd
}

for lib in $libs; do setup $lib; done
for lib in $libs; do build $lib; done
for lib in $libs; do fix $lib; done
for lib in $libs; do finalize $lib; done

touch .releaseme
