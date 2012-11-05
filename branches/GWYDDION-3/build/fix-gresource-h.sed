#!/usr/bin/sed -f
s/^extern /G_GNUC_INTERNAL /
s/__RESOURCE_\(.*__\)/__GWY_RESOURCE\U\1/
s/ (/(/
1i\
/* This is a FOO file. */
1s/FOO/GENERATED/
1i\
/*<private_header>*/
