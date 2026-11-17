# Knowledge

The music-theory knowledge Ursatz works from, as data. Each entry is one fact about one thing (a scale, a chord type, a way to move from one key to another) with the book it comes from. Nothing here is code; the library reads it through the snapshot described in the target architecture, section 5.

## Layout

```
knowledge/
  README.md
  sources.json          the books an entry may cite
  schema/               one JSON Schema per kind and subtype, and entry.schema.json
  material/omt.json     one folder per kind, one file per source
```

A file holds the entries of one kind from one source. The folder is the kind, the file name is the source (`omt.json` for source `omt`), and the file starts with its envelope:

```json
{ "schema": "ursatz-knowledge", "schema_version": 1, "source": "omt", "entries": [ ... ] }
```

## Kinds

The folder name, the `kind` field and the middle part of an id are the same keyword.

| Keyword | Kind | Schema now |
|---|---|---|
| `material` | K1 pitch material | `unordered` (a collection above a center), `ordered` (a row) |
| `chord-type` | K2 chord type | yes |
| `voicing`, `vocabulary`, `schema`, `function`, `theme-type`, `form`, `operation`, `pattern`, `instrument`, `rule`, `preset`, `term` | K3 to K5 and K7 to K15 | none yet |
| `relation` | K6 relation | `tonal`, `modal`, `neo-riemannian`, `set`, `serial` |

A kind with no schema file accepts no entry. A schema is added with the first entries of its kind.

## An entry

`entry.schema.json` is the anatomy every kind shares:

- `id`: `<source>.<kind>.<name>`, lowercase words joined by hyphens (`omt.material.natural-minor`). A user's own entries start with `user`. The id never changes.
- `kind`, `subtype` (required for `material` and `relation`), `name`, optional `aliases`.
- `content`: what the entry says, checked by the kind's schema.
- `relations`: `{ "type": "variant-of", "target": "<id>" }`, every target an existing entry.
- `citations`: at least one for a repository entry. `source` is an id of `sources.json`, with `part`, `chapter`, `section` and `example` as the book numbers them. `note` says in your own words what the entry takes from it. Never copy book text.
- `status`: `reference` (recorded, nothing uses it), `active` (code or a generator uses it) or `retired`.
- `examples`: required, at least one, when the status is `active`. Their shape is the kind's schema. For a pitch material each example spells the steps from a center, so the check can compare them.
- `version`: an integer from 1. The content hash is computed by the snapshot, never stored.

## Add an entry

1. Add the source to `sources.json` if it is new.
2. Add the entry to `<kind>/<source>.json` (a new file gets the envelope above).
3. Run the check and fix what it reports:

```bash
python3 tools/ci/check-knowledge.py knowledge
```

It prints one line per problem, `knowledge: <file>: <id>: <what>`, and exits 0 when clean. It needs python3 with `jsonschema`. The tests `knowledge_validate` and `knowledge_validate_fixtures` run it in CI.
