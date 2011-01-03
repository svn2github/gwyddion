#!/usr/bin/python
import re, sys, math

stat_types = {
    'documented': 'good',
    'incomplete': 'fuzzy',
    'not documented': 'bad'
}

def parse(fh):
    stats = []
    for line in fh:
        percentage, name, lines = line.split()
        stat = { 'r': float(percentage), 'name': name, 'lines': int(lines) }
        stats.append(stat)
    return stats

def xround(x):
    return int(math.floor(x + 0.5))

def total(stats):
    lines = 0
    rlines = 0.0
    for row in stats:
        lines += row['lines']
        rlines += row['lines'] * row['r']
    stat = { 'r': rlines/lines, 'name': 'Total', 'lines': lines }
    return stat

def format_row(stat, counter=[False]):
    rowfmt = """<tr%s>
<td class='left'>%s</td>
<td>%d</td>
<td>%d</td>
<td>%.2f</td>
<td class='left'>%s</td>
</tr>
"""

    boxfmt = u"""<span class="stat %s" style="padding-right: %dpx;">\ufeff</span>"""

    cvg = stat['r']
    coverage = cvg/100.0
    lines = stat['lines']
    name = stat['name']
    factor = 3.0
    if entire_files:
        factor /= 8
    if name == 'Total':
        factor /= 8
    if entire_files and name == 'Total':
        factor /= 3
    if name == 'Total' and totalname:
        name += ' ' + totalname
    badlines = xround(lines * (1.0 - coverage))
    badw = xround(factor * badlines)
    goodw = xround(factor*(lines - badlines))
    badbox = boxfmt % ('bad', badw)
    goodbox = boxfmt % ('good', goodw)
    cls = ''
    if counter[0]:
        cls = " class='odd'"
    counter[0] = not counter[0]

    return rowfmt % (cls, name, lines, badlines, 100*coverage, goodbox + badbox)

totalname = None
if len(sys.argv) > 1:
    totalname = sys.argv[1]

stats = parse(sys.stdin)
entire_files = len([x for x in stats if x['name'].endswith('.c')])

sys.stdout.write(format_row(total(stats)).encode('utf-8'))
for x in stats:
    sys.stdout.write(format_row(x).encode('utf-8'))

# vim: set ts=4 sw=4 et :
