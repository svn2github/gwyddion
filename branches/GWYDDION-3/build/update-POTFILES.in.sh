#!/bin/sh

outfile=$1
shift

# Keep the `GENERATED' string separated to prevent match here
G=GENERATED
cat >$outfile.new <<EOF
# List of source files containing translatable strings.
# This is a $G file, by build/update-POTFILES.in.sh.
EOF

for dir in "$@"; do
  echo >>$outfile.new
  echo "# $dir" >>$outfile.new
  find $dir -name \*.\[ch\] \
    | xargs egrep -l '\<[QNC]?_\(|\<[a-z_]{,5}gettext\(' \
    | sort >>$outfile.new
done

if test -f $outfile && diff -q $outfile $outfile.new >/dev/null; then
  rm -f $outfile.new
else
  mv -f $outfile.new $outfile
fi
