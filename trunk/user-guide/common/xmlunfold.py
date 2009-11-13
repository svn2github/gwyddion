#!/usr/bin/python
# vim: set ts=4 sw=4 et fileencoding=utf-8 :
#
# Recursively resolve XIncludes in a XML file.
# Limitations:
# - Only xi namespace is recognized, i.e. <xi:include .../>
# - includes are expanded also in CDATA sections
# - many others
# But it can resolve the includes more than 100× faster than xsltproc.
#
# direct xsltproc:
# real	1m7.962s
# user	1m5.179s
# sys	0m1.744s
#
# xmlunfold:
# real	0m0.203s
# user	0m0.108s
# sys	0m0.066s
#
# xsltproc on unfolded result:
# real	0m27.518s
# user	0m26.565s
# sys	0m0.345s

import sys, re, os

if len(sys.argv) < 2:
    sys.stdout.write('%s: MAIN-DOCUMENT.xml >SINGLE-FILE.xml\n' % sys.argv[0])
    sys.stdout.write('Resolve XIncludes, producing a single XML file.\n')
    sys.exit(0)

searchpath = []
filename = os.path.abspath(sys.argv[1])
inputdir = os.path.dirname(filename)
searchpath.append(inputdir)
searchpath.append(os.path.abspath(os.path.join(inputdir, '..', 'formulas')))
searchpath.append(os.path.abspath(os.path.join(inputdir, '..', '..', 'common', 'formulas')))

subxinclude = re.compile(r'''(?s)<xi:include\s+href=(['"])(?P<file>[^'"]+)\1\s*/>''').sub
subcruft = re.compile(r'(?s)<\?xml\s[^>]*\?>\s*'
                      r'|<!DOCTYPE\s[^>]*>\s*'
                      r'|<!--\s[^>]*-->').sub

currentfile = [filename]

def xinclude(match):
    global currentfile
    filename = match.group('file')
    for p in searchpath:
        try:
            fnm = os.path.join(p, filename)
            text = file(fnm).read()
            currentfile.append(fnm)
            text = subxinclude(xinclude, subcruft('', text))
            del currentfile[-1]
            return text
        except IOError:
            pass
    lineno = match.string.count('\n', 0, match.start()) + 1
    sys.stderr.write('%s:%d: unresolved xi:include of ‘%s’\n'
                     % (currentfile[-1], lineno, filename))
    return ''

try:
    text = file(filename).read()
except IOError:
    sys.stderr.write('Cannot read ‘%s’\n' % filename)
    sys.exit(1)

sys.stdout.write(subxinclude(xinclude, text))
