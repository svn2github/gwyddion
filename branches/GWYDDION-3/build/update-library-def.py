#!/usr/bin/python
# vim: set ts=4 sw=4 et :
import sys, re, difflib

if len(sys.argv) != 3:
    print 'Usage: update-library-def.py OUTPUTFILE.def LIBRARY.symbols'
    sys.exit(0)

outfilename = sys.argv[1]
infilename = sys.argv[2]

try:
    orig = [x.strip() for x in file(outfilename).readlines()]
except IOError:
    orig = []

output = ['EXPORTS']
for line in file(infilename):
    line = line.strip()
    if line.startswith('#') or not line:
        continue
    pos = line.find(' ')
    if pos == -1:
        function = line
    else:
        function = line[:pos]

    output.append('\t' + function)

if orig:
    diff = difflib.unified_diff(orig, output, outfilename, outfilename + '.new')
    # There seems to be no easy way to tell whether the generator is empty
    diff = '\n'.join(x.rstrip() for x in diff)
else:
    diff = ''

if diff or not orig:
    file(outfilename, 'w').write('\n'.join(output))
    print diff
