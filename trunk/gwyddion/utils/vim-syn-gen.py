#!/usr/bin/python
# vim-syn-gen.py -- Generate vim syntax highligting from gtk-doc documentation
# Written by Yeti <yeti@physics.muni.cz>, last changed: 2004-08-13.
# This file is in the public domain.
import re, glob, time, sys, os, pwd

# To find modules in directory it was symlinked to.
sys.path.insert(0, os.path.dirname(sys.argv[0]))

options = {
    'filter_regexp': r'^_',
}

if len(sys.argv) != 2:
    print """\
Usage: %s config-file.py >something.vim'
Config-file options:
  syntax_name       Vim syntax name (mandatory)
  file_glob         SOMETHING-decl.txt files to scan (mandatory)
  description       Syntax file metadata Language field
  url               Syntax file metadata URL field
  mantainer         Syntax file metadata Maintainer field
  filter_regexp     Exclude matching symbols (default: r'^_')
  override          Declaration override map (like {'NULL': 'Constant', ... })
  types             Type highligting override map (like {'Enum': 'Keyword'})

Gtk-doc types (case doesn't matter):
  Constant, Macro, Function, Struct, Enum, Union, Typedef, User_Function
Extra vim-syn-gen types:
  Define (parameterless macro)

Version: $Id$\
""" % sys.argv[0]
    sys.exit(0)

xoptions = {}
execfile(sys.argv[1], xoptions)
options.update(xoptions)
assert options.has_key('syntax_name') and options.has_key('file_glob')
syntax_name = options['syntax_name']
if not options.has_key('maintainer'):
    options['maintainer'] = '@'.join(pwd.getpwuid(os.getuid())[0], os.uname())
if not options.has_key('description'):
    options['description'] = 'C %s extension' % syntax_name

if options.has_key('url'):
    url_line = '" URL: %s\n' % options['url']
else:
    url_line = ''

# Default highlighting
types = {
    'CONSTANT': 'Constant',
    'DEFINE': 'Constant',   # non-parametric macros
    'MACRO': 'Macro',
    'FUNCTION': 'Function',
    'STRUCT': 'Type',
    'ENUM': 'Type',
    'UNION': 'Type',
    'TYPEDEF': 'Type',
    'USER_FUNCTION': 'Type'
}

if options.has_key('types'):
    types.update(dict([(k.upper(), v) for k, v in options['types'].items()]))

header_template = """\
" Vim syntax file
" Language: %s
" Maintainer: %s
" Last Change: %s
%s" Generated By: vim-syn-gen.py
" Options: let %s_enable_deprecated = 1
"          enables highlighting of deprecated declarations (if any).
"""

footer_template = """
" Default highlighting
if version >= 508 || !exists("did_%s_syntax_inits")
  if version < 508
    let did_%s_syntax_inits = 1
    command -nargs=+ HiLink hi link <args>
  else
    command -nargs=+ HiLink hi def link <args>
  endif
%s
  delcommand HiLink
endif
"""

normalize = lambda x: x.title().replace('_', '')

hi_link_template = '  HiLink %s%s %s'
hi_links = '\n'.join([hi_link_template % (syntax_name, normalize(k), v)
                      for k, v in types.items()])

header = header_template % (options['description'], options['maintainer'],
                            time.strftime('%Y-%m-%d'),
                            url_line, syntax_name)
footer = footer_template % (syntax_name, syntax_name, hi_links)

re_decl = re.compile(r'<(?P<type>' + r'|'.join(types.keys()) + r')>\n'
                     + r'<NAME>(?P<ident>\w+)</NAME>\n'
                     + r'(?P<body>.*?)'
                     + r'</(?P=type)>\n',
                     re.S)
re_enum = re.compile(r'^\s+(?P<ident>[A-Z][A-Z0-9_]+)\b', re.M)
re_param_macro = re.compile(r'^\s*#\s*define\s+\w+\(', re.M)
re_ident_macro = re.compile(r'^\s*#\s*define\s+\w+\s+'
                            + r'(?P<ident>[A-Za-z_]\w+)\s*$', re.M)
re_filter = re.compile(options['filter_regexp'])

def print_decls(decldict, value):
    for t, d in decldict.items():
        d = [k for k, v in d.items() if v == value]
        if not d:
            continue
        d.sort()
        print 'syn keyword %s%s %s' % (syntax_name, normalize(t), ' '.join(d))

def override(decldict, overides):
    for o, v in overides.items():
        v = v.upper()
        has_it = False
        for k, d in decldict.items():
            if d.has_key(o):
                has_it = True
                del d[o]
        if has_it:
            decldict[v][o] = 1

decls = dict([(x, {}) for x in types])
deprecated_found = False
identdefs = {}
for filename in glob.glob(options['file_glob']):
    fh = file(filename, 'r')
    text = fh.read()
    fh.close()
    for d in re_decl.finditer(text):
        d = d.groupdict()
        if re_filter.search(d['ident']):
            continue

        if d['body'].find('<DEPRECATED/>') > -1:
            value = 'deprecated'
            deprecated_found = True
        else:
            value = None

        if d['type'] == 'MACRO' and not re_param_macro.search(d['body']):
            d['type'] = 'DEFINE'
            m = re_ident_macro.search(d['body'])
            if m:
                identdefs[d['ident']] = m.group('ident')

        decls[d['type']][d['ident']] = value
        if d['type'] == 'ENUM':
            for e in re_enum.finditer(d['body']):
                decls['CONSTANT'][e.group('ident')] = value

# FIXME: this is not recursive, and also doesn't catch defines to symbols
# from other libraries -- how to handle that?
for macro, body in identdefs.items():
    for k, d in decls.items():
        if not d.has_key(body):
            continue
        #sys.stderr.write('%s -> %s (%s)\n' % (macro, body, k))
        if k == 'FUNCTION' or k == 'MACRO':
            decls['MACRO'][macro] = decls['DEFINE'][macro]
            del decls['DEFINE'][macro]

if options.has_key('overide'):
    override(decls, options['override'])

print header
print_decls(decls, None)
if deprecated_found:
    print 'if exists("%s_enable_deprecated")' % syntax_name
    print_decls(decls, 'deprecated')
    print 'endif'
print footer
