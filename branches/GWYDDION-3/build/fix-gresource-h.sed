#!/usr/bin/sed -f
s/^extern /G_GNUC_INTERNAL /
s/__RESOURCE_\(.*__\)/__GWY_RESOURCE\U\1/
s/ (/(/
1{s#^#/* This is a @@GENERATED@@ file. */\n#
s/@@GENERATED@@/GENERATED/
}
2i\
/*<private_header>*/
