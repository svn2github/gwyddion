#!/usr/bin/python
import sys, os, re

if len(sys.argv) < 4:
  print "Usage: check-library-headers.py: LIBRARY MAIN-HEADER.H HEADERS..."
  sys.exit(1)

library = sys.argv[1]
main_header = sys.argv[2]
headers = set(os.path.basename(x) for x in sys.argv[3:])

main_header_text = file(main_header).read()

included_headers = set()
for m in re.finditer(r'(?ms)^#\s*include\s*<'
                     + library + r'/(?P<name>[^>]+\.h)>\s*$',
                     main_header_text):
    header = m.group('name')
    included_headers.add(header)

missing_headers = headers - included_headers
for header in missing_headers:
    sys.stderr.write('Library header %s lacks #include <%s/%s>\n'
                     % (main_header, library, header))
extra_headers = included_headers - headers
for header in extra_headers:
    sys.stderr.write('Library header %s contains extra #include <%s/%s>\n'
                     % (main_header, library, header))

ok = not missing_headers and not extra_headers
sys.exit(0 if ok else 1)

