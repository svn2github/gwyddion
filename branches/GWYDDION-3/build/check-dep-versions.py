#!/usr/bin/python
import sys, re

max_versions = {
    'cairo': (1, 10),
    'glib': (2, 36),
    'gobject': (2, 36),
    'gio': (2, 36),
    'pango': (1, 32),
    'gdk3': (3, 6),
    'gdk-pixbuf': (2, 22),
    'gtk3': (3, 6),
}

accept = frozenset([
])

req_re = re.compile(r'^(?P<location>\S+)\s+'
                    r'(?P<package>\S+?)-(?P<major>[0-9]+)\.(?P<minor>[0-9]+)\s+'
                    r'required for (?P<reason>.*)$')

exitstatus = 0
for line in sys.stdin:
    line = line.strip()

    if line in accept:
        continue

    m = req_re.match(line)
    if not m:
        sys.stderr.write('Cannot parse: %s\n' % line)
        exitstatus = 1
        continue

    package = m.group('package')
    major = int(m.group('major'))
    minor = int(m.group('minor'))
    if package not in max_versions:
        sys.stderr.write('Unknown package %s\n' % package)
        exitstatus = 1
        continue

    ver = max_versions[package]
    if major > ver[0] or (major == ver[0] and minor > ver[1]):
        sys.stderr.write(line + '\n')
        exitstatus = 1
        continue

sys.exit(exitstatus)
