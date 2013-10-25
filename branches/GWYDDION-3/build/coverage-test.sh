#!/bin/bash
function extract_table() {
  name="$1"
  shift
  sed -n '# Extract tabelated data from gcov -f output
s/^'$name' //
T
N
s/IA__//g
s/\n/ /
s/'"'"'//g
/^\//d
s/Lines executed://
s/% of//
s/^\([^ ]*\) \([^ ]*\)/\2 \1/
/^No/d
p' "$@"
}

function extract_funcs() {
  extract_table Function "$@" | sort -g -r
}

function extract_files() {
  extract_table File "$@" | grep -v ' \.\./libgwy[^ ]\.h' | sort -g -r
}

function print_header() {
cat <<EOF
<tr class='head'>
  <th>$1</th>
  <th>Lines</th>
  <th>Missing</th>
  <th>Coverage</th>
  <th>Graph</th>
</tr>
EOF
}

set -e
outdir=coverage
libname=$(sed -e 's/^library *= *//;t;d' Makefile)
if test -z "$libname"; then
  echo "Cannot determine library name from Makefile!"
  exit 1
fi
testprog=$(sed -e 's/^test_program *= *//;t;d' Makefile)
if test -z "$testprog"; then
  echo "Cannot determine test program name from Makefile!"
  exit 1
fi
plot=$(dirname $(readlink -f "$0"))/coverage-plot.py
if test ! -x "$plot"; then
  echo "Cannot find coverage-plot.py!"
  exit 1
fi
report=$outdir/coverage.html
make clean
make CFLAGS="-O0 --coverage -pg" LDFLAGS="--coverage"
./$testprog
rm -rf $outdir
mkdir $outdir
for x in *.c; do
  b=${x%.c}
  test -f .libs/$b.gcno || continue
  gcov -f -o .libs $x >$outdir/$x.func 2>coverage.tmp
  if grep -s -q 'no functions found' coverage.tmp; then
    rm -f $x.gcov $outdir/$x.func
  else
    mv $x.gcov $outdir/
    extract_funcs $outdir/$x.func >$outdir/$x.dat
  fi
done
rm -f coverage.tmp
extract_files $outdir/*.func >$outdir/files.dat

cat >$report <<EOF
<!DOCTYPE html PUBLIC "-//W3C//DTD XHTML 1.0 Strict//EN"
                      "http://www.w3.org/TR/xhtml1/DTD/xhtml1-strict.dtd">
<html xmlns="http://www.w3.org/1999/xhtml" xml:lang="en">
<head>
<meta http-equiv="Content-Type" content="text/html; charset=UTF-8"/>
<meta name="Author" content="David Nečas (Yeti)"/>
<title>Test Coverage Statistics for $libname</title>
<style type="text/css"><!--
  .good { background-color: #f7d48e; }
  .bad { background-color: #5b1508; }
  table.stats { border-collapse: collapse; }
  table.stats tr.head { background-color: #ab2e12; }
  table.stats tr.head th { color: white; }
  table.stats tr.odd { background-color: #fcedcf; }
  table.stats td { text-align: right; }
  table.stats td.left { text-align: left; }
  table.stats td { padding: 0px 6px; }
  table.stats th { padding: 2px 6px; }
--></style>
</head>
<body>
<h1>Test Coverage Statistics for $libname</h2>
<table summary="Coverage statistics" class="stats">
EOF
echo -n "FILES "
print_header File >>$report
python $plot $libname <$outdir/files.dat | sed 's/\([-a-z0-9]*\)\.c/<a href="#\1">\0\<\/a>/'>>$report
for x in $outdir/*.c.dat; do
  b=${x%.c.dat}
  b=${b#*/}
  echo -n "$b "
  print_header Function | sed 's/<tr/<tr id="'$b'"/' >>$report
  python $plot $b.c <$x >>$report
done
echo
print_header Function >>$report
cat >>$report <<EOF
</table>
</body>
</html>
EOF

make clean
echo "See coverage/coverage.html"
