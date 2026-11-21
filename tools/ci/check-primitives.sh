#!/usr/bin/env bash
# Fails when a shared primitive is copied outside its owner (R-622, R-688): a gcd function, __int128, a type named *Rational,
# *Fraction or *Ratio, a table of the twelve pitch-class names, a scale-step table of integers, a Roman numeral table, the FNV-1a 64
# prime, a run of pitch letters C C D (the sharp spelling table), a pitch name with an octave from MIDI (pitch_class_name followed
# by / 12 - 1), a chord-quality interval set ({0, 4, 7}, {0, 3, 6, 10} and the like), a scan of the numeral letters i v I V, a
# pitch class by ((x % 12) + 12) % 12, and the chord symbol suffixes "m7b5" and "maj7". Matched after stripping comments.
# Scans common/, library/, analyzer/, composer/ and gui/backend/src (C and H files, no tests/ folder). The owners are
# library/model/core/rational.{c,h}, common/hash.c (the FNV prime), library/model/pitch/, library/theory/tonality/
# and knowledge/. A hit that stays is listed in tools/ci/primitive-allow.txt as "<path> <pattern>  # reason, phase" (patterns: gcd,
# int128, type, pitch-names, scale-steps, numerals, fnv, pitch-letters, pitch-octave, chord-intervals, numeral-letters, pc-mod,
# quality-suffix). Prints each other hit as
# "FAIL <file>:<line> <pattern>: <text>".
set -euo pipefail

root="$(cd "$(dirname "$0")/../.." && pwd)"
cd "$root"

python3 - "$root" <<'PY'
import os, re, sys

root = sys.argv[1]
scan = ["common", "library", "analyzer", "composer", "cli", "gui/backend/src"]
owners = ["library/model/core/rational.c", "library/model/core/rational.h", "common/hash.c", "library/model/pitch/", "library/theory/tonality/", "knowledge/"]

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
PITCH_LETTERS = re.compile(r"PITCH_LETTER_C\s*,\s*PITCH_LETTER_[CD]\s*,\s*PITCH_LETTER_D\b")
PITCH_OCTAVE = re.compile(r"pitch_class_name\([^;]*/\s*(?:12|PITCH_SEMITONES_PER_OCTAVE)\)?\s*-\s*1")
CHORD_INTERVALS = re.compile(r"\{\s*0\s*,\s*(?:4\s*,\s*[78]|3\s*,\s*[67])\s*(?:,\s*(?:0|9|10|11)\s*)?\}")
NUMERAL_LETTERS = re.compile(r"==\s*'i'\s*\|\|[^;]*==\s*'I'|strchr\(\"ivIV\"")
PC_MOD = re.compile(r"\(\([^()]*%\s*12\s*\)\s*\+\s*12\s*\)\s*%\s*12")
QUALITY_SUFFIX = re.compile(r'"m7b5"|"maj7"')
COMMENTS = re.compile(r"/\*.*?\*/|//[^\n]*", re.S)
PATTERNS = [("fnv", FNV), ("gcd", GCD), ("int128", INT128), ("type", TYPE), ("pitch-names", PITCH_NAMES), ("scale-steps", SCALE_STEPS), ("numerals", NUMERALS),
            ("pitch-letters", PITCH_LETTERS), ("pitch-octave", PITCH_OCTAVE), ("chord-intervals", CHORD_INTERVALS),
            ("numeral-letters", NUMERAL_LETTERS), ("pc-mod", PC_MOD), ("quality-suffix", QUALITY_SUFFIX)]

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
            # a comment is blanked, not removed, so a hit keeps its line number
            text = COMMENTS.sub(lambda m: re.sub(r"[^\n]", " ", m.group(0)), text)
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
