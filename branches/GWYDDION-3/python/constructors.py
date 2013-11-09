#!/usr/bin/python
# vim: set fileencoding=utf-8 :
# Check whether new-alike and similar functions work as constructors.
import typelibpath
from gi.repository import Gwy

field1 = Gwy.Field()
mask = Gwy.MaskField()

field2 = Gwy.Field(30, 30, True)

field3 = Gwy.Field(mask, 0.0, 1.0)
field4 = Gwy.Field(mask)

field5 = Gwy.Field(field2, True)

field6 = Gwy.Field(field2, 50, 50, Gwy.Interpolation.LINEAR)

fpart = Gwy.FieldPart()
fpart.col,fpart.row, fpart.width, fpart.height = 0, 0, 8, 12
field7 = Gwy.Field(field2, fpart, True)
assert field7.xres == fpart.width
assert field7.yres == fpart.height

field8 = Gwy.Field(field2, fpart, Gwy.PlaneCongruence.MIRROR_DIAGONALLY)
assert field8.xres == fpart.height
assert field8.yres == fpart.width
