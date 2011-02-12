#!/usr/bin/python
# vim-syn-gen.py -- Generate vim syntax highligting from gtk-doc documentation
# Written by Yeti <yeti@physics.muni.cz>, 2005 and later.
# This script is in the public domain.
import re, glob, time, sys, os, pwd

# To find modules in directory it was symlinked to.
sys.path.insert(0, os.path.dirname(sys.argv[0]))

options = {
    'filter_regexp': r'^_',
}

if len(sys.argv) not in range(2, 4):
    print """\
Usage: %s config-file.py [OUTPUT.vim]
If output is unspecified, dumps the vim syntax file to the standard output.

Config-file options:
  syntax_name       Vim syntax name (mandatory)
  file_glob         SOMETHING-decl.txt files to scan (mandatory)
  description       Syntax file metadata Language field
  url               Syntax file metadata URL field
  mantainer         Syntax file metadata Maintainer field
  filter_regexp     Exclude matching symbols (default: r'^_')
  override          Declaration override map (like {'NULL': 'Constant', ... })
  supplement        Declarations to insert (like {'GdkPixmap': 'Type', ...})
  types             Type highligting override map (like {'Enum': 'Keyword'})

Gtk-doc types (case doesn't matter):
  Constant, Macro, Function, Struct, Enum, Union, Typedef, User_Function,
  Variable
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

# Output redirection
if len(sys.argv) == 3:
    outputfilename = sys.argv[2]
    outputfile = file(outputfilename, 'w')
else:
    outputfilename = None
    outputfile = sys.stdout

# Default highlighting
types = {
    'CONSTANT': 'Constant',
    'DEFINE': 'Constant',   # non-parametric macros
    'MACRO': 'Macro',
    'FUNCTION': 'Function',
    'VARIABLE': 'Identifier',
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
"""

deprecation_options_template = """\
" Options:
"    Deprecated declarations are not highlighted by default.
"    let %s_enable_deprecated = 1
"       highlights deprecated declarations too (like normal declarations)
"    let %s_deprecated_errors = 1
"       highlights deprecated declarations as Errors
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
%s
  delcommand HiLink
endif
"""

deprecation_hi_link_template = """\
  if exists("%s_deprecated_errors")
%s
  elseif exists("%s_enable_deprecated")
%s
  endif
"""

normalize = lambda x: x.title().replace('_', '')

hi_link_template = '  HiLink %s%s %s'
hi_link_template_v = '    HiLink %s%s%s %s'

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

def output(s):
    outputfile.write(s)
    outputfile.write('\n')

def types_present(decldict, value=''):
    pt = [t for t, d in decldict.items()
          if [k for k, v in d.items() if v == value]]
    return dict([(x, types[x]) for x in pt])

def output_decls(decldict, value=''):
    for t, d in decldict.items():
        d = [k for k, v in d.items() if v == value]
        if not d:
            continue
        d.sort()
        output('syn keyword %s%s%s %s' % (syntax_name, value,
                                          normalize(t), ' '.join(d)))

def override(decldict, overrides, create_new):
    for o, v in overrides.items():
        if v:
            v = v.upper()
        has_it = False
        for k, d in decldict.items():
            if d.has_key(o):
                if create_new:
                    sys.stderr.write("%s already exists in %s" % (o, k))
                else:
                    has_it = True
                    del d[o]
        if (create_new or has_it) and v:
            decldict[v][o] = 1

decls = dict([(x, {}) for x in types])
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
            value = 'Deprecated'
        else:
            value = ''

        # Change parameterless macros to constants
        if d['type'] == 'MACRO' and not re_param_macro.search(d['body']):
            d['type'] = 'DEFINE'
            m = re_ident_macro.search(d['body'])
            if m:
                identdefs[d['ident']] = m.group('ident')

        # Find enum values and make them constants
        decls[d['type']][d['ident']] = value
        if d['type'] == 'ENUM':
            for e in re_enum.finditer(d['body']):
                decls['CONSTANT'][e.group('ident')] = value

# Kill macros if the same symbol also exists as a regular function
# (this fixes things like g_file_test()).
todelete = []
for macro in identdefs.keys():
    if decls['FUNCTION'].has_key(macro):
        todelete.append(macro)

for macro in todelete:
    del decls['DEFINE'][macro]
    del identdefs[macro]
del todelete

# Change macros defined to functions back to macros
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

if options.has_key('supplement'):
    override(decls, options['supplement'], True)

if options.has_key('override'):
    override(decls, options['override'], False)

# Find types that are really present
normal_types = types_present(decls)
deprecated_types = types_present(decls, 'Deprecated')

hi_links = '\n'.join([hi_link_template % (syntax_name, normalize(k), v)
                      for k, v in normal_types.items()])
hi_links_depA = '\n'.join([hi_link_template_v
                           % (syntax_name, 'Deprecated', normalize(k), v)
                          for k, v in deprecated_types.items()])
hi_links_depE = '\n'.join([hi_link_template_v
                           % (syntax_name, 'Deprecated', normalize(k), 'Error')
                          for k, v in deprecated_types.items()])

# Header
header = header_template % (options['description'], options['maintainer'],
                            time.strftime('%Y-%m-%d'), url_line)
if deprecated_types:
    header += deprecation_options_template % (syntax_name, syntax_name)

# Footer
if deprecated_types:
    deprecated_hi_links = deprecation_hi_link_template % (syntax_name,
                                                          hi_links_depE,
                                                          syntax_name,
                                                          hi_links_depA)
else:
    deprecated_hi_links = ''
footer = footer_template % (syntax_name, syntax_name, hi_links,
                            deprecated_hi_links)

# Dump!
output(header)
output_decls(decls)
if deprecated_types:
    output_decls(decls, 'Deprecated')
output(footer)
