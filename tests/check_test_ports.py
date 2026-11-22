#!/usr/bin/env python3
"""Fails when two test sources use the same TCP port (A20261121-45, A20261121-1): ctest -j runs them together.
Usage: python3 tests/check_test_ports.py <repository root> (registered as the test test_ports_unique). Scans the C sources of gui/backend/tests, workshop/tests, cli/tests
and tests/ for port literals 18000 to 18999, comments stripped; one file may use its port many times."""
import collections, os, re, sys

root = sys.argv[1]
COMMENTS = re.compile(r"/\*.*?\*/|//[^\n]*", re.S)
PORT = re.compile(r'(?<![\w.])"?(18[0-9]{3})"?(?![\w.])')
seen = collections.defaultdict(set)
for top in ("gui/backend/tests", "workshop/tests", "cli/tests", "tests"):
    for directory, _, names in os.walk(os.path.join(root, top)):
        for name in names:
            if name.endswith((".c", ".h")):
                path = os.path.join(directory, name)
                with open(path, encoding="utf-8", errors="replace") as handle:
                    text = COMMENTS.sub("", handle.read())
                for match in PORT.finditer(text):
                    seen[match.group(1)].add(os.path.relpath(path, root))
clashes = {port: sorted(files) for port, files in seen.items() if len(files) > 1}
for port, files in sorted(clashes.items()):
    print("port %s is used by %s" % (port, ", ".join(files)))
if clashes:
    sys.exit(1)
print("OK %d test ports, each in one file" % len(seen))
