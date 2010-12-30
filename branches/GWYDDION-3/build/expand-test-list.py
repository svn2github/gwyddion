#!/usr/bin/python
# vim: set ts=4 sw=4 et :
import sys, re
HEADER, CODE = range(2)

if len(sys.argv) != 3:
    print 'Usage: expand-test-list.py {h|c} ROOT <TEST-LIST >SOURCE-FILE'
    sys.exit(0)

if sys.argv[1].lower() in ('h', 'header'):
    mode = HEADER
elif sys.argv[1].lower() in ('c', 'code'):
    mode = CODE
else:
    sys.stderr.write('Mode must be c or h.\n')
    sys.exit(1)

root = sys.argv[2]
if not root.startswith('/'):
    root = '/' + root
if not root.endswith('/'):
    root = root + '/'

sys.stdout.write('/* This is a ' + 'GENERATED' + ' file. */\n')
for line in sys.stdin:
    path = line.strip()
    if not path or path.startswith('#'):
        continue

    fullpath = root + path
    funcname = 'test_' + path.replace('/', '_').replace('-', '_')
    if mode == HEADER:
        sys.stdout.write('void %s(void);\n' % funcname)
    if mode == CODE:
        sys.stdout.write('g_test_add_func("%s", %s);\n' % (fullpath, funcname))
