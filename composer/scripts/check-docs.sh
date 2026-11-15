#!/usr/bin/env bash
# Docs check: README size, doc size, relative links and anchors, docs index coverage.
# Exit 0 with no output when clean.
cd "$(dirname "$0")/.." || exit 1
exec python3 - <<'PY'
import os, re, sys

EXCL = {"tests", "external", "build", ".git", "node_modules", "_deps"}
README_MAX, DOC_MAX = 10 * 1024, 20 * 1024
errors = []

files = []
for root, dirs, names in os.walk("."):
    dirs[:] = [d for d in dirs if d not in EXCL and not d.startswith("build")]
    for n in names:
        if n.endswith(".md"):
            files.append(os.path.normpath(os.path.join(root, n)))
files.sort()

def is_readme(p):
    return p == "README.md" or (p.startswith("frameworks/") and os.path.basename(p) == "README.md"
                                and p.count("/") == 2)

for p in files:
    size = os.path.getsize(p)
    if is_readme(p):
        if size > README_MAX:
            errors.append(f"{p}: README is {size} bytes, limit {README_MAX}")
    elif size > DOC_MAX:
        errors.append(f"{p}: doc is {size} bytes, limit {DOC_MAX}")

def slug(h):
    h = re.sub(r"`", "", h.strip().lower())
    h = re.sub(r"\[([^\]]*)\]\([^)]*\)", r"\1", h)
    h = re.sub(r"[^\w\- ]", "", h, flags=re.UNICODE)
    return h.replace(" ", "-")

anchor_cache = {}
def anchors(p):
    if p in anchor_cache:
        return anchor_cache[p]
    out, seen, fence = set(), {}, False
    try:
        text = open(p, encoding="utf-8").read()
    except OSError:
        anchor_cache[p] = out
        return out
    for line in text.splitlines():
        if line.lstrip().startswith(("```", "~~~")):
            fence = not fence
            continue
        if fence:
            continue
        m = re.match(r"^#{1,6}\s+(.*?)\s*#*\s*$", line)
        if m:
            s = slug(m.group(1))
            n = seen.get(s, 0)
            seen[s] = n + 1
            out.add(s if n == 0 else f"{s}-{n}")
    anchor_cache[p] = out
    return out

LINK = re.compile(r"(?<!\!)\[[^\]]*\]\(([^)\s]+)(?:\s+\"[^\"]*\")?\)")
for p in files:
    fence = False
    for i, line in enumerate(open(p, encoding="utf-8").read().splitlines(), 1):
        if line.lstrip().startswith(("```", "~~~")):
            fence = not fence
            continue
        if fence:
            continue
        line = re.sub(r"`[^`]*`", "", line)
        for m in LINK.finditer(line):
            t = m.group(1)
            if re.match(r"^[a-z][a-z0-9+.-]*:", t) or t.startswith("//"):
                continue
            path, _, frag = t.partition("#")
            target = p if path == "" else os.path.normpath(os.path.join(os.path.dirname(p), path))
            if not os.path.exists(target):
                errors.append(f"{p}:{i}: broken link {t}")
            elif frag and target.endswith(".md") and frag.lower() not in anchors(target):
                errors.append(f"{p}:{i}: broken anchor {t}")

index = "docs/README.md"
if os.path.exists("docs"):
    if not os.path.exists(index):
        errors.append("docs/README.md: missing")
    else:
        linked = set()
        for m in LINK.finditer(open(index, encoding="utf-8").read()):
            t = m.group(1).partition("#")[0]
            if t and not re.match(r"^[a-z][a-z0-9+.-]*:", t):
                linked.add(os.path.normpath(os.path.join("docs", t)))
        for p in files:
            if p.startswith("docs/") and p != index and p not in linked:
                errors.append(f"{p}: missing from docs/README.md")

if errors:
    print("\n".join(errors))
    sys.exit(1)
PY
