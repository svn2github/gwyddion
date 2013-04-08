#!/usr/bin/python
import sys, re

known_boxed = frozenset(
    ('FIELD_PART', 'LINE_PART', 'BRICK_PART', 'RANGE', 'RGBA', 'XY', 'XYZ')
)

signal_re = re.compile(r'(?ms)'
    r'signals\[(?P<nameuc>\w+)\]\s*'
    r'=\s*g_signal_new_class_handler\s*\((?P<args>[^;]+?)\);')

if len(sys.argv) < 2:
  print "Usage: check-marshallers.py [--verbose] SOURCE.c..."
  sys.exit(1)

verbose = False
if sys.argv[1] in ('-v', '--verbose'):
    verbose = True
    del sys.argv[1]

status = 0
for filename in sys.argv[1:]:
    try:
        code = file(filename).read()
    except OSError as (errno, strerror):
        sys.stderr.write('Cannot read %s: %s.\n' % (filename, strerror))
        status = max(status, 2)
        continue

    for m in signal_re.finditer(code):
        lineno = code.count('\n', 0, m.start()) + 1
        nameuc = m.group('nameuc')
        if not nameuc.startswith('SGNL_'):
            sys.stderr.write('%s:%s: Signal identifier %s does not '
                             'start with SGNL_.\n'
                             % (filename, lineno, nameuc))
            status = max(status, 1)
        nameuc = re.sub(r'^SGNL_', '', nameuc)

        args = [x.strip() for x in m.group('args').split(',')]

        namelc = args[0]
        if verbose:
            sys.stdout.write('%s:%s: Checking signal %s (%s).\n'
                             % (filename, lineno, namelc, nameuc))

        if namelc[0] != '"' or namelc[-1] != '"':
            sys.stderr.write('%s:%s: Cannot parse signal name %s.\n'
                             % (filename, lineno, namelc))
            status = max(status, 2)
            continue
        namelc = namelc[1:-1]
        if namelc.upper().replace('-', '_') != nameuc:
            sys.stderr.write('%s:%s: Enumerated signal name %s does not '
                             'match string name "%s".\n'
                             % (filename, lineno, nameuc, namelc))
            status = max(status, 1)

        objtype = args[1]
        if objtype != 'type' \
           and objtype != 'G_OBJECT_CLASS_TYPE(klass)' \
           and objtype != 'G_TYPE_FROM_INTERFACE(iface)':
            sys.stderr.write('%s:%s: Object type %s should be '
                             'G_OBJECT_CLASS_TYPE(klass).\n'
                             % (filename, lineno, objtype))
            status = max(status, 1)

        flags, classhandler, accumulator, accudata = args[2:6]
        marshaller, rettype, nargs = args[6:9]
        nargs = int(nargs)

        mt = re.sub(r'^[a-z_]+_cclosure_marshal_', '', marshaller)
        mt_ret, mt_args = mt.split('__')
        mt_args = mt_args.split('_')
        if mt_args == ['VOID']:
            mt_args = []
        if mt_ret == 'VOID':
            mt_ret = 'NONE'

        argtypes = [re.sub(r'^[A-Z]+_TYPE_', '', x) for x in args[9:]]
        rettype = re.sub(r'^[A-Z]+_TYPE_', '', rettype)
        #print namelc, mt_ret, mt_args, argtypes

        if rettype != mt_ret:
            sys.stderr.write('%s:%s: Marshaller return type %s does not match'
                             'given type %s.\n'
                             % (filename, lineno, mt_ret, rettype))
            status = max(status, 1)

        if len(mt_args) != nargs:
            sys.stderr.write('%s:%s: Marshaller number of args %u does not '
                             'match given number of arguments %u.\n'
                             % (filename, lineno, len(mt_args), nargs))
            status = max(status, 1)

        if len(argtypes) != nargs:
            sys.stderr.write('%s:%s: Length of argument type list %u does not '
                             'match given number of arguments %u.\n'
                             % (filename, lineno, len(argtypes), nargs))
            status = max(status, 1)

        for i in range(nargs):
            # Some manual tweaks necessary...
            if mt_args[i] == 'BOXED' and argtypes[i] in known_boxed:
                continue
            if mt_args[i] == 'ENUM':
                # Anything can be an enum, unfortunately
                continue
            if argtypes[i] != mt_args[i]:
                sys.stderr.write('%s:%s: Marshaller argument #%u type %s '
                                 'does not match given argument type %s.\n'
                                 % (filename, lineno, i+1, mt_args[i], argtypes[i]))
                status = max(status, 1)

sys.exit(status)
