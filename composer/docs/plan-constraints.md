# Plan format: constraints

The constraints of a plan node and the motive relation steps, continuing the [plan format](plan-format.md). Back to the [docs index](README.md).

## Constraints

A constraint is an object with a `kind` and the fields for that kind. Constraints are data only;
nothing checks them.

| `kind` | Fields |
| --- | --- |
| `length_bars` | `bars`: integer, 1 or more. |
| `cadence` | `type`: `pac`, `iac`, `hc`, `dc` or `pc`. |
| `motive_ref` | `motive`: the id of a motive node; `step` (optional): the relation that makes the node's motive from it, as the text of a library relation step ([Motive relation steps](#motive-relation-steps)). Without `step` the node's melody is that motive itself. Files saved before `step` write `transform` and its members instead, which are still read. |
| `key` | `tonic` (spelled), `mode` (`major` or `minor`). |
| `archetype` | `type`: `sentence`, `period` or `hybrid`. |
| `embellish` | No fields. Marks a harmony lane span reopened for the embellishment pass. |
| `schema` | `schema`: the name of a galant schema as Ursatz's galant-schemas framework names it (`romanesca`, `do-re-mi`, `fonte` and so on); `stage`: the index of the schema's stage the span sounds, from 0. The span's other voices are read from the framework's prototype. |
| `material` | No fields. On a section node: the section and everything under it is source material, not part of the piece. Rendering skips its lane spans (`RENDER_SKIP_MATERIAL`) and the piece ends where the last section that is not material ends. |
| `harmony_shape` | The shape a harmony span is to take. `shape`: `prolongation`, `phrase_model` or `cadential`; `starts_on`: `any`, `tonic`, `dominant` or `pivot` (optional, `any`), `pivot` for a phrase-model span that opens a new key: the key lane changes where it starts and its first chord is a pivot chord between the two keys, written as its numeral in the new key; `ends_on`: `any`, `tonic` or `dominant` (optional, `any`); `max_cells_per_chord`: whole number, 0 for no limit (optional, 0); `split_at_bars`: true when no chord may cross a barline (optional, false). See [grow.md](grow.md). |
| `theme_pairing` | How a theme begins and ends. `beginning`: `presentation`, `antecedent` or `compound_basic_idea`; `ending`: `continuation`, `consequent` or `cadential`. The nine pairings make the sentence, the period and the seven hybrids of standard-form. |
| `accompaniment` | The accompaniment an inner voice span (voice 1) is to be written as. `pattern`: `block`, `arpeggiated`, `alberti`, `oom-pah` or `any`; `texture`: a standard-texture name (`homophony`, `homorhythm`, `melody and accompaniment` and the others the library names), absent for none; `low` and `high`: MIDI pitches, absent for 48 and 67. `high` must be above `low`. See [accompaniment.md](accompaniment.md). |
| `voicing` | The four-part voicing an inner voice span (voice 1, the alto) is to be written as. `write_soprano`: true when the voicing writes the melody span over the same time too (optional, false). See [voicing.md](voicing.md). |
| `counterpoint` | The counter-melody an inner voice span (voice 1) is to be written as. `species`: 1 to 4 (required). `above`: true when the given line is the bass and the counter-melody sounds above it, false when the given line is the melody (optional, false). See [counterpoint.md](counterpoint.md). |
| `return` | On a section node: the section is a return of an earlier one. `section`: the id of the section it returns (a Section node, else `PLAN_ERR_BAD_PAYLOAD`); `variation`: `exact`, `ornament`, `reharmonize` or `reregister`. `form-return` gives the section the returned one's nodes and lane spans, varied as `variation` says, when the two are as long and the meter and the key lane have the same shape; any other is grown afresh from its motive. See [piece.md](piece.md). |
| `layers` | On a section node: the lanes that sound in a layer section. `lanes`: an array of lane names (`bass`, `harmony`, `melody`, `inner_voice`, `key`, `meter`), at least one. Rendering skips the section's spans in the other lanes (`RENDER_SKIP_LAYER`). |
| `section_letter` | The letter of a section in its form. `letter`: one to three characters (`A`, `B`, `A'`). |
| `style` | On the form node: the style model file `fill` prefers. `path`: the file as `plan --style` was given it. `weight`: a fraction as text (`"1/1"`; `"0/0"` is the default, 1/1). `without` (optional): a list of at most 8 style category names the style is read without (`--style-without`); absent reads as none, so older plans load. `blend` (optional): a second style model file blended with the first, `path` and `weights`, an object with a fraction from `"0/1"` to `"1/1"` as text for each of `harmony`, `rhythm`, `melody` and `form`: the second file's share in that dimension. See [style.md](style.md). |
| `source` | Where seeded material came from in a corpus ([style.md](style.md)), a note for the reader: on the motive node `m0` for a motive taken with `--like`, on the section for a progression taken with `--like-progression`. `piece`: the piece's path in the corpus; `start_measure`, `start_beat`, `end_measure`, `end_beat`: the passage as the Analyzer reports it (the beat plus the fraction of it); `score`: the similarity as a fraction from `"0/1"` to `"1/1"` as text. A Given node takes no more constraints, so it is added when the node is made. Nothing reads it at `fill`. |
| `pickup` | On a melody span (or its owner): the first `length` of the span is an anacrusis before the first downbeat note, and the phrase the melody operator reads starts after it. `length`: a fraction of a whole note as text (`"1/4"`), a multiple of 1/32 and shorter than a bar. See [melody-fill.md](melody-fill.md). |
| `invent_motive` | No fields. On a Constrained melody span of a motive node: `motive-invent` writes the motive. See [piece.md](piece.md). |
| `piece_part` | On a section node of a piece structure: `part`: `intro`, `body`, `turnaround` or `ending`; `id`: the section's id in the structure, 1 to 15 characters. See [piece-structure.md](piece-structure.md). |
| `free_phrase` | No fields. On a section node: a phrase of any length in beats, not a theme; `grow-theme` builds it as ideas of a bar. See [grow-harmony.md](grow-harmony.md#free-phrases). |
| `hold` | No fields. On a section node: the stages from structure to texture leave it alone, and the seam stage fills it against the seam. See [loop-seam.md](loop-seam.md). |
| `seam` | No fields. On a section node: the bar after the loop's end, a Given copy of the body's first bar; it is not part of the piece. See [loop-seam.md](loop-seam.md). |
| `motive_tones` | On a motive node (`m0`, the slot and idea motives, a piece's motive nodes): the tones of its motive, which the melody operator places it by ([melody-fill.md](melody-fill.md#tone-roles)). `harmony`: the base harmony the notes were labelled against, a list of 1 to 8 `{"numeral", "onset", "length"}` (the numeral as a motive's own harmony writes it: `"I"`, `"V65"`, `"bVII7"`; the onset from the start of the motive and the length, both as a fraction of a whole note as text). `tones`: one label for each note of the node's melody span, in note order: `chord`, `unclassified` or a non-chord tone (`passing`, `neighbor`, `appoggiatura`, `suspension`, `retardation`, `escape`, `anticipation`, `double-neighbor`, `changing-tone`, `pedal`). A label count that is not the note count is refused when the plan is read (`motive_tones on <node>: <n> labels for <m> notes`). A motive with no tones (a rhythm, a contour, one with no key) and an accompaniment motive have none; a plan without the constraint reads as before and the schema version is the same. A Given node takes no more constraints, so it is added when the node is made. |

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
`drop`, `keep_rhythm`, `ratio_num`, `ratio_den`). They are read, by one function in `src/plan/plan_json.c`, into the
step of the same relation (`none` into no step, `restated` into `exact`; `semitones` or `steps` into `by`; an absent
`axis` into -1) and are written back as `step`; nothing writes the old names. A `transform` the table does not
have fails the read with its name in the message. The format version stays 2.
