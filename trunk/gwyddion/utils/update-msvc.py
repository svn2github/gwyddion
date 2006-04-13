#!/usr/bin/python
# @(#) $Id$
import re, os, sys, shutil, glob

re_listend = re.compile(r'\s*\\\n\s*')
re_nm = re.compile(r'(?P<addr>[a-z0-9 ]+) (?P<type>[-A-Za-z?]) (?P<symbol>\w+)')
re_template = re.compile(r'<\[\[:(?P<name>\w+):\]\]>')
re_cvsid = re.compile(r'^# @\(#\) \$(Id).*', re.MULTILINE)

prg_object_rule = """\
%s.obj: %s.c
\t$(CC) $(CFLAGS) -GD -c $(PRG_CFLAGS) %s.c
"""

mod_dll_rule = """\
%s.dll: %s.obj
\t$(LINK32) %s.obj $(MOD_LINK) $(WIN32LIBS) $(LDFLAGS) /out:%s.dll /dll /implib:%s.lib
"""

mo_inst_rule = """\
\t$(INSTALL) %s.gmo "$(DEST_DIR)\\locale\\%s\\LC_MESSAGES\\gwyddion.mo"\
"""

top_dir = os.getcwd()

quiet = len(sys.argv) > 1 and sys.argv[1] == '-q'

def backup(filename):
    try:
        shutil.copyfile(filename, filename + '~')
        return True
    except IOError:
        return False

def get_file(filename):
    fh = file(filename, 'r')
    text = fh.read()
    fh.close()
    return text

def write_file(filename, text):
    if type(text) == type([]):
        t = '\n'.join(text) + '\n'
    else:
        t = text
    fh = file(filename, 'w')
    fh.write(t);
    fh.close()

def print_filename(filename):
    rcwd = os.getcwd()[len(top_dir)+1:]
    print os.path.join(rcwd, filename)

def backup_write_diff(filename, text):
    rundiff = backup(filename)
    write_file(filename, text)
    if not quiet:
        print_filename(filename)
    if rundiff:
        fh = os.popen('diff %s~ %s' % (filename, filename), 'r')
        diff = fh.readlines()
        fh.close()
        if diff:
            if quiet:
                print_filename(filename)
            sys.stdout.write(''.join(diff))

def get_list(text, name):
    r = re.compile(r'^\s*%s\s*=\s*(?P<list>(?:.*\\\n)*.*)' % name, re.M)
    m = r.search(text)
    if not m:
        return []
    m = m.group('list')
    m = re_listend.sub(' ', m).strip().split(' ')
    return m

def fix_suffixes(lst, suffix, replacewith=''):
    if not suffix:
        return lst
    sl = len(suffix)
    newlst = []
    for x in lst:
        if x.endswith(suffix):
            x = x[:-sl] + replacewith
        newlst.append(x)
    return newlst

def get_object_symbols(filename, symtype='T'):
    fh = os.popen('nm -p -t x %s' % filename, 'r')
    syms = [re_nm.match(x) for x in fh.readlines()]
    fh.close()
    syms = [x for x in syms if x]
    syms = [x.group('symbol') for x in syms if x.group('type') == symtype]
    syms = [x for x in syms if not x.startswith('_')]
    syms.sort()
    return syms

def make_lib_defs(makefile):
    libraries = fix_suffixes(get_list(makefile, 'lib_LTLIBRARIES'), '.la')
    for l in libraries:
        syms = get_object_symbols('.libs/%s.so' % l)
        syms = ['EXPORTS'] + ['\t' + x for x in syms]
        backup_write_diff('%s.def' % l, syms)

