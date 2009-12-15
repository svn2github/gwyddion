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

def format_row(stat, counter=[False]):
    rowfmt = """<tr%s>
<td class='left'>%s</td>
<td>%d</td>
<td>%d</td>
<td>%.2f</td>
<td class='left'>%s</td>
</tr>"""

    boxfmt = u"""<span class="stat %s" style="padding-right: %dpx;">\ufeff</span>"""

    cvg = stat['r']
    coverage = cvg/100.0
    lines = stat['lines']
    name = stat['name']
    factor = 3.0;
    if name.endswith('.c'):
        factor /= 8
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

stats = parse(sys.stdin)

for x in stats:
    print format_row(x).encode('utf-8')

# vim: set ts=4 sw=4 et :
