#!/bin/sh
# vim: set ts=2 sw=2 et:
# Work around intltool inability to deal with more directories.  We put files
# into POTFILES.in in one po directory and to POTFILES.skip in all others.

# All po directories
subdirs="po po-libgwy po-libgwyui"

# Directories to scan
# PO-DIRECTORY:SOURCE-CODE-DIRECTORY
# We do not have any translatable content for the main domain.
POTFILES_map="
po-libgwy:libgwy
po-libgwyui:libgwyui
"

# Files to ignore everywhere
POTFILES_ignore="
"

# Files to add to specific po directories
# PO-DIRECTORY:FILE-NAME
POTFILES_extra="
"

# Keep the `GENERATED' string separated to prevent match here
G=GENERATED

# 1. Make headers
for subdir in $subdirs; do
  cat >$subdir/POTFILES.in.new <<EOF
# List of source files containing translatable strings.
# This is a $G file.  Created by build/update-POTFILES.in.sh.

EOF
  cat >$subdir/POTFILES.skip.new <<EOF
# List of source files to ignore when looking for translatable strings.
# This is a $G file.  Created by build/update-POTFILES.in.sh.

EOF
done

# 2. Scan the sources
for m in $POTFILES_map; do
  podir=${m%:*}
  srcdir=${m#*:}
  files="$(find $srcdir -name \*.\[ch\] \
           | xargs egrep -l '\<[QNC]?_\(|\<[a-z_]{,5}gettext\(' \
           | sort)"
  for subdir in $subdirs; do
    echo "# $srcdir" >>$subdir/POTFILES.in.new
    echo "# $srcdir" >>$subdir/POTFILES.skip.new
    if test "$subdir" = "$podir"; then
      echo "$files" >>$subdir/POTFILES.in.new
    else
      echo "$files" >>$subdir/POTFILES.skip.new
    fi
    echo >>$subdir/POTFILES.in.new
    echo >>$subdir/POTFILES.skip.new
  done
done

# 3. Explictly specified files
for subdir in $subdirs; do
  echo "# extra files" >>$subdir/POTFILES.in.new
  echo "# extra files" >>$subdir/POTFILES.skip.new
done
for m in $POTFILES_extra; do
  podir=${m%:*}
  filename=${m#*:}
  for subdir in $subdirs; do
    if test "$subdir" = "$podir"; then
      echo "$filename" >>$subdir/POTFILES.in.new
    else
      echo "$filename" >>$subdir/POTFILES.skip.new
    fi
  done
done
for subdir in $subdirs; do
  echo >>$subdir/POTFILES.in.new
  echo >>$subdir/POTFILES.skip.new
done

# 4. Explictly excluded files
for subdir in $subdirs; do
  echo "# ignored files" >>$subdir/POTFILES.skip.new
  echo "$POTFILES_ignore" >>$subdir/POTFILES.skip.new
done

# 5. Check whether anything changed
for subdir in $subdirs; do
  for outfile in POTFILES.in POTFILES.skip; do
    outfile=$subdir/$outfile
    if test -f $outfile && diff -q $outfile $outfile.new >/dev/null; then
      rm -f $outfile.new
    else
      mv -f $outfile.new $outfile
    fi
  done
done
