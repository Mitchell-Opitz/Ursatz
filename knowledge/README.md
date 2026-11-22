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
| `voicing`, `schema`, `form`, `operation`, `pattern`, `preset`, `term` | K3, K5 and K9 to K15 | none yet |
| `vocabulary` | K4 vocabulary | `cadence-kind`, `embellishment`, `texture`, `progression-type`, `function` (a harmonic function); `omt.json` holds 32 entries, each with the `code` the C table uses for the same thing |
| `function` | K7 formal function | `formal-function` (core, level, the progression types its harmony may take, harmonic rhythm, rhythmic activity, motive harmony, cadence, contrast target); `omt.json` holds ten (basic idea, contrasting idea, fragment, cadential idea, presentation, continuation, cadential, antecedent, consequent, compound basic idea) |
| `theme-type` | K8 theme type | yes, no subtype (a beginning, an ending and the ideas between them with their functions, slots and units); `omt.json` holds the sentence, the period and seven hybrids |
| `instrument` | K12 instrument and ensemble | `instrument` (family, transposition, clefs, range, General MIDI program, sustain pedal), `ensemble` (named slots of instruments); `omt.json` holds the piano and a solo-piano ensemble |
| `rule` | K13 rule | `constraint` (a declared limit with its stage, waive and setting), `evaluator` (a weighted judgment with its default weight and parameters), `observation` (a plain-word note, never scored); every one names its scope, message and the function that checks it; `michelle.json` holds 12 (eight evaluators, four observations), `fux.json` seven constraints (the melody rules) |
| `relation` | K6 relation | `tonal`, `modal`, `neo-riemannian`, `set`, `serial`; `omt.json` holds the tonal, modal and neo-Riemannian relations (no set or serial entries yet) |

A kind with no schema file accepts no entry. A schema is added with the first entries of its kind.

## An entry

`entry.schema.json` is the anatomy every kind shares:

- `id`: `<source>.<kind>.<name>`, lowercase words joined by hyphens (`omt.material.natural-minor`). A user's own entries start with `user`. The id never changes.
- `kind`, `subtype` (required for `material`, `vocabulary`, `relation`, `function`, `instrument` and `rule`), `name`, optional `aliases`.
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

It prints one line per problem, `knowledge: <file>: <id>: <what>`, and exits 0 when clean. It also checks that an instrument's range runs low to high, holds its comfortable range and agrees with its examples, that an ensemble's slots name instruments of the folder with unique names, that a relation's examples resolve (the target is what the relation gives from the anchor, over the materials of the folder), that a material's key word and brightness rank are its own, that a `rotation-of` target is the same collection rotated, that a vocabulary code is unique within its subtype, that a texture's category is the code of a texture entry, and that a progression type's example chords follow its type (a prolongation starts and ends on one numeral, a cadential progression has a predominant before its V), that a formal function's harmony ids are progression types and its contrast target a formal function, and that a theme type's beginning and ending are formal functions of subphrase or phrase level, its ideas' functions of idea level, its units sum to 8 and its examples' bars follow its units. It needs python3 with `jsonschema`. The tests `knowledge_validate` and `knowledge_validate_fixtures` run it in CI.

## In code

The build embeds every `*.json` of this folder except `schema/` in `ursatz_theory` (`cmake/embed_json.py`; the file is regenerated when an entry changes). Nothing reads this folder at run time.

- `theory/knowledge/knowledge_snapshot.h`: `knowledge_snapshot_build` parses the embedded files, then each user file the caller passes, into an immutable `KnowledgeSnapshot`: every entry by its common fields (unknown kinds kept), the pitch materials with their steps, key word, family, brightness, color notes, aliases and links, the chord types by their members, the relations by family, the instruments and ensemble templates (`knowledge_snapshot_instrument`, `knowledge_snapshot_ensemble`; an ensemble slot naming anything but an instrument entry is refused), the vocabulary entries by subtype (`knowledge_snapshot_vocabulary`; the C tables stay the identity and tests pin them to the entries), the formal functions and theme types (`knowledge_snapshot_theme_type_of` finds a theme type by the codes of its beginning and ending; every link between entries is resolved or the build is refused), a hash per entry and one for the whole snapshot. A material is found by id, name, key word or alias (`knowledge_snapshot_material_named`), a chord type by id or by the pitch classes above its root, a relation by id, name or alias. A broken entry, a repeated id or an id in the wrong namespace (`user.` is for a user's own files only) is refused with `<file>: <id>: <field>: <problem>`.
- Whoever calls the library builds the snapshot and hands it on. The library holds no global copy, so two snapshots in one process are independent, and a snapshot outlives every key, motive sketch and analysis made from it (they point into it). The applications build it with `knowledge_snapshot_build(NULL, 0, ...)`: the Analyzer CLI once at start, the GUI backend at server start (`service/knowledge_service.h`), the Composer CLI once per process. The Composer takes it from the call context of every entry that composes or reads (`ComposerContext.knowledge`, [the call context](../composer/docs/api.md#the-call-context); a NULL context or snapshot is refused with `a knowledge snapshot is required`), and its keys, key texts and plan files are made of its materials. The Analyzer takes it in `AnalyzerOptions.knowledge` (`analyzer_analyze` with none is `URSATZ_ERR_REQUEST`), the chain in `AnalysisChainConfig.knowledge` (none is `ANALYSIS_CHAIN_ERR_KNOWLEDGE`). Library tests use `tests/support/test_knowledge.h`; the Analyzer, Composer and GUI tests have the same helper in their support folders.
- Keys carry materials: a key made in the library holds the `PitchMaterial` it was made from (the entry of the snapshot), and the tonal functions read its steps, mask and signature. `theory/tonality/common_practice_materials.h` resolves the four common-practice materials (`omt.material.major`, `.natural-minor`, `.harmonic-minor`, `.melodic-minor`) once from a snapshot; a snapshot without one of these ids is refused.
- The four tonality materials (`omt.material.major`, `.natural-minor`, `.harmonic-minor`, `.melodic-minor`) are `active` because the key code reads their steps from the snapshot (`common_practice_materials_resolve`); there is no other table of their steps in the repository.

