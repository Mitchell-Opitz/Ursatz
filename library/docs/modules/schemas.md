# K5 schemas (P70 B2, R-759)

A `schema` entry holds a pattern of stages that a passage can follow. B2 adds the `galant` subtype, the 26 galant schemas of Open Music Theory part II; the `progression` subtype follows in B3 (R-760). The folder is `knowledge/schema/` (the JSON Schemas of every kind live in `knowledge/json-schema/`).

## The galant subtype

`schema_entry.h` holds the `SchemaEntry` the loader (`knowledge_schema.c`) fills. The content is closed (`knowledge/json-schema/schema.galant.schema.json`):

| Member | Meaning |
|---|---|
| `function` | where the schema appears: `opening-gambit`, `continuation`, `pre-cadential`, `cadence` or `post-cadential` |
| `bass_defining` | the bass is what defines the schema, so a match must have it |
| `melody_in_tenths` | the melody accompanies the bass in parallel tenths |
| `stages` | 1 to 8 stages, each: `strength` (`strong` or `weak`), `melody` and `bass` (`{"degree": 1 to 7, "alteration": "natural"\|"lowered"\|"either"\|"raised"}`), `figures` (the figured bass, may be empty), `numeral` (the Roman numeral in the tonic key; `Cad.` for the cadential six-four of the Cadenza composta), optional `numeral_dominant` and optional `melody_next` (the second melody note of a two-note stage, as in the Monte) |

The loader keeps its own `SchemaDegree` (`alteration` 0 natural, -1 lowered, 1 raised, 2 either), because `theory-knowledge` does not use `theory-galant`. A stage count outside 1 to 8, a degree outside 1 to 7, or an unknown function, strength or alteration refuses the build with `<file>: <id>: <field>: <problem>`; a missing or unknown subtype is refused naming `subtype`.

```c
const SchemaEntry *prinner = knowledge_snapshot_schema(snapshot, "omt.schema.prinner");
/* prinner->stage_count == 4, prinner->galant[0].melody == {6, either}, prinner->function == "continuation" */
```

`knowledge_snapshot_schema`, `_schema_count` and `_schema_at` (id order) are defined in `knowledge_schema.c`; the lookup returns NULL for an id that is absent or of another kind.

## Pinned to the C table

`knowledge/schema/omt.json` holds 26 entries, `omt.schema.<name>` for each `galant_schema_name`, status `reference`, in the table's order. The C table (`theory/galant/galant_prototypes.c`) stays the identity every reader uses: the matcher and the Composer do not read these entries yet. The test `schema_entries` (and its valgrind twin) pins the two both ways: every prototype has its entry with equal function, flags and stage fields, and every galant entry's name is a prototype's. `tools/ci/check-knowledge.py` checks the function and that every numeral is a Roman numeral.
