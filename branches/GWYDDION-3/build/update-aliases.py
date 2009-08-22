#!/usr/bin/python
# vim: set ts=4 sw=4 et :
import sys, re, difflib

template_h = """
extern __typeof (%(function)s) IA__%(function)s __attribute__((visibility("hidden")))%(attributes)s;
#define %(function)s IA__%(function)s"""

template_c = """
#undef %(function)s 
extern __typeof (%(function)s) %(function)s __attribute__((alias("IA__%(function)s"), visibility("default")));"""

header_h = """\
/* This is a %s file.  It was generated by update-aliases.py. */
#include "config.h"
#ifdef HAVE_GNUC_VISIBILITY

#undef SEEN_HEADER
#define SEEN_HEADER defined

#undef COMPILING
#define COMPILING(x) 1
"""

footer_h = """\
#endif
"""

header_c = """\
/* This is a %s file.  It was generated by update-aliases.py. */
#ifdef HAVE_GNUC_VISIBILITY

#undef SEEN_HEADER
#define SEEN_HEADER(x) 1

#undef COMPILING
#define COMPILING defined
"""

footer_c = """\
#endif
"""

if len(sys.argv) != 3:
    print 'Usage: update-aliases.py OUTPUTFILE.{c|h} LIBRARY.symbols'
    sys.exit(0)

outfilename = sys.argv[1]
infilename = sys.argv[2]

if outfilename.endswith('.h'):
    template = template_h
    header = header_h
    footer = footer_h
elif outfilename.endswith('.c'):
    template = template_c
    header = header_c
    footer = footer_c
else:
    sys.stderr.write('Cannot determine output type for %s\n' % outfilename)
    sys.exit(1)

try:
    orig = [x.strip() for x in file(outfilename).readlines()]
except IOError:
    orig = []

output = [x.rstrip() for x in (header % 'GENERATED').split('\n')]
for line in file(infilename):
    line = line.strip()
    if line.startswith('#') or not line:
        output.append(line)
        continue
    pos = line.find(' ')
    if pos == -1:
        function, attributes = line, ''
    else:
        function, attributes = line[:pos], line[pos:]

    t = template % {'function': function, 'attributes': attributes}
    output.extend(x.rstrip() for x in t.split('\n'))
output.extend(x.rstrip() for x in footer.split('\n') if x.rstrip())

diff = difflib.unified_diff(orig, output, outfilename, outfilename + '.new')
# There seems to be no easy way to tell whether the generator is empty
diff = '\n'.join(x.rstrip() for x in diff)
if diff:
    file(outfilename, 'w').write('\n'.join(output))
    print diff
