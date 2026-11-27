# K5 schemas (P70 B2, R-759)

A `schema` entry holds a pattern of stages that a passage can follow. The `galant` subtype holds the 26 galant schemas of Open Music Theory part II (B2); the `progression` subtype holds the pop and modal chord schemas of part VII (B3, R-760). The folder is `knowledge/schema/` (the JSON Schemas of every kind live in `knowledge/json-schema/`).

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

## The progression subtype

The content is closed (`knowledge/json-schema/schema.progression.schema.json`) and read into `SchemaEntry` beside the galant members:

| Member | Meaning |
|---|---|
| `stages` | 1 to 16 stages `{"chord": text, "units": 1 to 8}`; `units` is optional and reads 1. The chord is the numeral against the center's major scale, case for quality (`I`, `bVII`, `vi`); the loader keeps the text and does not parse it (`theory-knowledge` does not include the numeral grammar), so the reader of the entry does |
| `loop` | the stages repeat |
| `rotations` | optional, unique stage indexes a loop may start on; each must be a stage of the schema |
| `role` | `loop`, `opening`, `continuation` or `cadence` |
| `materials` | optional, up to 8 ids of pitch materials the progression is heard in |
| `realizes` | the id of the progression-type vocabulary entry it realizes (`prolongational`, `cadential`, `phrase-model`, `sequential`) |

`realizes` and `materials` are checked when the snapshot is built (`knowledge_schema_link`): a `realizes` that is not a progression-type entry, or a material that is not a pitch material, refuses the build with `<file>: <id>: <field>: <problem>`, as does a rotation past the stage count, a `units` outside 1 to 8 or an unknown role. `tools/ci/check-knowledge.py` checks the chords against `^[b#]?(VII|...|i)(o|\+)?$`, the rotations and `realizes`.

`knowledge/schema/omt.json` holds fifteen of them after the galant ones, status `reference` (a reader sets them `active` when it uses them): the twelve-bar blues (six stages, 12 units, a loop), the double and extended plagal and the aeolian and lydian cadences (cadences), the doo-wop, singer-songwriter, hopscotch, lament and circle of fifths (loops), the puff (an opening) and the subtonic, aeolian, dorian and lydian shuttles (loops). The aeolian cadence has the alias "Mario cadence", the lament "lament bass" and the twelve-bar blues "12-bar blues". The entries do not hold the common variations of OMT's Example 1; a later phase may add them.

## Pinned to the C table

`knowledge/schema/omt.json` holds 26 galant entries, `omt.schema.<name>` for each `galant_schema_name`, status `reference`, in the table's order. The C table (`theory/galant/galant_prototypes.c`) stays the identity every reader uses: the matcher and the Composer do not read these entries yet. The test `schema_entries` (and its valgrind twin) pins the two both ways: every prototype has its entry with equal function, flags and stage fields, and every galant entry's name is a prototype's. `tools/ci/check-knowledge.py` checks the function and that every numeral is a Roman numeral.
