#!/bin/bash
# $1: if equal to HEAD, generates one-level deeper PHP inclusions and adds
#     (HEAD) to titles
shopt -s nullglob
set -e

version="$1"
basedir=$HOME/Projects/Gwyddion
srcdir=$basedir/devel-docs
libs="libgwyddion libgwyprocess libgwydraw libgwydgets libgwymodule libgwyapp"

cd $srcdir
rm -f .releaseme

xrefdirs=$(echo $libs | sed 's#[^ ]\+#--extra-dir=../\0/xhtml#g')

pushd() {
  builtin pushd "$@" >/dev/null
}

popd() {
  builtin popd "$@" >/dev/null
}

setup() {
  local name=$1
  local xmldir=$basedir/gwyddion/devel-docs/$name
  local driverfile=$name-docs.sgml
  local x

  rm -rf $name
  mkdir $name

  pushd $name
  ln -s $xmldir/xml .
  ln -s $xmldir/$driverfile .
  for x in $xmldir/*.xml; do
    ln -s $x .
  done
  popd
}

build() {
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

fix() {
  local name=$1
  local x y

  pushd $name
  gtkdoc-fixxref --module-dir=xhtml $xrefdirs

  pushd xhtml
  for x in *.html; do
    y=$(echo "$x" | sed 's/html$/php/')
    xsltproc $srcdir/fixme.xsl $x | $srcdir/fixme.pl $y $version >$y
    rm -f $x
  done
  popd

  popd
}

finalize() {
  local name=$1
  local xmldir=$basedir/gwyddion/devel-docs/$name
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
