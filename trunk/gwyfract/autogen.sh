#!/bin/sh
libtoolize && aclocal -I m4 && autoheader && automake && autoconf
