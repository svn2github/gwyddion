#!/usr/bin/python
import sys, re
from subprocess import Popen, PIPE

vars = {}
refre = re.compile(r'(?<!\$)\$(\{\w+\}|\(\w+\))').sub
varre = re.compile(r'(?P<name>\w+)\s*=\s*(?P<value>.*)').match

def expand(match):
    name = match.group(1)[1:-1]
    return refre(expand, vars[name])

output = Popen(['gmake', '-qp'], stdout=PIPE).communicate()[0]
for line in output.split('\n'):
    m = varre(line)
    if m:
        vars[m.group('name')] = m.group('value').strip()

for name in sys.argv[1:]:
    print '%s = %s' % (name, refre(expand, '$(' + name + ')'))
