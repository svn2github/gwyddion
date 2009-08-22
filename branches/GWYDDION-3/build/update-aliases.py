#!/usr/bin/python
import sys, re

template_h = """
extern __typeof (%(function)s) IA__%(function)s __attribute((visibility("hidden")))%(attributes)s;
#define %(function)s IA__%(function)s"""

template_c = """
#undef %(function)s 
extern __typeof (%(function)s) %(function)s __attribute((alias("IA__%(function)s"), visibility("default")));"""

header_h = """\
/* This is a %s file. */
#undef SEEN_HEADER
#define SEEN_HEADER defined

#undef COMPILING
#define COMPILING(x) 1
"""

header_c = """\
/* This is a %s file. */
#undef SEEN_HEADER
#define SEEN_HEADER(x) 1

#undef COMPILING
#define COMPILING defined
"""

if len(sys.argv) != 2:
    print 'Usage: update-aliases {alias|aliasdef} <LIBRARY.symbols >OUTPUT'
    sys.exit(0)

if sys.argv[1] == 'h':
    template = template_h
    header = header_h
elif sys.argv[1] == 'c':
    template = template_c
    header = header_c
else:
    sys.stderr.write('Bad argument %s\n' % sys.argv[1])
    sys.exit(1)

print header % 'GENERATED'
for line in sys.stdin:
    line = line.strip()
    if line.startswith('#') or not line.strip():
        print line
        continue
    pos = line.find(' ')
    if pos == -1:
        function, attributes = line, ''
    else:
        function, attributes = line[:pos], line[pos:]

    print template % {'function': function, 'attributes': attributes}
