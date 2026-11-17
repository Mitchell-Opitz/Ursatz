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
| `chord-type` | K2 chord type | yes; `omt.json` holds the triads and seventh chords |
| `voicing`, `vocabulary`, `schema`, `function`, `theme-type`, `form`, `operation`, `pattern`, `instrument`, `rule`, `preset`, `term` | K3 to K5 and K7 to K15 | none yet |
| `relation` | K6 relation | `tonal`, `modal`, `neo-riemannian`, `set`, `serial`; `omt.json` holds the tonal, modal and neo-Riemannian relations (no set or serial entries yet) |

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

It prints one line per problem, `knowledge: <file>: <id>: <what>`, and exits 0 when clean. It also checks that a relation's examples resolve (the target is what the relation gives from the anchor, over the materials of the folder), that a material's key word and brightness rank are its own, and that a `rotation-of` target is the same collection rotated. It needs python3 with `jsonschema`. The tests `knowledge_validate` and `knowledge_validate_fixtures` run it in CI.

## In code

The build embeds every `*.json` of this folder except `schema/` in `ursatz_theory` (`cmake/embed_knowledge.py`; the file is regenerated when an entry changes). Nothing reads this folder at run time.

- `theory/knowledge/knowledge_snapshot.h`: `knowledge_snapshot_build` parses the embedded files, then each user file the caller passes, into an immutable `KnowledgeSnapshot`: every entry by its common fields (unknown kinds kept), the pitch materials with their steps, key word, family, brightness, color notes, aliases and links, the chord types by their members, the relations by family, a hash per entry and one for the whole snapshot. A material is found by id, name, key word or alias (`knowledge_snapshot_material_named`), a chord type by id or by the pitch classes above its root, a relation by id, name or alias. A broken entry, a repeated id or an id in the wrong namespace (`user.` is for a user's own files only) is refused with `<file>: <id>: <field>: <problem>`.
- Whoever calls the library builds the snapshot and hands it on. The library holds no global copy, so two snapshots in one process are independent, and a snapshot outlives every key, motive sketch and analysis made from it (they point into it). The applications build it with `knowledge_snapshot_build(NULL, 0, ...)`: the Analyzer CLI once at start, the GUI backend at server start (`service/knowledge_service.h`), the Composer's hearing once per hearing until the Composer takes its own from its call context (P58 B13). The Analyzer takes it in `AnalyzerOptions.knowledge` (`analyzer_analyze` with none is `ANALYZER_ERR_ARGUMENT`), the chain in `AnalysisChainConfig.knowledge` (none is `ANALYSIS_CHAIN_ERR_KNOWLEDGE`). Library tests use `tests/support/test_knowledge.h`; the Analyzer, Composer and GUI tests have the same helper in their support folders.
- Keys carry materials: a key made in the library holds the `PitchMaterial` it was made from (the entry of the snapshot), and the tonal functions read its steps, mask and signature. `theory/tonality/common_practice_materials.h` resolves the four common-practice materials (`omt.material.major`, `.natural-minor`, `.harmonic-minor`, `.melodic-minor`) once from a snapshot; a snapshot without one of these ids is refused.
- The four tonality modes still reach `theory/tonality/tonal_key.c` as generated rows, `knowledge_tonality_mode_steps` in `theory/knowledge/knowledge_modes.h`, taken from the `omt.material.*` entries by the same build step, for the Composer's mode-based keys until P58 B13 moves it. They are `active` because that code uses them; there is no other table of their steps in the repository.

