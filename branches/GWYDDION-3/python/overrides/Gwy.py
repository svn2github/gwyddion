import sys
from gi.repository import GObject
from gi.overrides import override
from gi.module import get_introspection_module

Gwy = get_introspection_module('Gwy')

__all__ = []

class Field(Gwy.Field):
    def __new__(klass, *args, **kwargs):
        if not args:
            return Gwy.Field()

        n = len(args)
        t0 = type(args[0])
        if n == 1 and issubclass(t0, Gwy.MaskField):
            return Gwy.Field.new_from_mask(args[0], 0.0, 1.0)
        if n == 2 and issubclass(t0, Gwy.Field):
            return Gwy.Field.new_alike(*args)
        if n == 3 and issubclass(t0, Gwy.MaskField):
            return Gwy.Field.new_from_mask(*args)
        if n == 3 and issubclass(t0, int):
            return Gwy.Field.new_sized(*args)
        if n == 3 and issubclass(t0, Gwy.Field):
            t2 = type(args[2])
            if issubclass(t2, Gwy.PlaneCongruence):
                return Gwy.Field.new_congruent(*args)
            return Gwy.Field.new_part(*args)
        if n == 4 and issubclass(t0, Gwy.Field):
            return Gwy.Field.new_resampled(*args)

        raise TypeError('Wrong number or type of constructor arguments')

    def __init__(self, *args, **kwargs):
        super(Field, self).__init__(**kwargs)

Field = override(Field)
__all__.append('Field')

