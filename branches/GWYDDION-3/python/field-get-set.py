#!/usr/bin/python
# vim: set fileencoding=utf-8 :
# Measure the execution speed of various method how to multiply all field
# values with 1.1.  The speed can differ by factor 10⁴...
import typelibpath
from timeit import timeit
from gi.repository import Gwy

field = Gwy.Field.new_sized(400, 400, False)

def time_mult(name, code, n):
    field.fill_full(1.0)
    t = timeit(code, 'from __main__ import field', number=n)
    print '=====[ %s ]=====' % name
    print 'Code:'
    print code
    print
    print 'Time: %g ms' % (1e3*t/n)

time_mult('lib',
          'field.multiply_full(1.1)',
          1000)

time_mult('set-data',
          'd = [1.1*x for x in field.get_data(None, None, 0)]\n'
          'field.set_data(None, None, 0, d)',
          10)

time_mult('set-data-full',
          'd = [1.1*x for x in field.get_data_full()]\n'
          'field.set_data_full(d)',
          10)

time_mult('set-data-inline',
          'field.set_data(None, None, 0, [1.1*x for x in field.get_data(None, None, 0)])',
          10)

time_mult('set-data-full-inline',
          'field.set_data_full([1.1*x for x in field.get_data_full()])',
          10)

time_mult('apply-lambda',
          'field.apply_func(None, None, 0, lambda x, y: 1.1*x, None)',
          1)

time_mult('get-set',
          'xr, yr = field.xres, field.yres\n'
          'for i in range(yr):\n'
          '    for j in range(xr):\n'
          '        field.set(j, i, 1.1*field.get(j, i))',
          1)

