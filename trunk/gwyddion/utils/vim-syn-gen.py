#!/usr/bin/python
# vim-syn-gen.py -- Generate vim syntax highligting from gtk-doc documentation
# Written by Yeti <yeti@physics.muni.cz>
# This file is in the public domain.
import re, glob, time, sys, os, pwd

options = {
    'filter_regexp': r'^_',
}

if len(sys.argv) != 2:
    print """\
Usage: %s config-file.py >something.vim'
Config-file options:
  syntax_name          Vim syntax name (mandatory)
  file_glob            SOMETHING-decl.txt files to scan (mandatory)
  description          Syntax file Language field
  base_url             Syntax file URL field (w/o actual filename)
  mantainer            Syntax file Maintainer field
  filter_regexp        Exclude matching symbols (default: r'^_')""" \
          % sys.argv[0]
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

if options.has_key('url_base'):
    url_line = '" URL: %s/%s.vim\n' % (options['url_base'], syntax_name)
else:
    url_line = ''

# Default highlighting
types = {
    'CONSTANT': 'Constant',
    'MACRO': 'Macro',
    'FUNCTION': 'Function',
    'STRUCT': 'Type',
    'ENUM': 'Type',
    'TYPEDEF': 'Type',
    'USER_FUNCTION': 'Type'
}

header_template = """\
" Vim syntax file
" Language: %s
" Maintainer: %s
" Last Change: %s
%s" Generated By: vim-syn-gen.py
" Options: Set %s_enable_deprecated to enable highlighting
"          of deprecated declarations (if any).
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
re_enum = re.compile(r'^\s+(?P<ident>[A-Z][A-Z0-9_]+)\s*=', re.M)
re_filter = re.compile(options['filter_regexp'])

def print_decls(decldict):
    for t, d in decldict.items():
        d = d.keys()
        if not d:
            continue
        d.sort()
        print 'syn keyword %s%s %s' % (syntax_name, normalize(t), ' '.join(d))

decls = dict([(x, {}) for x in types])
depdecls = dict([(x, {}) for x in types])
for filename in glob.glob(options['file_glob']):
    fh = file(filename, 'r')
    text = fh.read()
    fh.close()
    for d in re_decl.finditer(text):
        d = d.groupdict()
        if re_filter.search(d['ident']):
            continue

        if d['body'].find('<DEPRECATED/>') > -1:
            insert_to = depdecls
        else:
            insert_to = decls

        insert_to[d['type']][d['ident']] = 1
        if d['type'] == 'ENUM':
            for e in re_enum.finditer(d['body']):
                insert_to['CONSTANT'][e.group('ident')] = 1

print header
print_decls(decls)
if [x for x in depdecls.values() if x]:
    print 'if exists("%s_enable_deprecated")' % syntax_name
    print_decls(depdecls)
    print 'endif'
print footer
