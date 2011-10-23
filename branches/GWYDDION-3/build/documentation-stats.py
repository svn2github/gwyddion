#!/usr/bin/python
# $Id$
import re, sys
from math import floor

stat_types = {
    'documented': 'good',
    'incomplete': 'fuzzy',
    'not documented': 'bad'
}
total_width = 200

def parse(filename):
    group = re.sub(r'.*/(?P<group>\w+)-undocumented\.txt', r'\g<group>', filename)
    stat = {'group': group}
    total = 0
    for line in file(filename):
        line.strip()
        m = re.match(r'^(?P<count>\d+)( symbols?)? (?P<what>[a-z ]+).$', line)
        if m:
            count = int(m.group('count'))
            stat[m.group('what')] = count
            total += count
    stat['total'] = total
    return stat

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

stats = [parse(x) for x in sys.argv[1:]]

print """<table summary="Documentation statistics" class="stats">
<thead>
<tr>
  <th class="odd">Library</th>
  <th>Fully</th><th class="odd">%</th>
  <th>Partially</th><th class="odd">%</th>
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
