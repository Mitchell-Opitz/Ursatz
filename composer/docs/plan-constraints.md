# Plan format: constraints

The constraints of a plan node and the motive relation steps, continuing the [plan format](../../library/docs/modules/plan-format.md). Back to the [docs index](README.md).

## Constraints

A constraint is an object with a `kind` and the fields for that kind. Constraints are data only;
nothing checks them. What a section is (its letter, role, pairing, cadences, return, layers, pedal, construction, figure, function, hold and connective) is not a constraint: it is the section's description ([plan-format.md](../../library/docs/modules/plan-format.md#section-descriptions)). Formats 1 to 4 wrote those as the constraints `archetype`, `material`, `theme_pairing`, `return`, `layers`, `section_letter`, `piece_part`, `free_phrase`, `hold`, `seam`, `pedal`, `construction`, `connective`, `liquidation`, `figure` and `section_function`, and the reader still takes them.

| `kind` | Fields |
| --- | --- |
| `length_bars` | `bars`: integer, 1 or more. |
| `cadence` | `type`: `pac`, `iac`, `hc`, `dc` or `pc`. On a phrase, subphrase or idea; a section holds its cadence in its description and refuses this constraint. |
| `motive_ref` | `motive`: the id of a motive node; `step` (optional): the relation that makes the node's motive from it, as the text of a library relation step ([Motive relation steps](#motive-relation-steps)). `segment` (optional): the label of the segment of the basic motif the node is made from (1 to 8 characters, `"b"`), and `steps` (optional, 1 to 8): the liquidation steps (`liquidate-features`, ch. VIII) the node's motive is the result of; both are a record of what grow-theme made, absent for none. Without `step` the node's melody is that motive itself. Files saved before `step` write `transform` and its members instead, which are still read. |
| `key` | `tonic` (spelled), `material` (the id of a K1 material, `omt.material.major` or `omt.material.natural-minor`). Format 2 wrote `mode` (`major` or `minor`), which is still read. |
| `embellish` | No fields. Marks a harmony lane span reopened for the embellishment pass. |
| `schema` | `schema`: the name of a galant schema as Ursatz's galant-schemas framework names it (`romanesca`, `do-re-mi`, `fonte` and so on); `stage`: the index of the schema's stage the span sounds, from 0. The span's other voices are read from the framework's prototype. |
| `harmony_shape` | The shape a harmony span is to take. `shape`: `prolongation`, `phrase_model` or `cadential`; `starts_on`: `any`, `tonic`, `dominant` or `pivot` (optional, `any`), `pivot` for a phrase-model span that opens a new key: the key lane changes where it starts and its first chord is a pivot chord between the two keys, written as its numeral in the new key; `ends_on`: `any`, `tonic` or `dominant` (optional, `any`); `max_cells_per_chord`: whole number, 0 for no limit (optional, 0); `split_at_bars`: true when no chord may cross a barline (optional, false); `pedal`: true when every chord's bass is the start anchor's root, only for a prolongation with `starts_on` and `ends_on` both `dominant` (optional, false; written only when true). `free_motive`: true when the span's harmony holds none of the chords the idea's motive wrote (a formal function with free motive harmony, R-711; optional, false, written only when true). See [grow.md](grow.md). |
| `accompaniment` | The accompaniment an inner voice span (voice 1) is to be written as. `pattern`: `block`, `arpeggiated`, `alberti`, `oom-pah` or `any`; `texture`: a standard-texture name (`homophony`, `homorhythm`, `melody and accompaniment` and the others the library names), absent for none; `low` and `high`: MIDI pitches, absent for 48 and 67. `high` must be above `low`. See [accompaniment.md](accompaniment.md). |
| `voicing` | The four-part voicing an inner voice span (voice 1, the alto) is to be written as. `write_soprano`: true when the voicing writes the melody span over the same time too (optional, false). See [voicing.md](voicing.md). |
| `counterpoint` | The counter-melody an inner voice span (voice 1) is to be written as. `species`: 1 to 4 (required). `above`: true when the given line is the bass and the counter-melody sounds above it, false when the given line is the melody (optional, false). See [counterpoint.md](counterpoint.md). |
| `style` | On the form node: the style model file `fill` prefers. `path`: the file as `plan --style` was given it. `weight`: a fraction as text (`"1/1"`; `"0/0"` is the default, 1/1). `without` (optional): a list of at most 8 style category names the style is read without (`--style-without`); absent reads as none, so older plans load. `blend` (optional): a second style model file blended with the first, `path` and `weights`, an object with a fraction from `"0/1"` to `"1/1"` as text for each of `harmony`, `rhythm`, `melody` and `form`: the second file's share in that dimension. See [style.md](style.md). |
| `source` | Where seeded material came from in a corpus ([style.md](style.md)), a note for the reader: on the motive node `m0` for a motive taken with `--like`, on the section for a progression taken with `--like-progression`. `piece`: the piece's path in the corpus; `start_measure`, `start_beat`, `end_measure`, `end_beat`: the passage as the Analyzer reports it (the beat plus the fraction of it); `score`: the similarity as a fraction from `"0/1"` to `"1/1"` as text. A Given node takes no more constraints, so it is added when the node is made. Nothing reads it at `fill`. |
| `pickup` | On a melody span (or its owner): the first `length` of the span is an anacrusis before the first downbeat note, and the phrase the melody operator reads starts after it. `length`: a fraction of a whole note as text (`"1/4"`), on the meter's grid (a multiple of 1/32, or a triplet of the beat or the division) and shorter than a bar. See [melody-fill.md](melody-fill.md). |
| `invent_motive` | No fields. On a Constrained melody span of a motive node: `motive-invent` writes the motive. See [piece.md](piece.md). |
| `motive_tones` | On a motive node (`m0`, the slot and idea motives, a piece's motive nodes): the tones of its motive, which the melody operator places it by ([melody-fill.md](melody-fill.md#tone-roles)). `harmony`: the base harmony the notes were labelled against, a list of 1 to 8 `{"numeral", "onset", "length"}` (the numeral as a motive's own harmony writes it: `"I"`, `"V65"`, `"bVII7"`; the onset from the start of the motive and the length, both as a fraction of a whole note as text). `tones`: one label for each note of the node's melody span, in note order: `chord`, `unclassified` or a non-chord tone (`passing`, `neighbor`, `appoggiatura`, `suspension`, `retardation`, `escape`, `anticipation`, `double-neighbor`, `changing-tone`, `pedal`). A label count that is not the note count is refused when the plan is read (`motive_tones on <node>: <n> labels for <m> notes`). A motive with no tones (a rhythm, a contour, one with no key) and an accompaniment motive have none; a plan without the constraint reads as before and the schema version is the same. A Given node takes no more constraints, so it is added when the node is made. |
| `contrast` | On an idea node (R-710): what the idea must contrast against another idea in, and by how much; written by grow with `development.functions.contrast`. `against`: the name of the other idea (`"basic idea"`, 1 to 63 characters); `aspects`: 1 to 4 distinct `{"aspect", "amount"}` (`rhythm`, `contour`, `register` or `density`; the least distance from that idea as a fraction as text, `"1/2"`, never negative). The plan holds the target; the theme response's `contrasts` reports the distances the notes measured. See [grow-contrast.md](grow-contrast.md#contrast-targets). |

