#!/usr/bin/python
# $Id$
# take-declarations.py written by Yeti.
# This file is in the public domain.
import re, sys

enum_re = re.compile(
    r'(?ms)^typedef\s+enum\s+{\s+(?P<body>[^}]+?)\s+}\s+(?P<name>\w+)\s*;'
)

struct_re = re.compile(
    r'(?ms)^typedef\s+struct\s+{\s+(?P<body>[^}]+?)\s+}\s+(?P<name>\w+)\s*;'
)

func_re = re.compile(
    r'(?ms)^(?P<ret>[^()]+?)\s+(?P<name>\w+)\s*'
    r'\((?P<args>[^()]+?)\)'
    r'(?P<attrs>\s*G_GNUC_\w+(?:\([^()]+\))?)*;'
)

def format_doc(name, fields, hint='', returns=False):
    s = '/**\n * ' + name + ':\n'
    for f in fields:
        s += ' * @' + f + ': \n'
    s += ' *\n * ' + hint + '.\n'
    if returns:
        s += ' *\n * Returns: \n'
    s += ' **/'
    return s

def remove_comments(text):
    text = re.sub(r'(?s)/\*.*?\*/', '', text)
    text = re.sub(r'(?m)//.*$', '', text)
    return text

def process_enums(text):
    out = text
    for m in reversed(list(enum_re.finditer(text))):
        name, body = m.group('name'), m.group('body')
        body = remove_comments(body)
        fields = []
        for f in body.split(','):
            f = f.strip()
            if not f:
                continue
            f = re.findall(r'\w+', f)[0]
            fields.append(f)
        out = out[:m.start()] \
              + format_doc(name, fields, 'Type of ') \
              + out[m.end():]
    return out

def process_structs(text):
    out = text
    for m in reversed(list(struct_re.finditer(text))):
        name, body = m.group('name'), m.group('body')
        body = remove_comments(body)
        fields = []
        for f in body.split(';'):
            f = f.strip()
            if not f:
                continue
            f = re.sub(r'\([^)]+\)$', '', f)
            f = re.sub(r'\[[^]]+\]$', '', f)
            f = re.findall(r'\w+', f)[-1]
            fields.append(f)
        out = out[:m.start()] \
              + format_doc(name, fields, 'Type of ') \
              + out[m.end():]
    return out

def process_funcs(text):
    out = text
    for m in reversed(list(func_re.finditer(text))):
        name, ret, args = m.group('name'), m.group('ret'), m.group('args')
        args = remove_comments(args)
        definition = '\n' + ret.strip() + '\n' + name + '('
        n = len(name)
        fields = []
        for f in args.split(','):
            f = f.strip()
            if fields:
                definition += ' ' * (n + 1)
            definition += f + ',\n'
            f = re.sub(r'\([^)]+\)$', '', f)
            f = re.sub(r'\[[^]]+\]$', '', f)
            f = re.findall(r'\w+', f)[-1]
            fields.append(f)
        if definition.endswith(',\n'):
            definition = definition[:-2]
        definition += ')\n{\n\n}\n'
        ret = re.sub(r'\<static\>|\<inline\>', '', ret).strip()
        if '_get_' in name:
            hint = 'Gets the '
        if '_set_' in name:
            hint = 'Sets the '
        else:
            hint = ''
        out = out[:m.start()] \
              + format_doc(name, fields, hint, ret.strip() != 'void') \
              + definition \
              + out[m.end():]
    return out

text = sys.stdin.read().decode('utf-8')
text = process_enums(text)
text = process_structs(text)
text = process_funcs(text)
sys.stdout.write(text.encode('utf-8'))
