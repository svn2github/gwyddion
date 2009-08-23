#!/usr/bin/python
# Written by Yeti <yeti@gwyddion.net>. Public domain.
from __future__ import generators
import sys, re

debug = False

if len(sys.argv) < 4:
    print sys.argv[0], 'SECTION_FILE OBJECT_FILE INTERFACE-FILE [--standard-files=FILE,...]'
    sys.exit(1)

title_re = re.compile(r'<TITLE>(?P<object>\w+)</TITLE>')
file_re = re.compile(r'<FILE>(?P<file>\w+)</FILE>')
classlike_re = re.compile(r'^\w+(?:Class|Interface)$')
section_file = sys.argv[1]
object_file = sys.argv[2]
interface_file = sys.argv[3]
ignore_files = {}
for x in sys.argv[4:]:
    if x.startswith('--standard-files='):
        for f in x[len('--standard-files='):].split(','):
            ignore_files[f] = 1
    else:
        stderr.write(sys.argv[0] + ': Unknown option ' + x + '\n')

fh = file(object_file, 'r')
objects = set(s.strip() for s in fh.readlines())
fh.close()

if debug:
    print 'Objects from %s:' % object_file, objects

# FIXME: This catches only implemented interfaces
fh = file(interface_file, 'r')
interfaces = set(s.split()[1] for s in fh.readlines() if s.strip())
fh.close()

if debug:
    print 'Interfaces from %s:' % interface_file, interfaces

fh = file(section_file, 'r')
lines = fh.readlines()
fh.close()

fh = file(section_file, 'w')
addme = ''
added = False
standardizing = False
for i, l in enumerate(lines):
    m = file_re.match(l)
    if m and m.group('file') in ignore_files:
        standardizing = True
    if l.strip() in (addme, addme + 'Class', addme + 'Interface') \
       or classlike_re.match(l.strip()):
        if debug:
            print 'Skipping matching %s' % l.strip()
        l = ''
    if addme and not added:
        if addme in interfaces:
            suffix = 'Interface'
        else:
            suffix = 'Class'
        if debug:
            print 'Adding %s, %s%s' % (addme, addme, suffix)
        fh.write(addme + '\n')
        fh.write(addme + suffix + '\n')
        added = True
    m = title_re.match(l)
    if m:
        added = False
        addme = m.group('object')
        if debug:
            print 'Object-like declaration %s' % addme
        if addme not in objects:
            if debug:
                print 'Type %s is not in objects' % addme
            addme = ''
    fh.write(l)
    if standardizing:
        fh.write('<SUBSECTION Standard>\n')
        standardizing = False
fh.close()