### Motive relation steps

A `motive_ref` on a motive node says how the node's motive is made from another motive node: the
motivic development operators (`<ursatz_composer/motive_develop.h>`) read it and write the node's
melody. `step` is the text of a `MotiveRelationStep` of the Ursatz library (`analysis/motive_relation.h`,
`motive_relation_step_text` writes it and `motive_relation_step_parse` reads it): the relation name, then
`name=value` for each parameter that is not at its default, a ratio as `n:d`. The relations the operators
answer to, with their parameters:

| Step | Parameters | Left out |
| --- | --- | --- |
| `exact` | none | The exact repetition, or a transposition of it (the operator draws one). |
| `transpose` | `tonal` (0 or 1), `by` (semitones, or steps of the key's scale when tonal) | `by` 0: the operator draws it. |
| `invert` | `axis` (a MIDI pitch), `tonal` | `axis` absent: the source's first note, then axes inside its range. |
| `retrograde` | none | |
| `interval-change` | `interval` (1-based, 0 every one), `amount` (semitones), `keep-contour` | `amount` 0: the operator draws interval and amount. |
| `fragment` | `position` (1 leading, 2 trailing, 3 middle), `length`, `min` | `position` or `length` 0: drawn. |
| `liquidate` | `drop`, `keep-rhythm`, `min` | `drop` 0: every number of notes that leaves the minimum. |
| `augment`, `diminish` | `ratio` (`n:d`) | `ratio` absent: the usual ones (2:1, 3:2, 4:1; 1:2, 2:3, 1:4). |

`min` is the fewest notes a fragment or liquidation may have; 0 (absent) is the context's, else 2. An open
parameter is drawn from the run's seed. The operators write only the Realized melody lane span of the
node, and only when the result fits the node's span. A ref with no `step` is a plain reference: the node's
melody is the source motive itself.

Other relations of the library are valid in a `step` and are read and written, but no operator develops a
node from them yet. A `step` that is not a relation step fails the read, with the text in the message
(`not a relation step "transpose sideways=1"`).

Plan files saved before relation steps write `transform` (`none`, `transposed`, `inverted`, `retrograde`,
`augmented`, `diminished`, `fragmented`, `restated`, `interval_changed` or `liquidated`), `semitones` and the
members the transform uses (`tonal`, `steps`, `axis`, `interval`, `amount`, `keep_contour`, `position`, `length`,
`drop`, `keep_rhythm`, `ratio_num`, `ratio_den`). They are read, by one function in `library/plan/plan_json.c`, into the
step of the same relation (`none` into no step, `restated` into `exact`; `semitones` or `steps` into `by`; an absent
`axis` into -1) and are written back as `step`; nothing writes the old names. A `transform` the table does not
have fails the read with its name in the message. The format version stays 2.

## Adding a kind

A constraint kind is one row of `library/plan/plan_constraint_kinds.c` (R-609): its value in `PlanConstraintKind`, its name in the plan file, and three functions: `valid` (whether a constraint of the kind is valid in a plan, which `plan_node_add_constraint` and `plan_lane_span_add_constraint` ask), `write` (the members of its JSON object after `kind`) and `read` (the same members read, each refusal at its path). `plan_json.c` and `constraint.c` look the row up and call it, so no switch names a kind. The kind also goes in the constraint enum of `library/plan/schema/plan.v6.schema.json` (with an if/then for its members), with a row of the table above, and a case in the `same_constraint` of `test_plan_json`.

## Construction and pedal

A theme's construction (R-468) is the `construction` member of its section's description: `{"ideas": {"basic idea": 3, "contrasting idea": 2, ...}, "extensions": [], "reason": "given"}`, with the ideas, extensions and reason it always had.

The `pedal` member of a section's description (`"pedal": "dominant"`) and the `pedal` member of a `harmony_shape` (written `"pedal": true` after `split_at_bars`, only when set) ask for a dominant pedal under a presentation (R-446). The degree is required and only 5 is valid. A plan without either reads as before and writing it again gives the same bytes; the schema version is the same.
