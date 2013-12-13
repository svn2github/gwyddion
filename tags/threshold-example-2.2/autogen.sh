#!/bin/sh
test -d m4 || mkdir m4
libtoolize --copy --install --force
aclocal -I m4
autoheader --force
automake --add-missing --copy --force-missing
autoconf --force
./configure "$@"
