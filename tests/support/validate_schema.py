"""Validates JSON documents against a JSON Schema (draft 2020-12).

Usage: validate_schema.py [--ref <schema.json>]... <schema.json> <document.json>...
Each --ref schema is loaded under its $id, so the schema (and the references inside those schemas) can $ref it by that URL.
Exits 0 when every document is valid, 1 when one is not, 2 on a usage error.
"""

import json
import sys

import jsonschema


def load(path):
    with open(path, encoding="utf-8") as schema_file:
        return json.load(schema_file)


def main(argv):
    args = argv[1:]
    refs = []
    while len(args) >= 2 and args[0] == "--ref":
        refs.append(args[1])
        args = args[2:]
    if len(args) < 2:
        print("Usage: validate_schema.py [--ref <schema.json>]... <schema.json> <document.json>...", file=sys.stderr)
        return 2

    schema = load(args[0])
    jsonschema.Draft202012Validator.check_schema(schema)
    options = {}
    if refs:
        from referencing import Registry, Resource
        from referencing.jsonschema import DRAFT202012

        registry = Registry()
        for path in refs:
            contents = load(path)
            jsonschema.Draft202012Validator.check_schema(contents)
            registry = registry.with_resource(contents["$id"], Resource.from_contents(contents, default_specification=DRAFT202012))
        options["registry"] = registry
    validator = jsonschema.Draft202012Validator(schema, **options)

    failed = False
    for path in args[1:]:
        document = load(path)
        for error in sorted(validator.iter_errors(document), key=lambda e: list(e.path)):
            location = "/".join(str(part) for part in error.path) or "<root>"
            print(f"{path}: {location}: {error.message}", file=sys.stderr)
            failed = True
    return 1 if failed else 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
