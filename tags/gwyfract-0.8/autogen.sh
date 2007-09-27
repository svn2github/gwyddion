#!/bin/sh
libtoolize --automake --force \
  && aclocal -I m4 \
  && autoheader \
  && automake --add-missing \
  && autoconf \
  && ./configure "$@"
