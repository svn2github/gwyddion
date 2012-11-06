#!/usr/bin/sed -f
s/^extern /G_GNUC_INTERNAL /
s/ (/(/
s/) };/), NULL, NULL, NULL };/
1{s#^#/* This is a @@GENERATED@@ file. */\n#
s/@@GENERATED@@/GENERATED/
}