def expand_template(makefile, name):
    if name == 'DATA':
        lst = get_list(makefile, '\w+_DATA')
        list_part = name + ' =' + ' \\\n\t'.join([''] + lst)
        inst_part = [('$(INSTALL) %s "$(DEST_DIR)\$(DATA_TYPE)"' % x)
                     for x in lst]
        inst_part = '\n\t'.join(['install-data: data'] + inst_part)
        return list_part + '\n\n' + inst_part
    elif name == 'LIB_HEADERS':
        libraries = fix_suffixes(get_list(makefile, 'lib_LTLIBRARIES'), '.la')
        lst = []
        for l in libraries:
            l = re.sub(r'[^a-z0-9]', '_', l)
            lst += get_list(makefile, '%sinclude_HEADERS' % l)
        return name + ' =' + ' \\\n\t'.join([''] + lst)
    elif name == 'LIB_OBJECTS':
        libraries = get_list(makefile, 'lib_LTLIBRARIES')
        lst = []
        for l in libraries:
            l = re.sub(r'[^a-z0-9]', '_', l)
            lst += fix_suffixes(get_list(makefile, '%s_SOURCES' % l),
                                '.c', '.obj')
        return name + ' =' + ' \\\n\t'.join([''] + lst)
    elif name == 'PRG_OBJECTS':
        programs = get_list(makefile, 'bin_PROGRAMS')
        lst = []
        for p in programs:
            p = re.sub(r'[^a-z0-9]', '_', p)
            lst += fix_suffixes(get_list(makefile, '%s_SOURCES' % p),
                                '.c', '.obj')
        return name + ' =' + ' \\\n\t'.join([''] + lst)
    elif name == 'PRG_OBJECT_RULES':
        programs = get_list(makefile, 'bin_PROGRAMS')
        lst = []
        for p in programs:
            p = re.sub(r'[^a-z0-9]', '_', p)
            for x in fix_suffixes(get_list(makefile, '%s_SOURCES' % p), '.c'):
                lst.append(prg_object_rule % (x, x, x))
        return  '\n'.join(lst)
    elif name == 'MODULES':
        mods = get_list(makefile, r'\w+_LTLIBRARIES')
        mods = fix_suffixes(fix_suffixes(mods, '.la', '.dll'), ')', ').dll')
        return name + ' =' + ' \\\n\t'.join([''] + mods)
    elif name == 'MOD_DLL_RULES':
        mods = fix_suffixes(get_list(makefile, r'\w+_LTLIBRARIES'), '.la')
        lst = []
        for m in mods:
            lst.append(mod_dll_rule % (m, m, m, m, m))
        return  '\n'.join(lst)
    elif name == 'MO_INSTALL_RULES':
        pos = [l.strip() for l in file('LINGUAS')]
        pos = [l for l in pos if l and not l.strip().startswith('#')]
        assert len(pos) == 1
        pos = re.split('\s+', pos[0])
        if not pos:
            return
        lst = ['installdirs:', '\t-@mkdir "$(DEST_DIR)\\locale"']
        for p in pos:
            lst.append('\t-@mkdir "$(DEST_DIR)\\locale\\%s"' % p)
            lst.append('\t-@mkdir "$(DEST_DIR)\\locale\\%s\\LC_MESSAGES"' % p)
        lst.append('')
        lst.append('install-mo:')
        for p in pos:
            lst.append(mo_inst_rule % (p, p))
        return '\n'.join(lst)
    print '*** Unknown template %s ***' % name
    return ''

def fill_templates(makefile):
    templates = glob.glob('*.gwt')
    templates = fix_suffixes(templates, '.gwt')
    for templ in templates:
        text = get_file(templ + '.gwt')
        text = re_cvsid.sub('# This is a GENERATED file.', text)
        m = re_template.search(text)
        while m:
            text = text[:m.start()] \
                   + expand_template(makefile, m.group('name')) \
                   + text[m.end():]
            m = re_template.search(text)
        backup_write_diff(templ, text)

def process_one_dir(makefile):
    make_lib_defs(makefile)
    fill_templates(makefile)

def recurse(each):
    cwd = os.getcwd()
    try:
        makefile = get_file('Makefile.am')
    except IOError:
        return
    each(makefile)
    subdirs = get_list(makefile, 'SUBDIRS')
    for s in subdirs:
        os.chdir(s)
        recurse(each)
        os.chdir(cwd)

configure = get_file('configure.ac')
recurse(process_one_dir)

cwd = os.getcwd()
os.chdir('po')
fill_templates(configure)
os.chdir(cwd)

