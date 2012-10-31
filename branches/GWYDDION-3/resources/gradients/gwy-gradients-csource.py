#!/usr/bin/python
import sys, os

srcdir = sys.argv[1]
varname = sys.argv[2]

names = []
points = []
lengths = []
for filename in sys.argv[3:]:
    npoints = 0
    for i, line in enumerate(file(os.path.join(srcdir, filename))):
        line = line.strip()
        if i == 0:
            assert line == 'Gwyddion3 resource GwyGradient'
            continue
        if i == 1:
            assert line.startswith('name')
            name = line[5:].strip()
            continue
        if not line:
            continue

        line = line.split()
        x, rgba = line[0], line[1:]
        line = '    { ' + x + ', { ' + ', '.join(rgba) + ' } }'
        npoints += 1
        points.append(line)
    assert npoints >= 2
    lengths.append(npoints)
    names.append(name)

lenvar = '%s_nitems' % varname
sys.stdout.write('enum { %s = %u };\n' % (lenvar, len(names)))

sys.stdout.write('static const guint %s_lengths[%s] = {\n'
                 % (varname, lenvar))
sys.stdout.write('    %s\n' % (', '.join('%u' % n for n in lengths)))
sys.stdout.write('};\n')

sys.stdout.write('static const gchar %s_names[] =\n' % varname)
sys.stdout.write('    "%s"\n' % ('\\000"\n    "'.join(names)))
sys.stdout.write(';\n')

sys.stdout.write('static const GwyGradientPoint %s_data[] = {\n'
                 % varname)
sys.stdout.write(',\n'.join(points) + '\n')
sys.stdout.write('};\n')
