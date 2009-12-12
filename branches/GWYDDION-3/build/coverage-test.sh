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
  extract_table File "$@" | sort -g -r
}

function print_header() {
cat <<EOF
<tr>
  <th>$1</th>
  <th>Lines</th>
  <th>Coverage</th>
  <th>Graph</th>
</tr>
EOF
}

if test $# != 3; then
  echo Usage: coverage-test.sh LIBNAME PROGNAME PLOT-SCRIPT
  exit 0
fi

set -e
outdir=coverage
libname="$1"
testprog="$2"
plot="$3"
report=$outdir/coverage.html
make clean
make CFLAGS="-O0 --coverage -pg" LDFLAGS="--coverage" $testprog
./$testprog
mkdir $outdir
for x in *.c; do
  b=${x%.c}
  test -f .libs/$b.gcno || continue
  gcov -f -o .libs $x >$outdir/$x.func
  mv $x.gcov $outdir/
  extract_funcs $outdir/$x.func >$outdir/$x.dat
done
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
  table.stats td { text-align: right; }
  table.stats td, table.stats th { padding: 0px 6px; }
--></style>
</head>
<body>
<h1>Test Coverage Statistics for $libname</h2>
<table summary="Coverage statistics" class="stats">
EOF
print_header File >>$report
$plot <$outdir/files.dat | sed 's/\([-a-z0-9]*\)\.c/<a href="#\1">\0\<\/a>/'>>$report
for x in $outdir/*.c.dat; do
  b=${x%.c.dat}
  b=${b#*/}
  print_header Function | sed 's/<tr/<tr id="'$b'"/' >>$report
  $plot <$x >>$report
done
print_header Function >>$report
cat >>$report <<EOF
</table>
</body>
</html>
EOF

rm -vf *.gcov gmon.out *.gcda *.gcno
