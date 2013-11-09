import sys
from gi.repository import GObject
from gi.overrides import override
from gi.module import get_introspection_module

Gwy = get_introspection_module('Gwy')

__all__ = []

def match_signature(args, types):
    if len(args) != len(types):
        return False

    for pair in zip(args, types):
        if not issubclass(pair[0], pair[1]):
            return False
    return True

class Unit(Gwy.Unit):
    def __nonzero__(self):
        return not self.is_empty()

Unit = override(Unit)
__all__.append('Unit')
