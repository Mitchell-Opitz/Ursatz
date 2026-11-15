"""Validates an analysis document against the analysis document JSON Schema.

Usage: validate_schema.py <schema.json> <document.json>
Exits 0 when the document is valid, 1 when it is not.
"""

import json
import sys

import jsonschema


def main(argv):
    if len(argv) != 3:
        print("Usage: validate_schema.py <schema.json> <document.json>", file=sys.stderr)
        return 2

    with open(argv[1], encoding="utf-8") as schema_file:
        schema = json.load(schema_file)
    with open(argv[2], encoding="utf-8") as document_file:
        document = json.load(document_file)

    jsonschema.Draft202012Validator.check_schema(schema)
    validator = jsonschema.Draft202012Validator(schema)
    errors = sorted(validator.iter_errors(document), key=lambda error: list(error.path))
    for error in errors:
        location = "/".join(str(part) for part in error.path) or "<root>"
        print(f"{argv[2]}: {location}: {error.message}", file=sys.stderr)
    return 1 if errors else 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
