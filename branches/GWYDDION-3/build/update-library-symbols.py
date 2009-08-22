#!/usr/bin/python
import sys, re

fre = re.compile(r'(?P<define>^# *(ifn?def|if|else|elif|endif).*?$)'
                 r'|^[A-Za-z][A-Za-z0-9 *]+\b(?P<function>\w+) *\([^;]*?\)'
                 r'\s*(?P<gnu>(?:G_GNUC[^;]*)?)\s*;', re.M | re.S)
makeseen = re.compile(r'^#ifndef (?P<name>__\w+)_H__$')

output = []
for filename in sys.argv[1:]:
    for match in fre.finditer(file(filename).read()):
        define = match.group('define')
        function = match.group('function')
        gnu = match.group('gnu')
        if define:
            define = makeseen.sub(r'#if (COMPILING(\g<name>_C__) '
                                  r'&& SEEN_HEADER(\g<name>_H__))', define)
            output.append(define)
        else:
            if not gnu:
                gnu = ''
            else:
                gnu = ' ' + gnu
            output.append(function + gnu)
    output.append('')

print '\n'.join(output)
