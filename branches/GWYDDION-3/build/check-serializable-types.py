#!/usr/bin/python
import sys, os, re

if len(sys.argv) < 3:
  print "Usage: check-serializable-types.py: MAIN.C SOURCE.C..."
  sys.exit(1)

found_init = set()
ok = True
for line in file(sys.argv[1]):
    m = re.match(r'^\s*g_type_class_(?:peek_static|peek|ref)\s*\(GWY_TYPE_(?P<TYPENAME>\w+)\);\s*$',
                 line)
    if m:
        found_init.add(m.group('TYPENAME'))

for filename in sys.argv[2:]:
    text = file(filename).read()
    for m in re.finditer(r'(?m)^\s*GWY_IMPLEMENT_SERIALIZABLE\s*'
                         r'\(gwy_(?P<typename>\w+)_serializable_init\)', text):
        t = m.group('typename').upper()
        if t not in found_init:
            sys.stderr.write('Serializable object %s not found in init_types.\n'
                             % t)
            ok = False

    for m in re.finditer(r'(?m)^\s*gwy_serializable_boxed_register_static\s*'
                         r'\((?P<typename>\w+)_type,', text):
        t = m.group('typename').upper()
        if t not in found_init:
            sys.stderr.write('Serializable boxed %s not found in init_types.\n'
                             % t)
            ok = False

sys.exit(0 if ok else 1)
