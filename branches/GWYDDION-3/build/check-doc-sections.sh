#!/bin/bash
name=$(basename $(pwd))
defined_sections="$(sed 's# *<FILE>\(.*\)</FILE> *#\1#;t;d' $name-sections.txt)"
used_sections="$(sed 's# *<xi:include  *href="xml/\(.*\)\.xml" */> *#\1#;t;d' $name.xml)"
unused_sections=$({
    echo "$defined_sections"
    echo "$used_sections"
    echo "$used_sections"
} | sort | uniq -u | grep -v '^types$')
if test -n "$unused_sections"; then
    echo "Unused sections in $name: $unused_sections" 1>&2
    exit 1
fi
