"""Validates every workshop-owned file of data folders against its schema (P63 B7, R-642).

Usage: validate_formats.py <schema folder> <data folder>...
Each file the layout names (data-folder.json, project.json, item.json, envelope.json, body.json, op-checks.json, subject files, piece.json,
collection files, run.json, style.json, render.json) must carry "format" and "format_version", name the format its place in the layout
says, and match workshop/schema/<format>.v<N>.schema.json, N the version this build writes (2 for the data folder and the theme and piece bodies, else 1). A folder must hold at least one file of each format the first data folder is
expected to have (the migrated fixture holds every kind but the render cache). Exits 0 when all is valid, 1 otherwise.
"""

import json
import os
import re
import sys

import jsonschema

PLACES = [
    (re.compile(r"^data-folder\.json$"), "ursatz-data-folder"),
    (re.compile(r"^projects/[^/]+/project\.json$"), "ursatz-project"),
    (re.compile(r"^projects/[^/]+/items/[^/]+/item\.json$"), "ursatz-item"),
    (re.compile(r"^projects/[^/]+/items/[^/]+/v\d+/envelope\.json$"), "ursatz-item-version"),
    (re.compile(r"^projects/[^/]+/items/[^/]+/v\d+/body\.json$"), None),
    (re.compile(r"^projects/[^/]+/items/[^/]+/op-checks\.json$"), "ursatz-op-checks"),
    (re.compile(r"^subjects/[^/]+\.json$"), "ursatz-subject"),
    (re.compile(r"^library/pieces/[^/]+/piece\.json$"), "ursatz-library-piece"),
    (re.compile(r"^library/collections/[^/]+\.json$"), "ursatz-collection"),
    (re.compile(r"^library/links/[^/]+/run\.json$"), "ursatz-link-run"),
    (re.compile(r"^settings/styles/[^/]+/style\.json$"), "ursatz-style"),
    (re.compile(r"^settings/styles/[^/]+/settings\.json$"), "ursatz-settings"),
    (re.compile(r"^settings/taste/[^/]+\.json$"), "ursatz-settings"),
    (re.compile(r"^ratings/sessions/[^/]+\.json$"), "ursatz-listening-session"),
    (re.compile(r"^ratings/items/[^/]+\.json$"), "ursatz-item-ratings"),
    (re.compile(r"^ratings/critiques/[^/]+\.json$"), "ursatz-item-critiques"),
    (re.compile(r"^projects/[^/]+/settings\.json$"), "ursatz-settings"),
    (re.compile(r"^projects/[^/]+/items/[^/]+/settings\.json$"), "ursatz-settings"),
    (re.compile(r"^projects/[^/]+/items/[^/]+/v\d+/settings\.json$"), "ursatz-settings-snapshot"),
    (re.compile(r"^cache/renders/[^/]+/v\d+/render\.json$"), "ursatz-render"),
    (re.compile(r"^ursatz\.json$"), "ursatz-config"),
]
CURRENT = {"ursatz-data-folder": 2, "ursatz-theme-body": 2, "ursatz-piece-body": 2}  # every other format is at version 1
BODIES = {"motif": "ursatz-motif-body", "theme": "ursatz-theme-body", "piece": "ursatz-piece-body"}
REQUIRED = {"ursatz-data-folder", "ursatz-project", "ursatz-item", "ursatz-item-version", "ursatz-motif-body", "ursatz-theme-body",
            "ursatz-piece-body", "ursatz-op-checks", "ursatz-subject", "ursatz-library-piece", "ursatz-collection", "ursatz-style",
            "ursatz-link-run"}


def main(argv):
    if len(argv) < 3:
        print(__doc__, file=sys.stderr)
        return 2
    schemas = {}
    for name in os.listdir(argv[1]):
        match = re.match(r"^(ursatz-[a-z-]+)\.v(\d+)\.schema\.json$", name)
        if match and int(match.group(2)) == CURRENT.get(match.group(1), 1):
            with open(os.path.join(argv[1], name), encoding="utf-8") as handle:
                schema = json.load(handle)
            jsonschema.Draft202012Validator.check_schema(schema)
            schemas[match.group(1)] = jsonschema.Draft202012Validator(schema)
    failures = 0
    seen = {}
    for folder in argv[2:]:
        for directory, directories, files in os.walk(folder):
            directories[:] = [d for d in directories if d not in (".pre-1", ".migrate-1")]
            for name in files:
                path = os.path.join(directory, name)
                relative = os.path.relpath(path, folder).replace(os.sep, "/")
                expected = None
                placed = False
                for pattern, format_name in PLACES:
                    if pattern.match(relative):
                        placed, expected = True, format_name
                        break
                if not placed:
                    continue
                with open(path, encoding="utf-8") as handle:
                    document = json.load(handle)
                named = document.get("format")
                if expected is None:  # a body: the format is the one of its item's type (the id's prefix)
                    kind = {"mot": "motif", "thm": "theme", "pce": "piece"}[re.search(r"/items/([a-z]+)-", relative).group(1)]
                    expected = BODIES[kind]
                if named != expected:
                    print("%s: format is %r, the layout says %r" % (relative, named, expected))
                    failures += 1
                    continue
                if document.get("format_version") != CURRENT.get(expected, 1):
                    print("%s: format_version is %r" % (relative, document.get("format_version")))
                    failures += 1
                    continue
                errors = sorted(schemas[expected].iter_errors(document), key=lambda e: list(e.path))
                for error in errors:
                    print("%s: %s (at /%s)" % (relative, error.message[:200], "/".join(str(p) for p in error.path)))
                failures += len(errors)
                seen[expected] = seen.get(expected, 0) + 1
    for format_name in sorted(REQUIRED - set(seen)):
        print("no file of format %s was found" % format_name)
        failures += 1
    print("%d files in %d formats: %s" % (sum(seen.values()), len(seen), "valid" if failures == 0 else "%d problems" % failures))
    return 0 if failures == 0 else 1


if __name__ == "__main__":
    sys.exit(main(sys.argv))
