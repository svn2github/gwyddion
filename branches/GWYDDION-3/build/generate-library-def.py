#!/usr/bin/python
# vim: set ts=4 sw=4 et :
import sys, re, shlex, subprocess

if len(sys.argv) < 3:
    print 'Usage: generate-library-def.py NM-PROGRAM OBJECT-FILE.lo...'
    sys.exit(0)

# The output seems to differ
globalscopes = ('T', 'GLOBAL')
nmprog = [shlex.split(sys.argv[1])[0], '-f', 'sysv']

objectfiles = []
for objectfile in sys.argv[2:]:
    if objectfile.endswith('.lo'):
        ofile = None
        try:
            for line in file(objectfile):
                m = re.match(r'^pic_object=\'(?P<ofile>.*)\'\s*$', line)
                if m:
                    ofile = m.group('ofile')
                    break
            if not ofile:
                sys.stderr.write('Cannot find pic_object in %s.\n' % objectfile)
                sys.exit(1)
        except IOError as (errno, strerror):
            sys.stderr.write('Cannot read %s: %s.\n' % (objectfile, strerror))
            sys.exit(1)
        objectfile = ofile
    objectfiles.append(objectfile)

try:
    syminput = subprocess.Popen(nmprog + objectfiles, stdout=subprocess.PIPE)
except OSError as (errno, strerror):
    sys.stderr.write('Cannot execute %s to scan symbols: %s.\n'
                     % (' '.join(nmprog), strerror))
    sys.exit(1)

sys.stdout.write('EXPORTS\n')
for line in syminput.stdout:
    try:
        name, offset, scope, typ, size, source, sect = \
            [x.strip() for x in line.split('|')]
    except ValueError:
        continue

    if sect == '.text' and scope in globalscopes and name.startswith('gwy_'):
        sys.stdout.write('    ' + name + '\n')

