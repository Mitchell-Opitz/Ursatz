# Plan format

A piece plan is written as JSON by `plan_to_json` / `plan_write_json_file` and read by
`plan_from_json` / `plan_read_json_file` (`<ursatz_composer/plan_json.h>`). The file is the Composer's own: it has a
name, a version and a JSON Schema ([`schema/plan.v2.schema.json`](../schema/plan.v2.schema.json), JSON Schema 2020-12),
and it is written on library values: a key's tonic is a spelled note name, a chord's function is the library's function
name, a note may carry its spelling. Writing always gives format version 2 (`URSATZ_COMPOSER_PLAN_FORMAT_VERSION`).
Reading takes version 2 and version 1, the format of every plan saved before it ([Format versions](#format-versions)).

The output is pretty-printed with a 2-space indent, in the key order shown below, and ends with a
newline. The same plan always gives the same bytes.

## Conventions

These are the library's.

- **Time** is an exact rational position in whole notes from the start of the piece:
  `{"num": 3, "den": 2}`, in lowest terms. `den` is at least 1.
- **Span** is `{"start": <time>, "end": <time>}`. In memory a span is a start and a length; `end`
  is start plus length, and a span must be longer than zero.
- **Claim** is a lane span written as an entry of its lane's section: `framework`, `span`, `note_ids`, `label`,
  `uncertainty`, and the `composer` object that holds the plan's own data.
- **Tonic** is a note name without an octave as the library spells it (`pitch_spelling_name`): a letter and `bb`, `b`,
  nothing, `#` or `x` (`C`, `F#`, `Bb`, `Cx`). A key with a tonic outside these is not written.

## In memory

The file does not change with the types behind it. In memory a time is a library `Rational` (`<core/rational.h>`) in
whole notes, a span a `Rational` start and length, a cadence a `CadenceType` (the file names PAC, IAC, HC, DC and PC as
`pac`, `iac`, `hc`, `dc`, `pc`), a harmonic function a `HarmonicFunction` with a `has_function` flag (the file leaves
`function` out when it is false), a key an `AnalysisKey` (the file writes `tonic`, spelled, and `mode`), and a note a MIDI
number with an optional spelling (the file writes `spelling` for a spelled note and nothing for an unspelled one).

## Top level

| Key | Content |
| --- | --- |
| `format` | Always `"ursatz-composer-plan"`. |
| `format_version` | Always `2`. |
| `metadata` | `composer_version` (the Composer that wrote the file), `ursatz_commit` (the Ursatz commit the Analyzer was configured against, `ursatz_analyzer_ursatz_commit()`; `scripts/check-pins.sh` keeps it the commit the Composer pins) and, only for a plan loaded from an Analyzer result, `analyzer_version` (`ursatz_analyzer_version()`). |
| `meter` | `null`, or `{"spans": [<claim>, ...]}` for meter spans. |
| `keys` | Key lane claims. |
| `chords` | Harmony lane claims. |
| `form` | The node tree, or `null` for an empty plan. |
| `melody` | Melody lane claims. |
| `texture` | Bass and inner voice lane claims. |

Reading needs `format` and `format_version` (or `schema_version` for version 1) and `form`. A lane section that is absent counts as empty.

## Nodes

`form` is one node object; its `children` hold the rest of the tree, in order.

| Field | Type | Meaning |
| --- | --- | --- |
| `kind` | string | `form`, `section`, `phrase`, `subphrase`, `idea` or `motive`. |
| `label` | string or null | The node's name. |
| `span` | span | Where the node sits on the timeline. |
| `children` | array of nodes | In the order they were added. |
| `composer` | object | See below. |

A form is the root. A section goes under a form, a phrase under a section, a subphrase under a
phrase, an idea under a phrase or subphrase, and a motive under a subphrase or idea. A child's span
lies inside its parent's.

`composer` on a node:

| Field | Type | Meaning |
| --- | --- | --- |
| `id` | integer | The node's id. Ids run from 1 to the node count, with no gaps or repeats, and a parent's id is lower than its children's. A constraint refers to a motive by this id. |
| `state` | string | `given`, `constrained` or `realized`. |
| `constraints` | array | See Constraints. |

## Lane claims

Each lane span is a claim in the section for its lane:

| Lane | Section |
| --- | --- |
| `meter` | `meter.spans` |
| `key` | `keys` |
| `harmony` | `chords` |
| `melody` | `melody` |
| `bass`, `inner_voice` | `texture` |

Claim fields the Composer has no data for are fixed: `framework` is `"ursatz-composer"`,
`note_ids` is `[]` and `uncertainty` is `null`. `label` is for people: the meter (`4/4`), the key
(`C major`), the Roman numeral, or the lane name. A key claim also has `method`, which is
`"composer_plan"`, and a chord claim has `key_label`, the label of the key span at its start or
`"no match"`.

`composer` on a claim:

| Field | Type | Meaning |
| --- | --- | --- |
| `id` | integer | The span's id. Ids run from 1 to the span count across all sections, with no gaps or repeats, and are the order spans were added in. |
| `owner` | integer | The id of the node that owns the span. The span lies inside the node's span. |
| `lane` | string | `meter`, `key`, `harmony`, `bass`, `melody` or `inner_voice`. It must match the section. |
| `voice` | integer | 0, except for inner voices, where it numbers the voice. |
| `state` | string | `given`, `constrained` or `realized`. |
| `constraints` | array | See Constraints. |
| `payload` | object | By lane, below. |

Spans in the same lane and voice do not overlap; a span may start where another ends.

### Payloads

| Lane | Payload |
| --- | --- |
| `meter` | `beats_per_bar`, `beat_unit` (4 is a quarter), `bars_per_hyperbar` (0 for none). |
| `key` | `tonic` (spelled), `mode` (`major` or `minor`). |
| `harmony` | `roman_numeral` (1 to 15 characters), `function` (`tonic`, `predominant` or `dominant`; left out when the chord has none). |
| `bass`, `melody`, `inner_voice` | `notes`: a non-empty array of `{"midi_pitch": 0 to 127, "spelling": <tonic>, "offset": <time>, "duration": <time>}`; `spelling` is only there for a spelled note (read from MusicXML). `offset` is measured from the start of the span, and a note ends inside the span. An `inner_voice` span may have an empty array: the voice is silent over the span. |

## State

Every node and lane span is in one state. New ones are `constrained`.

| State | Meaning |
| --- | --- |
| `given` | Fixed by the user. Never changed: a write, a state change or a constraint change is refused. |
| `constrained` | Not written yet, and has requirements. |
| `realized` | Written by the Composer. Can be regenerated. |

Node state and span state are independent: a Given node does not lock its children or its spans. `plan_lock`
(`ursatz-composer lock`) sets both for a node, every node under it and the spans they own, so a lock reads in a file as
`given` on all of them; see [regenerate.md](regenerate.md). A plan file numbers its nodes and lane spans from 1 without
gaps, so the ids of a plan written after `regenerate` can differ from the ids of the plan read.

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
| `free_phrase` | No fields. On a section node: a phrase of any length in beats, not a theme; `grow-theme` builds it as ideas of a bar. See [grow.md](grow.md#free-phrases). |
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

## Reading

- Unknown fields are ignored, as the Analyzer's versioning rule says. Unknown values in a field
  the Composer owns (`kind`, `state`, `lane`, a constraint `kind` or its enums) are an error.
- A failure returns `PLAN_ERR_PARSE` for text that is not JSON, `PLAN_ERR_FORMAT` for JSON that is
  not a valid plan, `PLAN_ERR_IO` for a file that cannot be read, or `PLAN_ERR_ALLOC`. The
  `PlanJsonError` gives a `path` such as `form.children[1].span.end.den`, a `message`, and the byte
  `offset` for a syntax error.
- A lane claim without a `composer` object is an error. An analyzed piece is loaded with
  `plan_from_analyzer_result`, below.

## Format versions

`format_version` 2 is the format this document describes. `schema_version` 1 is the format the Composer wrote before it,
in the shape of an Ursatz-Analyzer analysis document; `plan_from_json` still reads it (a file with `format` is version 2,
a file with `schema_version` is version 1) and nothing writes it. What version 1 had that version 2 does not:

| Version 1 | Read as |
| --- | --- |
| `schema_version` 1 | `format_version` 2 once written. |
| `metadata`: `source_file`, `analyzer_version`, `ursatz_commit` `"unknown"`, `note_count`, `ticks_per_quarter`, `key_found` | Ignored. `analyzer_version` was the Composer's own version, not an Analyzer's (B-A-10), so the plan has no Analyzer version after reading it. |
| `cadences`, `phrases`, `motives`, `rhythm`, `voice_leading`, `schemas`, always `[]` | Ignored; version 2 does not write them. |
| A key's `tonic_pitch_class` (0 to 11) | The tonic the library spells for that pitch class and mode (`standard_tonality_key_tonic_of_pitch_class`: fewest accidentals in the signature, sharps on a tie): 3 minor is `D#`, 6 major is `F#`. |
| A chord's `function` of `unknown` | No function (the member is left out in version 2). |
| A motive ref's `transform` and its members | The relation step ([Motive relation steps](#motive-relation-steps)). |

Writing the plan again gives version 2: `plan_from_json` then `plan_to_json` turns any version 1 plan into its version 2
file, and the two render the same MIDI.

## Loading an analyzed piece

`plan_from_analyzer_result` (`<ursatz_composer/analysis_load.h>`) builds a plan from an `AnalyzerResult` and the
`ImportedNotes` it was made from, in which every node and lane span is `realized`. It reads the Analyzer's results, not
its analysis document, and records `ursatz_analyzer_version()` as the plan's `metadata.analyzer_version`.

| Analyzer result | Plan |
| --- | --- |
| The piece | A form root from time 0 to the end of the last note or chord. |
| The imported meter map | The meter lane: one span for each run of regions with one signature. A pickup measure joins the span of the measure after it. |
| The chain's key regions | The key lane: a span for each region, the tonic as the library spells it. |
| `analyzer_chord_sequence` | The harmony lane: a span for each joined chord, the root-position numeral, no function. |
| The chain's phrases | A section over the piece with a phrase under it for each phrase. |
| The chain's soprano and bass lines | The melody lane and the bass lane: one span over the piece with a note for each run of segments one note sounds in. |

Nothing is dropped silently. The report lists:

- a pickup measure (joined to the next measure's meter)
- a phrase that overlaps the one before it or does not fit the piece (skipped)
- a key region or chord that cannot be placed on the timeline (skipped)
- a line with no notes (no lane written)
- an area of the result with claims the plan has no place for yet: `cadences`, `themes`, `motives`, `texture`,
  `voice_leading`, `schemas`

A result with no notes, or with no key region and no chord, is an error (`PLAN_ERR_FORMAT`), since there is no timeline
to build on. An analysis document is not read: `fill` refuses one and names `plan_from_analyzer_result`.

## Motive documents

A motive is a short cell of notes the user supplies (`<ursatz_composer/motive.h>`). It is read from MIDI,
MusicXML, a note list or JSON, and attaches to a plan as a Given motive node with a Given melody lane span
(`motive_attach`). The JSON below is the motive on its own and the form `motive_from_json` reads and
`motive_to_json` writes. It uses the plan's vocabulary: the meter and key are payloads of the meter and key
lanes, and `notes` is the note payload of a melody lane, with times as in the conventions above.

| Key | Content |
| --- | --- |
| `meter` | Optional. `beats_per_bar` (1 to 64), `beat_unit` (1, 2, 4, 8 or 16) and optionally `bars_per_hyperbar`. |
| `key` | Optional. `tonic_pitch_class` (0 to 11) and `mode` (`major` or `minor`). |
| `notes` | A non-empty array of at most 64 `{"midi_pitch": 0 to 127, "offset": <time>, "duration": <time>}`. `offset` is measured from the start of the motive, so a gap between notes is a rest and a first note after 0 is a leading rest. Notes are in order and do not overlap. A tie is one note as long as both. |
| `form` | Optional, `full` (the default), `rhythm` or `contour`. A rhythm keeps only the durations and onsets (every note sounds on one pitch); a contour only the heights of the notes in relation to each other (even quarters). Written only when it is not `full`. |
| `pickup` | Optional. A time in whole notes: the length before the first barline, shorter than one bar. The notes still start at 0; bars and beats are counted from the barline. Written only when it is not 0. |
| `harmony` | Optional. The motive's own chords as text, `"I V6@2 I@3"`: upper case Roman numerals (`b` or `#` before one for an altered root; figures `6`, `64`, `7`, `65`, `43`, `42`) and `@` and the onset in beats of the beat unit from the start of the motive, a whole number or a fraction. The first chord needs no onset. At most 8 chords, onsets rising and inside the motive. Written only when the motive has chords. |
| `features` | Optional. Up to 8 labelled runs of notes, `{"label": 1 to 8 characters, "first": note index, "last": note index}`, 0-based and inclusive, labels different. Written only when the motive has features. |
| `role` | Optional, `melodic` (the default) or `accompaniment`. Written only when it is not `melodic`. |

The motive ends where its last note ends. Unknown fields are ignored; a value that is not allowed is an error
that names the field, for example `notes[2].duration.den`.

```json
{
  "meter": {"beats_per_bar": 4, "beat_unit": 4, "bars_per_hyperbar": 0},
  "key": {"tonic_pitch_class": 2, "mode": "minor"},
  "notes": [
    {"midi_pitch": 62, "offset": {"num": 0, "den": 1}, "duration": {"num": 1, "den": 4}},
    {"midi_pitch": 65, "offset": {"num": 1, "den": 4}, "duration": {"num": 1, "den": 8}},
    {"midi_pitch": 69, "offset": {"num": 3, "den": 8}, "duration": {"num": 1, "den": 8}},
    {"midi_pitch": 74, "offset": {"num": 1, "den": 2}, "duration": {"num": 1, "den": 2}}
  ]
}
```

The meter is the file's own for MIDI and MusicXML (4/4 when a file has none), and a meter given by the caller
that differs from a declared one is an error. A note list has no meter and needs one from the caller. The key is
the caller's, else the input's (a key signature with a major or minor mode, or `key` above); otherwise it is
inferred when the pitches settle on one key, and is left out when they do not. A key that was inferred is not
written, since reading the motive infers it again.

### Motive analysis

`motive_analysis_to_json` writes the motive and its analysis together, as an object with a `motive` member (the
document above) and an `analysis` member (and, from `motive --render-key`, `--midi` or `--musicxml`, a `rendered` member: `{"key", "notes": [{"midi_pitch", "offset", "duration"}]}`, see [cli-motive.md](cli-motive.md#rendering)):

| Key | Content |
| --- | --- |
| `form`, `pickup` | The motive's form, and its pickup in beats of the beat unit as text: `"0"`, `"1"`, `"1/2"`. |
| `notes_text`, `degrees_text` | The notes as a note list (`D4:q F4:e`), and as a degree list (`1:q 3:e`, `null` without a key). |
| `rhythm_text`, `contour_text` | Only for the form: the rhythm list (`x:q x:e`) of a `rhythm` motive, the contour list (`0 2 1 3`) of a `contour` motive. |
| `note_count` | The number of notes. |
| `length`, `bars` | The motive's length in whole notes, and as a number of bars of its meter. Times. |
| `key` | `null` without a key. Otherwise `tonic_pitch_class`, `mode` and `source`: `given` or `inferred`. |
| `intervals` | `null` for a rhythm or a contour. Otherwise one per pair of neighboring notes: `semitones` (signed) and `steps` (signed diatonic steps in the key, `null` without a key). |
| `contour` | `null` for a rhythm. Otherwise `shape` (`single note`, `level`, `ascending`, `descending`, `arch`, `valley` or `wave`) and `directions`, one of `up`, `down` and `same` per interval. |
| `rhythm` | `null` for a contour. Otherwise one per note: `onset`, `duration`, `bar` (the first full bar is 0, and a pickup's notes are in bar -1), `beat` (the position in the bar in beats of the beat unit, 0 is the downbeat; a time), `on_beat`, `dotted` (the duration is 3 or 7 over a power of two) and `rest_before` (a time). |
| `range` | `null` for a rhythm or a contour. Otherwise `lowest` and `highest` MIDI pitch, and `semitones` between them. |
| `scale_degrees` | `null` without a key, and for a rhythm or a contour. Otherwise `start`, `end` and `all` (one per note), each `{"degree": 1 to 7, "alteration": 0 or 1}`; 1 is a raised degree. |
| `harmony_text` | Only for a motive with chords of its own: the chords as text, as `harmony` above. |
| `implied_harmony` | `key_known`, and `candidates`, best fit first: `roman_numeral`, `function`, `fit` (the share of the motive that is chord tones, 0 to 1, to four places), `chord_tones` and `non_chord_tones`. Empty without a key, and for a rhythm or a contour. |
| `base_harmony` | `null` without a key, and for a rhythm or a contour. Otherwise `text` (the base harmony as `harmony` above) and `given` (true when it is the motive's own harmony, false when it was proposed from the notes; at most 2 chords in a bar). |
| `tones` | One object per note, `[]` when `base_harmony` is `null`: `note` (1-based), `role` (`chord`, `non-chord` or `unclassified`), `type` (`passing`, `neighbor`, `appoggiatura`, `suspension`, `retardation`, `escape`, `anticipation`, `double-neighbor`, `changing-tone` or `pedal` for a non-chord tone, else `null`), `resolves_to` (1-based note or `null`), `unresolved` and `overridden` (the label came from `--tone`). |
| `warnings` | One string per unresolved note: `Note 3 (B5) does not resolve`. |

For the motive above, `scale_degrees` gives `1 3 5 1`, the contour is `ascending` and the intervals are `+3`, `+4`
and `+5` semitones, `+2`, `+2` and `+3` steps. The best implied chord is `i`, with `fit` 1.

The analysis is deterministic: the same motive gives the same document, byte for byte.

## Example

A two-bar plan: a form in 4/4 and C major, one section, one chord. This is the exact output of
`plan_to_json`.

```json
{
  "format": "ursatz-composer-plan",
  "format_version": 2,
  "metadata": {
    "composer_version": "0.1.0",
    "ursatz_commit": "36cb9f306b5fdf07f7e907cb2836b2e39fee70ed"
  },
  "meter": {
    "spans": [{
        "framework": "ursatz-composer",
        "span": {
          "start": {
            "num": 0,
            "den": 1
          },
          "end": {
            "num": 2,
            "den": 1
          }
        },
        "note_ids": [],
        "label": "4/4",
        "uncertainty": null,
        "composer": {
          "id": 1,
          "owner": 1,
          "lane": "meter",
          "voice": 0,
          "state": "given",
          "constraints": [],
          "payload": {
            "beats_per_bar": 4,
            "beat_unit": 4,
            "bars_per_hyperbar": 0
          }
        }
      }]
  },
  "keys": [{
      "framework": "ursatz-composer",
      "span": {
        "start": {
          "num": 0,
          "den": 1
        },
        "end": {
          "num": 2,
          "den": 1
        }
      },
      "note_ids": [],
      "label": "C major",
      "uncertainty": null,
      "method": "composer_plan",
      "composer": {
        "id": 2,
        "owner": 2,
        "lane": "key",
        "voice": 0,
        "state": "realized",
        "constraints": [],
        "payload": {
          "tonic": "C",
          "mode": "major"
        }
      }
    }],
  "chords": [{
      "framework": "ursatz-composer",
      "span": {
        "start": {
          "num": 0,
          "den": 1
        },
        "end": {
          "num": 1,
          "den": 1
        }
      },
      "note_ids": [],
      "label": "I",
      "uncertainty": null,
      "key_label": "C major",
      "composer": {
        "id": 3,
        "owner": 2,
        "lane": "harmony",
        "voice": 0,
        "state": "constrained",
        "constraints": [{
            "kind": "length_bars",
            "bars": 1
          }],
        "payload": {
          "roman_numeral": "I",
          "function": "tonic"
        }
      }
    }],
  "form": {
    "kind": "form",
    "label": "example",
    "span": {
      "start": {
        "num": 0,
        "den": 1
      },
      "end": {
        "num": 2,
        "den": 1
      }
    },
    "children": [{
        "kind": "section",
        "label": "A",
        "span": {
          "start": {
            "num": 0,
            "den": 1
          },
          "end": {
            "num": 2,
            "den": 1
          }
        },
        "children": [],
        "composer": {
          "id": 2,
          "state": "constrained",
          "constraints": [{
              "kind": "cadence",
              "type": "pac"
            }]
        }
      }],
    "composer": {
      "id": 1,
      "state": "given",
      "constraints": []
    }
  },
  "melody": [],
  "texture": []
}
```
