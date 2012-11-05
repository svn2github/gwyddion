#!/usr/bin/sed -f
s/^extern /G_GNUC_INTERNAL /
s/ (/(/
s/) };/), NULL, NULL, NULL };/
1i\
/* This is a FOO file. */
1s/FOO/GENERATED/
