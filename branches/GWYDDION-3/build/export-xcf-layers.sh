#!/bin/bash
if test -z "$GIMP_CONSOLE"; then
    GIMP_CONSOLE=gimp-console
fi
if test -n "$srcdir"; then
    srcdir="srcdir='$srcdir';"
fi
if test -n "$destdir"; then
    destdir="destdir='$destdir';"
fi
cfgfile="cfgfile='$1';"
driver=$(dirname "$0")'/export-xcf-layers.py'

# gimp_quit() is a separate command because it somehow makes gimp-console more
# likely to terminate if the first batch command fails.
$GIMP_CONSOLE --no-interface --console-messages \
    --no-data --no-fonts --no-splash \
    --batch-interpreter=python-fu-eval \
    --batch="$cfgfile$srcdir$destdir execfile('$driver')" \
    --batch='pdb.gimp_quit(0)'
