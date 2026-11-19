#!/usr/bin/env bash
# Fails when a shared primitive is copied outside its owner (R-622): a gcd function, __int128, a type named *Rational, *Fraction
# or *Ratio, a table of the twelve pitch-class names, a scale-step table of integers, a Roman numeral table, or the FNV-1a 64 prime.
# Scans library/, analyzer/, composer/ and gui/backend/src (C and H files, no tests/ folder). The owners are
# library/model/core/rational.{c,h}, library/model/core/content_hash.c (the FNV prime), library/model/pitch/, library/theory/tonality/
# and knowledge/. A hit that stays is listed in tools/ci/primitive-allow.txt as "<path> <pattern>  # reason, phase" (patterns: gcd,
# int128, type, pitch-names, scale-steps, numerals, fnv). Prints each other hit as
# "FAIL <file>:<line> <pattern>: <text>".
set -euo pipefail

root="$(cd "$(dirname "$0")/../.." && pwd)"
cd "$root"

python3 - "$root" <<'PY'
import os, re, sys

root = sys.argv[1]
scan = ["library", "analyzer", "composer", "gui/backend/src"]
owners = ["library/model/core/rational.c", "library/model/core/rational.h", "library/model/core/content_hash.c", "library/model/pitch/", "library/theory/tonality/", "knowledge/"]

allowed = set()
with open(os.path.join(root, "tools/ci/primitive-allow.txt"), encoding="utf-8") as handle:
    for line in handle:
        line = line.split("#", 1)[0].strip()
        if line:
            path, pattern = line.split()[:2]
            allowed.add((path, pattern))

GCD = re.compile(r"^(?:static\s+|inline\s+|extern\s+)*[A-Za-z_][\w\s\*]*?[\s\*]\w*gcd\w*\s*\([^;]*$", re.M)
INT128 = re.compile(r"__int128")
TYPE = re.compile(r"\}\s*\w*(?:Rational|Fraction|Ratio)\s*;|typedef\s[^;{]*\s\w*(?:Rational|Fraction|Ratio)\s*;")
PITCH_NAMES = re.compile(r'"C"\s*,\s*"(?:C#|Db|Cs)"\s*,\s*"D"\s*,\s*"(?:D#|Eb|Ds)"\s*,\s*"E"\s*,\s*"F"')
SCALE_STEPS = re.compile(r"\{\s*(?:0\s*,\s*2\s*,\s*4\s*,\s*5\s*,\s*7\s*,\s*9\s*,\s*11|0\s*,\s*2\s*,\s*3\s*,\s*5\s*,\s*7\s*,\s*8\s*,\s*(?:10|11)|2\s*,\s*2\s*,\s*1\s*,\s*2\s*,\s*2\s*,\s*2\s*,\s*1)\s*\}")
NUMERALS = re.compile(r'"I"\s*,\s*"II"\s*,\s*"III"\s*,\s*"IV"|"i"\s*,\s*"ii"\s*,\s*"iii"\s*,\s*"iv"')
FNV = re.compile(r"1099511628211|0x100000001b3", re.I)
PATTERNS = [("fnv", FNV), ("gcd", GCD), ("int128", INT128), ("type", TYPE), ("pitch-names", PITCH_NAMES), ("scale-steps", SCALE_STEPS), ("numerals", NUMERALS)]

failed = 0
for top in scan:
    for directory, names, files in os.walk(os.path.join(root, top)):
        names[:] = [n for n in names if n not in ("tests", "build")]
        for name in files:
            if not name.endswith((".c", ".h")):
                continue
            path = os.path.relpath(os.path.join(directory, name), root)
            if any(path == o or (o.endswith("/") and path.startswith(o)) for o in owners):
                continue
            with open(os.path.join(root, path), encoding="utf-8", errors="replace") as handle:
                text = handle.read()
            for pattern, regex in PATTERNS:
                if (path, pattern) in allowed:
                    continue
                for match in regex.finditer(text):
                    line = text.count("\n", 0, match.start()) + 1
                    shown = text[match.start():].split("\n", 1)[0].strip()[:100]
                    print("FAIL %s:%d %s: %s" % (path, line, pattern, shown), file=sys.stderr)
                    failed = 1
sys.exit(failed)
PY
status=$?
if [ "$status" -ne 0 ]; then
    echo "A shared primitive is copied outside its owner (R-622): call rational.h, library/model/pitch or library/theory/tonality, or list the file in tools/ci/primitive-allow.txt with a reason." >&2
    exit 1
fi
echo "OK no copy of a shared primitive outside its owner"
