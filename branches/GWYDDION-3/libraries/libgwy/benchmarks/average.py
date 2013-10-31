#!/usr/bin/python
import sys
from math import sqrt

abscissacols = [int(x) for x in sys.argv[1].split()]
files = [file(x) for x in sys.argv[2:]]

while True:
    try:
        rows = [[float(y) for y in x.next().strip().split()] for x in files]
    except StopIteration:
        break

    if not rows:
        sys.stdout.write('\n')

    fields = zip(*rows)
    out = []
    for i, f in enumerate(fields):
        if i+1 in abscissacols:
            assert max(f) == min(f)
            out.append(f[0])
        else:
            avg = sum(f)/len(f)
            rms = sqrt(sum((x - avg)**2 for x in f)/len(f))
            out.append(avg)
            out.append(rms)
    sys.stdout.write(' '.join('%g' % x for x in out) + '\n')

