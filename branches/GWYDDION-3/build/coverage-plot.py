#!/usr/bin/python
import re, sys, math

stat_types = {
    'documented': 'good',
    'incomplete': 'fuzzy',
    'not documented': 'bad'
}
total_width = 200

def parse(fh):
    stats = []
    for line in fh:
        percentage, name, lines = line.split()
        stat = { 'r': float(percentage), 'name': name, 'lines': int(lines) }
        stats.append(stat)
    return stats

def format_row(stat):
    rowfmt = """<tr>
<td>%s</td>
<td>%d</td>
<td>%.2f</td>
<td>%s</td>
</tr>"""

    boxfmt = u"""<span class="stat %s" style="padding-right: %dpx;">\ufeff</span>"""

    coverage = stat['r']/100.0
    goodw = int(math.floor(total_width * coverage + 0.5))
    badw = total_width - goodw
    goodbox = boxfmt % ('good', goodw)
    badbox = boxfmt % ('bad', badw)

    return rowfmt % (stat['name'], stat['lines'], 100*coverage, goodbox + badbox)

stats = parse(sys.stdin)

for x in stats:
    print format_row(x).encode('utf-8')

# vim: set ts=4 sw=4 et :
