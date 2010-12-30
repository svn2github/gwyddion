#!/usr/bin/python
import sys, os, re, glob, shlex, subprocess

if len(sys.argv) != 5:
  print "Usage: check-library-symbols.py: NM-PROGRAM LIBRARY.la LIBRARY-decl.txt HEADER-DIRECTORY"
  sys.exit(1)

# The output seems to differ
globalscopes = ('T', 'GLOBAL')
nmprog = [shlex.split(sys.argv[1])[0], '-f', 'sysv']

laname = sys.argv[2]
declname = sys.argv[3]
headerdir = sys.argv[4]

# Read libtool's LIBRARY.la file to learn the real name
dlname = None
for line in file(laname):
    m = re.match(r'''^dlname='(?P<name>.*)'$''', line.strip())
    if m:
        dlname=m.group('name')
        break

if not dlname:
    sys.stderr.write('Cannot determine library dlname from %s.\n', laname)
    sys.exit(1)

# Read the symbols using nm
dlname = os.path.join(os.path.dirname(laname), dlname)
try:
    syminput = subprocess.Popen(nmprog + [dlname], stdout=subprocess.PIPE).stdout
except OSError as (errno, strerror):
    sys.stderr.write('Cannot execute %s to scan symbols: %s.\n'
                     % (' '.join(nmprog), strerror))
    sys.exit(1)

exported_symbols = {}
for line in syminput:
    try:
        name, offset, scope, typ, size, source, sect = \
            [x.strip() for x in line.split('|')]
    except ValueError:
        continue

    if sect == '.text' and scope in globalscopes and not name.startswith('_'):
        exported_symbols[name] = source
del syminput

# Read FUNCTIONs from the documentation, this saves us parsing the headers
function_re = re.compile(r'<FUNCTION>.*?</FUNCTION>', re.S).finditer
name_re = re.compile(r'<NAME>(?P<name>[^<>]*)</NAME>', re.S).search
declared_symbols = {}
for m in function_re(file(declname).read()):
    m = name_re(m.group())
    declared_symbols[m.group('name')] = '???'

# Find header files for functions
decllistname = declname.replace('decl.txt', 'decl-list.txt')
file_re = re.compile(r'<FILE>(?P<name>.*?)</FILE>').match
filename = '???'
for line in file(decllistname):
    m = file_re(line)
    if m:
        filename = m.group('name')
    else:
        line = line.strip()
        if line in declared_symbols:
            declared_symbols[line] = filename + '.h'

# Read header files and gather static inline functions.  They are decalared and
# documented, but not exported.
inline_re = re.compile(r'_GWY_STATIC_INLINE +[A-Za-z0-9_* ]+ (?P<name>[A-Za-z0-9_]+) *\(').match
inline_functions = {}
for filename in glob.glob(os.path.join(headerdir, '*.h')):
    for line in file(filename):
        m = inline_re(line)
        if m:
            inline_functions[m.group('name')] = filename

exported_private = set(exported_symbols) - set(declared_symbols)
declared_missing = set(declared_symbols) - (set(exported_symbols)
                                            | set(inline_functions))
lib = re.sub(r'.la$', '', os.path.basename(laname))

for sym in sorted(exported_private):
    loc = exported_symbols[sym]
    sys.stderr.write('Exported undeclared symbol (%s, %s): %s\n'
                     % (lib, loc, sym))

for sym in sorted(declared_missing):
    loc = declared_symbols[sym]
    sys.stderr.write('Declared undefined symbol (%s, %s): %s\n'
                     % (lib, loc, sym))

sys.exit(not (not exported_private and not declared_missing))
