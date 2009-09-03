#!/usr/bin/python
import xml.parsers.expat
import sys

class Parser(object):
    def StartElementHandler(self, name, attributes):
        self.path.append((name, attributes))

    def EndElementHandler(self, name):
        del self.path[-1]

    def _setup_handlers(self):
        for a in dir(type(self)):
            if a.endswith('Handler'):
                setattr(self.parser, a, getattr(self, a))

    def parse_file(self, filename, *args):
        self.filename = filename
        self.parser = xml.parsers.expat.ParserCreate(*self.parserargs)
        self._setup_handlers()
        self.path = []
        self.parser.ParseFile(file(filename))
        del self.parser

    def __init__(self, *args):
        self.parserargs = args

class FormulaParser(Parser):
    def StartElementHandler(self, name, attributes):
        Parser.StartElementHandler(self, name, attributes)
        if name in ('informalequation', 'inlineequation'):
            self.lineno = self.parser.CurrentLineNumber
            if 'id' not in attributes:
                if name == 'informalequation':
                    sys.stderr.write('%s:%d: missing id on <%s>\n'
                                     % (self.filename, self.lineno, name))
                self.fid = None
                return
            self.fid = attributes['id']
            if self.fid in self.formulas:
                self.stash = self.formulas[self.fid]
            else:
                self.stash = None
            self.formulas[self.fid] = ''

    def EndElementHandler(self, name):
        if name in ('informalequation', 'inlineequation'):
            if not self.fid:
                return
            if self.stash != None and self.formulas[self.fid] != self.stash:
                sys.stderr.write('%s:%d: conflicting definition of formula %s\n'
                                 % (self.filename, self.lineno, self.fid))
                # Keep the first definition, unless it's empty
                if self.stash:
                    self.formulas[self.fid] = self.stash
            elif not self.formulas[self.fid]:
                sys.stderr.write('%s:%d: empty formula %s\n'
                                 % (self.filename, self.lineno, self.fid))
            self.fid = self.stash = None
        Parser.EndElementHandler(self, name)

    def CharacterDataHandler(self, data):
        p = self.path
        if len(p) < 5 or not self.fid:
            return

        if (p[-1][0] != 'phrase'
            or p[-2][0] != 'textobject' or p[-2][1].get('role') != 'tex'
            or p[-3][0] not in ('mediaobject', 'inlinemediaobject')
            or p[-4][0] not in ('informalequation', 'inlineequation')):
            return

        data = data.rstrip()
        if data:
            self.formulas[self.fid] += data + '\n'

    def __init__(self, *args):
        Parser.__init__(self, *args)
        self.formulas = {}

    def parse_file(self, filename, *args):
        self.stash = self.fid = None
        Parser.parse_file(self, filename, *args)

class ImageParser(Parser):
    def StartElementHandler(self, name, attributes):
        Parser.StartElementHandler(self, name, attributes)
        if name == 'imagedata':
            for a in 'fileref', 'format':
                if a not in attributes:
                    sys.stderr.write('%s:%d: missing attribute %s in <%s>\n'
                                     (self.filename,
                                      self.parser.CurrentLineNumber, a,
                                      self.name))
                    return
            # TODO: Warn if the format does not match
            self.imageinfo[attributes['fileref']] = attributes['format']

    def __init__(self, *args):
        Parser.__init__(self, *args)
        self.imageinfo = {}

def update_file(filename, content):
    content = content.encode('utf-8')
    try:
        orig = file(filename).read()
    except IOError:
        orig = None

    if content != orig:
        print 'Writing %s...' % filename
        file(filename, 'w').write(content)

def extract_formulas(filenames):
    parser = FormulaParser('utf-8')
    for filename in filenames:
        parser.parse_file(filename)
    for k, v in sorted(parser.formulas.items()):
        update_file('formulas/%s.tex' % k, v)

def extract_formulainfo(filenames):
    parser = FormulaParser('utf-8')
    for filename in filenames:
        parser.parse_file(filename)
    for k in sorted(parser.formulas.keys()):
        print 'formulas/%s.tex' % k

def extract_imageinfo(filenames):
    parser = ImageParser('utf-8')
    for filename in filenames:
        parser.parse_file(filename)
    for k, v in sorted(parser.imageinfo.items()):
        print '%s:images/%s' % (v, k)

if len(sys.argv) < 2:
    print 'Usage: %s {formulas|formulainfo|imageinfo} FILES...' % sys.argv[0]
    sys.exit(0)

mode = sys.argv[1]
files = sys.argv[2:]
if mode == 'formulas':
    extract_formulas(files)
if mode == 'formulainfo':
    extract_formulainfo(files)
elif mode == 'imageinfo':
    extract_imageinfo(files)
else:
    sys.stderr.write('Wrong mode %s\n' % mode)

# vim: set ts=4 sw=4 et :
