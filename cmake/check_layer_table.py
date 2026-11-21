#!/usr/bin/env python3
"""Layer table check (spec/target-architecture.md 12.2): every C file of the repository belongs to one module of
cmake/layer-table.txt, and every #include it makes names its own module or one the table lets it use.

usage: python3 cmake/check_layer_table.py <repository root> [<staged header manifest>]

Rows of the table, one per line; # starts a comment and " -- " starts the reason:
  module <name> <path>... [prefix <include>...] [root <dir>...]
        C files under each path belong to <name> (the deepest listed path wins). An include starting with a prefix
        (one ending in /) or equal to it names <name>. <name>'s own includes are also looked up in each root dir.
  group <name> <module or group>...   a name for several modules, usable wherever a module is
  uses <name> <module|group|header>...  <name> may include these; a token ending in .h allows that one header. A token no
        include of <name>'s own files needs is an error (test modules, every path in a tests folder, and free modules are exempt)
  like <name> <module>                <name> may also include <module> and everything <module> may include
  allow <file> <module|group|header>...  one file's extra edge
  only <module|header> <path>...      only files under these paths may include this module or header
  target <name> <module or group>...  the modules one CMake target builds (an include within one target is free)
  staged <name>...                    a library header these modules include is one the build stages (manifest), unless
                                      it stays within one target; an application is in no target, so it always needs one
  generated <name> <include>...       headers the build writes for <name>
  forbid <path> <regex>               no C or CMake file under <path> matches <regex> (whole file, ^ and $ per line;
                                      write \\s, not a space)
  free <name>...                      these modules' own includes are not checked
An include is looked up beside the including file (for "..."), then in the module's root dirs, then by prefix.
One found nowhere is a system header when written with <...> and an error when written with "...".
With the staged header manifest given, a second rule reports every staged header that no include from outside its
target reaches, directly or through another staged header: "<header> is staged but nothing outside <target> includes
it". An include from a library test does not count (a test sees the library folders, not only the staged headers);
an include from an application, an application test, a root test or a library file of another target does.
Exit 0 when clean, 1 with one line per problem, 2 on a bad table.
"""
import collections
import os
import re
import sys

SKIP_DIRS = {".git", "node_modules"}
INCLUDE = re.compile(r'^\s*#\s*include\s*([<"])([^>"]+)[>"]')


def bad_table(lineno, text):
    print(f"layer-table: cmake/layer-table.txt:{lineno}: {text}", file=sys.stderr)
    sys.exit(2)


def read_table(path):
    t = {"module": {}, "prefix": [], "root": {}, "group": {}, "uses": {}, "like": {}, "allow": {}, "only": [],
         "staged": set(), "generated": {}, "forbid": [], "free": set(), "target": {}, "uses_at": {}}
    refs = []  # (lineno, token) to check once every module and group is known
    with open(path, encoding="utf-8") as f:
        for lineno, raw in enumerate(f, 1):
            line, _, why = raw.partition(" -- ")
            if not line.lstrip().startswith("forbid"):
                line = line.split("#", 1)[0]
            line, why = line.strip(), why.strip()
            if not line:
                continue
            kind, *args = line.split()
            if kind == "module" and len(args) >= 2:
                name, mode = args[0], "dir"
                for a in args[1:]:
                    if a in ("prefix", "root"):
                        mode = a
                    elif mode == "dir":
                        t["module"].setdefault(name, []).append(a.rstrip("/"))
                    elif mode == "prefix":
                        t["prefix"].append((a, name))
                    else:
                        t["root"].setdefault(name, []).append(a.rstrip("/"))
                if name not in t["module"]:
                    bad_table(lineno, f"module {name} has no path")
            elif kind == "group" and len(args) >= 2:
                t["group"][args[0]] = args[1:]
                refs += [(lineno, a) for a in args[1:] if not a.endswith(".h")]
            elif kind in ("uses", "allow") and len(args) >= 2:
                t[kind].setdefault(args[0], set()).update(args[1:])
                refs += [(lineno, a) for a in args[1:] if not a.endswith(".h")]
                if kind == "uses":
                    refs.append((lineno, args[0]))
                    for a in args[1:]:
                        t["uses_at"].setdefault((args[0], a), lineno)
            elif kind == "like" and len(args) == 2:
                t["like"].setdefault(args[0], []).append(args[1])
                refs += [(lineno, args[0]), (lineno, args[1])]
            elif kind == "only" and len(args) >= 2:
                t["only"].append((args[0], [a.rstrip("/") for a in args[1:]]))
            elif kind == "target" and len(args) >= 2:
                t["target"].setdefault(args[0], []).extend(args[1:])
                refs += [(lineno, a) for a in args[1:]]
            elif kind in ("staged", "free") and args:
                t[kind].update(args)
                refs += [(lineno, a) for a in args]
            elif kind == "generated" and len(args) >= 2:
                t["generated"].setdefault(args[0], set()).update(args[1:])
                refs.append((lineno, args[0]))
            elif kind == "forbid" and len(args) >= 2:
                t["forbid"].append((args[0].rstrip("/"), re.compile(" ".join(args[1:]), re.MULTILINE), why))
            else:
                bad_table(lineno, f"cannot read: {raw.strip()}")
    for lineno, token in refs:
        if token not in t["module"] and token not in t["group"]:
            bad_table(lineno, f"{token} is not a module or group")
    t["prefix"].sort(key=lambda pair: -len(pair[0]))
    return t


