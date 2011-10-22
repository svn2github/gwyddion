#!/usr/bin/python
# $Id$
import re, sys
from math import floor

stat_types = {
    'translated': 'good',
    'fuzzy': 'fuzzy',
    'untranslated': 'bad'
}
total_width = 200

def parse(filename, domain):
    stats = []
    for line in file(filename):
        m = re.match(r'TRANSLATION\s+(?P<domain>\S+)\s+(?P<content>.*)\n', line)
        if not m:
            continue
        if m.group('domain') != domain:
            continue
        line = m.group('content')
        m = re.match(r'(?P<group>[a-zA-Z_@.]+):', line)
        if not m:
            sys.stderr.write('Malformed TRANSLATION line: %s\n' % line)
            continue

        stat = {'group': m.group('group')}

        if stat['group'] == 'total':
            continue
        else:
            sum = 0
            for x in stat_types:
                m = re.search(r'\b(?P<count>\d+) %s (message|translation)' % x,
                              line)
                if m:
                    stat[x] = int(m.group('count'))
                    sum += stat[x]
            stat['total'] = sum
        stats.append(stat)

    return stats

# Redistribute rounding errors in box widths to achieve fixed total width
def balance(ratios, total_width):
    q = 1.0/total_width
    widths = dict((k, int(floor(v/q + 0.5))) for k, v in ratios.items())
    errs = [(q*v - ratios[k], k) for k, v in widths.items()]
    sw = sum(x for x in widths.values())
    while sw != total_width:
        errs.sort()
        if sw > total_width:
            k = errs[-1][1]
            widths[k] -= 1
            sw -= 1
            errs[-1] = (errs[-1][0] - q, k)
        else:
            k = errs[0][1]
            widths[k] += 1
            sw += 1
            errs[0] = (errs[0][0] + q, k)
    return widths

def format_row(stat):
    rowfmt = """<tr>
<th class="odd">%s</th>
%s
<td>%d</td>
<td>%s</td>
</tr>"""

    statfmt = """<td>%d</td><td class="odd">%.2f</td>""";

    boxfmt = u"""<span class="stat %s" style="padding-right: %dpx;">\ufeff</span>""";

    box = []
    stats = []
    sum_ = float(stat['total'])

    r = dict((x, stat[x]/sum_) for x in stat_types if x in stat)
    w = balance(r, total_width)

    t = dict([(z[1], z[0]) for z in stat_types.items()])
    for x in t['good'], t['fuzzy'], t['bad']:
        if not stat.has_key(x):
            stats.append(statfmt % (0, 0.0))
            continue
        box.append(boxfmt % (stat_types[x], w[x]))
        stats.append(statfmt % (stat[x], r[x]*100.0))

    return rowfmt % (stat['group'], '\n'.join(stats), sum_, ''.join(box))

filename = sys.argv[1]
domain = sys.argv[2]
stats = parse(filename, domain)

print '<table summary="Translation statistics for ' + domain + """" class="stats">
<thead>
<tr>
  <th class="odd">Language</th>
  <th>Translated</th><th class="odd">%</th>
  <th>Fuzzy</th><th class="odd">%</th>
  <th>Missing</th><th class="odd">%</th>
  <th>Total</th>
  <th>Graph</th>
</tr>
</thead>
<tbody>"""

for x in stats:
    print format_row(x).encode('utf-8')

print """</tbody>
</table>"""
