#!/bin/sh
if test -z "$1"; then
  echo "$0 FILES..."
  echo "Sets PNG pHYS chunks to 4724 px/m (cca 120 dpi)"
  echo "Warning: Performed via PNM conversion, auxiliary chunks are lost!"
  exit 0
fi

set -e
for x in "$@"; do
  pngtopnm "$x" >"$x.pnm"
  pnmtopng -size '4724 4724 1' "$x.pnm" >"$x.png.tmp"
  if cmp -s "$x" "$x.png.tmp"; then
    echo "$x: already OK"
    rm -f "$x.png.tmp"
  else
    echo "$x: fixed"
    mv -f "$x.png.tmp" "$x.png"
  fi
  rm -f "$x.pnm"
done
