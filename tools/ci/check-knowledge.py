#!/usr/bin/env python3
"""Knowledge check: tools/ci/check-knowledge.py <knowledge dir> [<schema dir>]

Validates every entry under the kind folders of the knowledge dir (target architecture 5): the files parse, the envelope and the
entry anatomy follow entry.schema.json, the content and examples follow the kind's schema, ids are unique and carry their file's
source and folder's kind, relations point at existing ids, citations name a source of sources.json, and a material's examples
spell its steps. One line per problem, "knowledge: <file>: <id>: <what>". Exit 0 clean, 1 with problems, 2 on a missing file.
"""
import json
import os
import re
import sys

KINDS = {
    "material", "chord-type", "voicing", "vocabulary", "schema", "relation", "function", "theme-type", "form", "operation",
    "pattern", "instrument", "rule", "preset", "term",
}
LETTER = {"C": 0, "D": 2, "E": 4, "F": 5, "G": 7, "A": 9, "B": 11}

problems = []


def problem(path, entry_id, text):
    problems.append("knowledge: %s: %s: %s" % (path, entry_id, text))


def load(path):
    try:
        with open(path, encoding="utf-8") as handle:
            return json.load(handle)
    except FileNotFoundError:
        print("knowledge: %s: missing file" % path)
        sys.exit(2)


def pitch_class(name):
    value = LETTER[name[0]]
    if len(name) > 1:
        value += 1 if name[1] == "#" else -1
    return value % 12


def check_material_examples(path, entry):
    content = entry.get("content", {})
    steps = content.get("steps")
    if not isinstance(steps, list) or not steps:
        return
    if steps != sorted(steps):
        problem(path, entry["id"], "steps are not ascending")
    descending = content.get("descending_steps", steps)
    for number, example in enumerate(entry.get("examples", []), 1):
        if not isinstance(example, dict) or "center" not in example or "pitches" not in example:
            continue
        try:
            center = pitch_class(example["center"])
            ascending = [pitch_class(name) for name in example["pitches"]]
            down = [pitch_class(name) for name in example.get("descending", [])]
        except (KeyError, IndexError, TypeError):
            continue
        if ascending != [(center + step) % 12 for step in steps]:
            problem(path, entry["id"], "example %d: pitches do not match steps" % number)
        if "descending" in example:
            expected = [center] + [(center + step) % 12 for step in reversed(descending[1:])]
            if down != expected:
                problem(path, entry["id"], "example %d: descending does not match the descending steps" % number)


def main(argv):
    if len(argv) not in (2, 3):
        print("usage: check-knowledge.py <knowledge dir> [<schema dir>]")
        return 2
    root = argv[1]
    schema_dir = argv[2] if len(argv) == 3 else os.path.join(root, "schema")
    try:
        import jsonschema
    except ImportError:
        print("knowledge: jsonschema is not installed")
        return 2
    if not os.path.isdir(root):
        print("knowledge: %s: missing file" % root)
        return 2
    sources = load(os.path.join(root, "sources.json"))
    entry_schema = load(os.path.join(schema_dir, "entry.schema.json"))
    source_ids = {source["id"] for source in sources.get("sources", [])}
    schema_cache = {}

    def kind_schema(kind, subtype):
        name = "%s.%s.schema.json" % (kind, subtype) if subtype else "%s.schema.json" % kind
        if name not in schema_cache:
            path = os.path.join(schema_dir, name)
            schema_cache[name] = load(path) if os.path.isfile(path) else None
        return name, schema_cache[name]

    ids = {}
    relations = []
    for folder in sorted(os.listdir(root)):
        folder_path = os.path.join(root, folder)
        if not os.path.isdir(folder_path) or folder == "schema":
            continue
        files = []
        for directory, _, names in os.walk(folder_path):
            files += [os.path.join(directory, name) for name in names if name.endswith(".json")]
        if folder not in KINDS:
            for file in sorted(files) or [folder_path]:
                problem(os.path.relpath(file, root), "-", "unknown kind folder '%s'" % folder)
            continue
        for file in sorted(files):
            rel = os.path.relpath(file, root).replace(os.sep, "/")
            try:
                with open(file, encoding="utf-8") as handle:
                    document = json.load(handle)
            except ValueError as error:
                problem(rel, "-", "not valid JSON: %s" % error)
                continue
            if not isinstance(document, dict):
                problem(rel, "-", "the envelope is not an object")
                continue
            if document.get("schema") != "ursatz-knowledge":
                problem(rel, "-", "envelope schema is not 'ursatz-knowledge'")
            if document.get("schema_version") != 1:
                problem(rel, "-", "envelope schema_version is not 1")
            source = document.get("source")
            if source not in source_ids:
                problem(rel, "-", "envelope source '%s' is not in sources.json" % source)
            entries = document.get("entries")
            if not isinstance(entries, list):
                problem(rel, "-", "envelope entries is not an array")
                continue
            for entry in entries:
                entry_id = entry.get("id", "-") if isinstance(entry, dict) else "-"
                for error in sorted(jsonschema.Draft202012Validator(entry_schema).iter_errors(entry), key=lambda e: list(e.path)):
                    where = "/".join(str(part) for part in error.path)
                    problem(rel, entry_id, "entry%s: %s" % (" " + where if where else "", error.message))
                if not isinstance(entry, dict) or not isinstance(entry_id, str):
                    continue
                kind, subtype = entry.get("kind"), entry.get("subtype")
                if kind != folder:
                    problem(rel, entry_id, "kind '%s' is not the folder '%s'" % (kind, folder))
                parts = entry_id.split(".")
                if len(parts) == 3:
                    if parts[0] != source:
                        problem(rel, entry_id, "id source part '%s' is not the file's source '%s'" % (parts[0], source))
                    if parts[1] != folder:
                        problem(rel, entry_id, "id kind part '%s' is not the folder '%s'" % (parts[1], folder))
                if entry_id in ids:
                    problem(rel, entry_id, "duplicate id (also in %s)" % ids[entry_id])
                else:
                    ids[entry_id] = rel
                if kind in KINDS:
                    name, schema = kind_schema(kind, subtype)
                    if schema is None:
                        problem(rel, entry_id, "no schema %s for this kind and subtype" % name)
                    else:
                        for error in sorted(jsonschema.Draft202012Validator(schema).iter_errors(entry), key=lambda e: list(e.path)):
                            where = "/".join(str(part) for part in error.path)
                            problem(rel, entry_id, "%s %s: %s" % (name, where, error.message))
                for citation in entry.get("citations", []) if isinstance(entry.get("citations"), list) else []:
                    if isinstance(citation, dict) and citation.get("source") not in source_ids:
                        problem(rel, entry_id, "citation source '%s' is not in sources.json" % citation.get("source"))
                for relation in entry.get("relations", []) if isinstance(entry.get("relations"), list) else []:
                    if isinstance(relation, dict):
                        relations.append((rel, entry_id, relation.get("target")))
                if kind == "material" and subtype == "unordered":
                    check_material_examples(rel, entry)
    for rel, entry_id, target in relations:
        if target not in ids:
            problem(rel, entry_id, "relation target '%s' is not an entry" % target)
    for line in problems:
        print(line)
    return 1 if problems else 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
