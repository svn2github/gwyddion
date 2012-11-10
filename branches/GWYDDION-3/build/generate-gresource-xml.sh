#!/bin/bash
compressed=
preprocess=
while test "x${1:0:1}" == 'x-'; do
    case "$1" in
        -c) compressed=' compressed="true"' ;;
        -p) preprocess=' preprocess="to-pixdata"' ;;
        *) echo "Unknown option $1" 1>&2 ;;
    esac
    shift
done

if test -z "$1"; then
    echo "$0 [-c|-p] PATH FILENAME..." 1>&2
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
    echo "    <file$compressed$preprocess>$filename</file>"
done

cat <<EOF
  </gresource>
</gresources>
EOF
