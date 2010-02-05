#!/usr/bin/python
import sys, re
from subprocess import Popen, PIPE

variables = {}
refre = re.compile(r'(?<!\$)\$(\{\w+\}|\(\w+\))').sub
varre = re.compile(r'(?P<name>\w+)\s*=\s*(?P<value>.*)').match

def expand(match):
    name = match.group(1)[1:-1]
    if name not in variables:
        return ''
    return refre(expand, variables[name])

output = Popen(['gmake', '-qp'], stdout=PIPE).communicate()[0]
for line in output.split('\n'):
    m = varre(line)
    if m:
        variables[m.group('name')] = m.group('value').strip()

bare = False
if len(sys.argv) > 1 and sys.argv[1] in ('-b', '--bare'):
    bare = True
    del sys.argv[1]

for name in sys.argv[1:]:
    if bare:
        print refre(expand, '$(%s)' % name)
    else:
        print '%s = %s' % (name, refre(expand, '$(%s)' % name))
