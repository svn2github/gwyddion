#!/bin/bash
compressed=
if test "x$1" = "x-c"; then
    compressed=' compressed="true"'
    shift
fi

if test -z "$1"; then
    echo "$0: PATH FILENAME..." 1>&2
    exit 1
fi

path="$1"
shift

cat <<EOF
<?xml version="1.0" encoding="UTF-8"?>
<gresources>
  <gresource prefix="$path">
EOF

for filename; do
    echo "    <file$compressed>$filename</file>"
done

cat <<EOF
  </gresource>
</gresources>
EOF
