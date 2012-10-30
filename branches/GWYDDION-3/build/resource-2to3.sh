#!/bin/bash
if test -z "$1"; then
    echo "$0 GWY2RESOURCE >GWY3RESOURCE" 1>&2
    exit
fi
if head -n 1 "$1" | grep -q -s '^Gwyddion3 resource'; then
    echo "$0: File $1 is already a Gwyddion 3 resource" 1>&2
    exit
fi
if ! head -n 1 "$1" | grep -q -s '^Gwyddion resource'; then
    echo "$0: File $1 is not a Gwyddion 2 resource" 1>&2
    exit 1
fi
name=$(basename "$1")
sed -n '1{s/^Gwyddion resource/Gwyddion3 resource/;p}' "$1"
echo "name $name"
sed '1d' "$1"