def expand(t, tokens, seen=()):
    out = set()
    for token in tokens:
        if token in t["group"] and token not in seen:
            out |= expand(t, t["group"][token], seen + (token,))
        else:
            out.add(token)
    return out


def allowed_sets(t):
    """For each module: what its uses and likes let it include, to any depth of like."""
    out = {}

    def walk(name, seen):
        if name in out:
            return out[name]
        allowed = expand(t, t["uses"].get(name, set()))
        for other in t["like"].get(name, []):
            if other not in seen:
                allowed |= {other} | walk(other, seen | {name})
        out[name] = allowed
        return allowed

    for name in t["module"]:
        walk(name, set())
    return out


def read_manifest(path):
    """The staged headers, and the target each is staged by."""
    staged = {}
    with open(path, encoding="utf-8") as f:
        for line in f:
            parts = line.rstrip("\n").split("|")
            if len(parts) >= 4 and parts[3]:
                for header in parts[3].split(","):
                    staged[header] = parts[0]
    return staged


def main():
    if len(sys.argv) not in (2, 3):
        print("usage: python3 cmake/check_layer_table.py <repository root> [<staged header manifest>]", file=sys.stderr)
        return 2
    root = os.path.abspath(sys.argv[1])
    t = read_table(os.path.join(root, "cmake", "layer-table.txt"))
    staged_headers = read_manifest(sys.argv[2]) if len(sys.argv) == 3 else None
    staged_modules = expand(t, t["staged"])
    target_of = {}
    for target_name, members in t["target"].items():
        for module in expand(t, members):
            target_of[module] = target_name
    allowed_by = allowed_sets(t)
    owners = sorted(((p, n) for n, paths in t["module"].items() for p in paths), key=lambda pair: -len(pair[0]))
    library = {n for n, paths in t["module"].items() if all(p.startswith("library/") for p in paths)}

    def under(rel, paths):
        return any(rel == p or rel.startswith(p + "/") for p in paths)

    def owner(rel):
        for path, name in owners:
            if under(rel, [path]):
                return name
        return None

    c_files, cmake_files = [], []
    for dirpath, dirs, names in os.walk(root):
        rel_dir = os.path.relpath(dirpath, root)
        dirs[:] = sorted(d for d in dirs if d not in SKIP_DIRS and not (rel_dir == "." and d.startswith("build")))
        for n in sorted(names):
            rel = os.path.normpath(os.path.join(rel_dir, n))
            if n.endswith((".c", ".h")):
                c_files.append(rel)
            elif n == "CMakeLists.txt" or n.endswith(".cmake"):
                cmake_files.append(rel)

    def is_test_module(name):
        return all(p == "tests" or "tests" in p.split("/") for p in t["module"][name])

    def is_library_test(rel):
        return rel.startswith("library/") and ("/tests/" in rel or rel.startswith("library/tests/"))

    errors = []
    used = set()  # (module, uses token) some include of the module's own files needs
    covers = {token: expand(t, {token}) for tokens in t["uses"].values() for token in tokens}
    included_from_outside = set()  # staged headers an include from outside their target names
    includes_of = collections.defaultdict(set)  # file -> staged headers it includes
    for rel in c_files + cmake_files:
        rules = [(regex, why) for path, regex, why in t["forbid"] if under(rel, [path])]
        if rules:
            with open(os.path.join(root, rel), encoding="utf-8", errors="replace") as f:
                text = f.read()
            for regex, why in rules:
                for m in regex.finditer(text):
                    errors.append(f"{rel}:{text.count(chr(10), 0, m.start()) + 1}: matches /{regex.pattern}/ ({why})")

    for rel in c_files:
        name = owner(rel)
        if name is None:
            errors.append(f"{rel}: in no module of cmake/layer-table.txt")
            continue
        if name in t["free"]:
            continue
        allowed = allowed_by[name] | expand(t, t["allow"].get(rel, set()))
        with open(os.path.join(root, rel), encoding="utf-8", errors="replace") as f:
            lines = f.read().splitlines()
        for i, line in enumerate(lines, 1):
            m = INCLUDE.match(line)
            if not m:
                continue
            delim, inc = m.groups()
            target, target_path = None, None
            candidates = [os.path.join(os.path.dirname(rel), inc)] if delim == '"' else []
            candidates += [os.path.join(r, inc) for r in t["root"].get(name, [])]
            for c in candidates:
                c = os.path.normpath(c)
                if os.path.isfile(os.path.join(root, c)):
                    target, target_path = owner(c), c
                    break
            if target is None and inc in t["generated"].get(name, set()):
                continue
            if target is None:
                for prefix, pname in t["prefix"]:
                    if inc == prefix or (prefix.endswith("/") and inc.startswith(prefix)):
                        target = pname
                        break
            if target is None:
                if delim == '"':
                    errors.append(f'{rel}:{i}: "{inc}" is found nowhere')
                continue
            for what, paths in t["only"]:
                if what in (target, inc, target_path) and not under(rel, paths):
                    errors.append(f'{rel}:{i}: includes "{inc}", which only {", ".join(paths)} may include')
            header = (target_path[len("library/"):] if target_path and target_path.startswith("library/") else inc)
            if staged_headers is not None and header in staged_headers and target in library:
                includes_of[rel].add(header)
                if (target_of.get(name) != staged_headers[header] and not is_library_test(rel)
                        and (target_of.get(name) is not None or not name.startswith("tests-"))):
                    included_from_outside.add(header)
            if target == name:
                continue
            for token in t["uses"].get(name, ()):
                if token in (inc, target_path) or target in covers[token]:
                    used.add((name, token))
            if target not in allowed and inc not in allowed and target_path not in allowed:
                errors.append(f'{rel}:{i}: {name} includes "{inc}" ({target}), which it does not use')
            elif (staged_headers is not None and name in staged_modules and target in library and target_path is None
                    and inc not in staged_headers and target_of.get(name) != target_of.get(target)):
                errors.append(f'{rel}:{i}: "{inc}" is not a staged library header')
    for (name, token), lineno in sorted(t["uses_at"].items(), key=lambda pair: pair[1]):
        if (name, token) not in used and name not in t["free"] and not is_test_module(name):
            errors.append(f"cmake/layer-table.txt:{lineno}: uses {name} {token}: no include of {name} needs it")
    if staged_headers is not None and t["target"]:
        reached = set()
        pending = sorted(included_from_outside)
        while pending:
            header = pending.pop()
            if header in reached:
                continue
            reached.add(header)
            pending += sorted(includes_of.get(os.path.join("library", header), set()) - reached)
        for header in sorted(staged_headers):
            if header not in reached:
                errors.append(f"{header} is staged but nothing outside {staged_headers[header]} includes it")
    for e in errors:
        print(f"layer-table: {e}")
    return 1 if errors else 0


if __name__ == "__main__":
    sys.exit(main())
