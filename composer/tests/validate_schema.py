"""Validates JSON documents against a JSON Schema (draft 2020-12).

Usage: validate_schema.py <schema.json> <document.json>...
Exits 0 when every document is valid, 1 when one is not.
"""

import json
import sys

import jsonschema


def main(argv):
    if len(argv) < 3:
        print("Usage: validate_schema.py <schema.json> <document.json>...", file=sys.stderr)
        return 2

    with open(argv[1], encoding="utf-8") as schema_file:
        schema = json.load(schema_file)
    jsonschema.Draft202012Validator.check_schema(schema)
    validator = jsonschema.Draft202012Validator(schema)

    failed = False
    for path in argv[2:]:
        with open(path, encoding="utf-8") as document_file:
            document = json.load(document_file)
        for error in sorted(validator.iter_errors(document), key=lambda e: list(e.path)):
            location = "/".join(str(part) for part in error.path) or "<root>"
            print(f"{path}: {location}: {error.message}", file=sys.stderr)
            failed = True
    return 1 if failed else 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
