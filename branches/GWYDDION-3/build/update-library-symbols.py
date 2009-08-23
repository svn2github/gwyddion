#!/usr/bin/python
# vim: set ts=4 sw=4 et :
import sys, re, difflib

fre = re.compile(r'(?P<define>^# *(ifn?def|if|else|elif|endif).*?$)'
                 r'|^[A-Za-z][A-Za-z0-9 *]+\b(?P<function>\w+) *\([^;]*?\)'
                 r'\s*(?P<gnu>(?:G_GNUC[^;]*)?)\s*;', re.M | re.S)
makeseen = re.compile(r'^#ifndef (?P<name>__\w+)_H__$')

if len(sys.argv) < 2:
    print 'Usage: update-library-symbols.py OUTPUT.symbols [HEADER...]'
    sys.exit(0)

outfilename = sys.argv[1]
del sys.argv[1]
try:
    orig = [x.strip() for x in file(outfilename).readlines()]
except IOError:
    orig = []

output = []
for filename in sys.argv[1:]:
    # Sort symbols between preprocessor directives alphabetically to get a more
    # stable output.  Gather a group of such symbols in outputgroup.
    outputgroup = []
    for match in fre.finditer(file(filename).read()):
        define = match.group('define')
        function = match.group('function')
        gnu = match.group('gnu')
        if define:
            output.extend(sorted(outputgroup))
            outputgroup = []
            define = makeseen.sub(r'#if (COMPILING(\g<name>_C__) '
                                  r'&& SEEN_HEADER(\g<name>_H__))', define)
            output.append(define)
        else:
            if not gnu:
                gnu = ''
            else:
                gnu = ' ' + gnu
            outputgroup.append(function + gnu)
    output.extend(sorted(outputgroup))
    output.append('')

if sys.argv[1:]:
    del output[-1]

if orig:
    diff = difflib.unified_diff(orig, output, outfilename, outfilename + '.new')
    # There seems to be no easy way to tell whether the generator is empty
    diff = '\n'.join(x.rstrip() for x in diff)
else:
    diff = ''

if diff or not orig:
    file(outfilename, 'w').write('\n'.join(output))
    print diff
