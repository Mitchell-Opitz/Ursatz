#!/usr/bin/env python3
"""Plants a violation in a temporary tree and checks that a boundary checker finds it, then removes it and checks that the checker
passes: a checker that never fails would pass the real tree too.

usage: python3 tests/check_self_test.py layer|primitives <repository root>
  layer       cmake/check_layer_table.py: a lower module that includes a higher one it does not use
  primitives  tools/ci/check-primitives.sh: a gcd function outside its owner
Exit 0 when the checker fails with its message on the planted tree and passes on the clean one."""
import os
import shutil
import subprocess
import sys
import tempfile


def write(path, text):
    os.makedirs(os.path.dirname(path), exist_ok=True)
    with open(path, "w", encoding="utf-8") as handle:
        handle.write(text)


def run(command):
    result = subprocess.run(command, capture_output=True, text=True)
    return result.returncode, result.stdout + result.stderr


def check(label, command, planted, message, clean):
    """planted() writes the violation, clean() removes it; the checker fails with message, then passes."""
    planted()
    code, output = run(command)
    if code != 1 or message not in output:
        sys.exit(f"{label}: the planted violation was not found (exit {code}): {output}")
    clean()
    code, output = run(command)
    if code != 0:
        sys.exit(f"{label}: the clean tree failed (exit {code}): {output}")
    print(f"{label}: ok")


def layer(root, tmp):
    write(os.path.join(tmp, "cmake/layer-table.txt"),
          "module low low prefix low/\nmodule high high prefix high/\nuses high low\n")
    write(os.path.join(tmp, "low/a.h"), "int a(void);\n")
    write(os.path.join(tmp, "high/b.h"), "int b(void);\n")
    write(os.path.join(tmp, "high/c.c"), '#include "low/a.h"\nint c(void) { return a(); }\n')
    low_c = os.path.join(tmp, "low/a.c")
    check("layer table",
          [sys.executable, os.path.join(root, "cmake/check_layer_table.py"), tmp],
          lambda: write(low_c, '#include "high/b.h"\nint a(void) { return b(); }\n'),
          'low includes "high/b.h" (high), which it does not use',
          lambda: write(low_c, "int a(void) { return 1; }\n"))


def primitives(root, tmp):
    write(os.path.join(tmp, "tools/ci/primitive-allow.txt"), "")
    shutil.copy(os.path.join(root, "tools/ci/check-primitives.sh"), os.path.join(tmp, "tools/ci/check-primitives.sh"))
    source = os.path.join(tmp, "library/x.c")
    # A definition over two lines: the pattern does not match one with a semicolon on its line.
    check("primitives",
          ["bash", os.path.join(tmp, "tools/ci/check-primitives.sh")],
          lambda: write(source, "static long gcd(long a, long b)\n{ return b == 0 ? a : gcd(b, a % b); }\n"),
          "FAIL library/x.c:1 gcd",
          lambda: write(source, "int ok(void) { return 1; }\n"))


def main():
    if len(sys.argv) != 3 or sys.argv[1] not in ("layer", "primitives"):
        sys.exit(__doc__)
    root = os.path.abspath(sys.argv[2])
    with tempfile.TemporaryDirectory() as tmp:
        (layer if sys.argv[1] == "layer" else primitives)(root, tmp)


main()
