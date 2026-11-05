# Voice leading claims

Claims about voice leading, schemas and sequences, markings and the counterpoint check in the analysis document. Back to the [docs index](../README.md) and the [analysis document](../analysis-document.md).

## Voice leading

`voice_leading` is one flat array of claims, each labelled by the result it is: one `"motions"` claim, then the
`"violation"` claims in time order, the `"resolution"` claims, and the `"doubling"` claims. The analyzer builds the
voice-pitch grid of the adapters of Ursatz's standard-counterpoint and standard-harmony in
`src/pipeline/voice_leading_pipeline.c`, and copies the library's
results. The kernel's voice separation runs over every note as one stream (voice 0 is the top); the grid has one row
per chord segment (row `i` is `chords[i]`), at most six voices (lower voices are left out), and each cell is the note of
that voice sounding at the start of the segment. A claim's `framework` is the library's own: `"standard-counterpoint-v1"` for
`motions` and `violation`, `"standard-harmony-v1"` for `resolution` and `"standard-voicing-v1"` for `doubling`. A piece
the separation or the library declines has an empty `voice_leading`.

**Motions** (`label` `"motions"`): `span` covers the whole piece and `note_ids` is every imported note.

| Field | Type | Meaning |
| --- | --- | --- |
| `uncertainty` | number | The mean of the library's per-motion uncertainty (1 less the least confidence of the voice separation), in [0, 1]. |
| `voices`, `segments` | integer | The grid's columns and rows. |
| `motion_count` | integer | Motions classified: each pair of voices that sound in two consecutive segments. |
| `totals` | object | `{"contrary", "similar", "parallel", "oblique", "static"}`: the library's motion types, counted. |
| `pairs` | array | One per pair of voices: `{"upper_voice", "lower_voice", "total", "counts"}`, `counts` as `totals`. |

**Violation** (`label` `"violation"`): the move from chord `from_chord` to `to_chord` (consecutive); `span` runs from the
start of the first chord to the end of the second, and `note_ids` are the upper voice's notes then the lower voice's.

| Field | Type | Meaning |
| --- | --- | --- |
| `uncertainty` | number | The library's: 1 less the least voice separation confidence of the four notes. |
| `rule` | string | The library's name: `"parallel-fifths"` or `"parallel-octaves"` (any pair of voices; a twelfth is a fifth, a unison an octave) or `"direct-perfects"` (the outer pair, similar motion with the top voice leaping). |
| `upper_voice`, `lower_voice` | integer | The voices of the pair (0 is the top). |
| `interval` | integer | The simple size arrived at: 5, 8 or 1. |
| `from_chord`, `to_chord` | integer | Indices into `chords`. |

**Resolution** (`label` `"resolution"`): a tendency tone left unresolved; only unresolved tones are reported.

| Field | Type | Meaning |
| --- | --- | --- |
| `uncertainty` | number | The library's: the voice's and the chord's confidence together. |
| `rule` | string | The library's name: `"seventh-resolves-down"` or `"leading-tone-resolves"`. |
| `voice` | integer | The voice that carries the tone. |
| `from_chord`, `to_chord` | integer | The last segment of the chord that resolves and the first of the next. `note_ids` are the tone and the note it moved to. |

**Doubling** (`label` `"doubling"`): one per chord segment that three or more voices sound and the harmony stage read as a
chord; `span` is the chord's and `note_ids` are the sounding voices' notes, top first.

| Field | Type | Meaning |
| --- | --- | --- |
| `uncertainty` | number | 1 less the least voice separation confidence of the sounding notes. |
| `chord` | integer | Index into `chords`. |
| `voices` | integer | Voices sounding (3 or more). |
| `doubled` | object or null | `{"member", "voices"}`: the chord member (`"root"`, `"third"`, `"fifth"` or `"seventh"`, the positions of the chord's interval set) that most voices sound, when more than one does; null otherwise. Counted, not judged. |
| `non_chord_voices` | integer | Voices on a pitch class the chord does not have. |
| `gaps` | array | Semitones from each sounding voice down to the next, top first (`voices - 1` of them; negative when voices cross). |
| `spacing` | object | `{"judged", "valid", "wide_voice"}`: judged only when the grid has four voices and all four sound; then standard-voicing's rule (an octave between soprano and alto and between alto and tenor, a twelfth between tenor and bass) gives `valid` and, when false, the upper voice (0, 1 or 2) of the first pair too wide. Otherwise `valid` and `wide_voice` are null. |

The Markdown report renders `## Voice leading` from this section: the motion counts and table, the faults with their
locations, and the doubling summary with the chords whose spacing is too wide.

## Schemas and sequences

`schemas` is one flat array of claims, each labelled by the result it is: the `"schema"` claims in order of their first
stage, then the `"sequence"` claims in order of their first segment. The analyzer builds the neutral input of the
galant-schemas and scale-harmonization adapters in `src/pipeline/schema_pipeline.c`, and copies the library's matches. A claim's `framework` is the library's own:
`"galant-schemas-v1"` for schemas and `"scale-harmonization-v1"` for sequences. `span` runs from the first chord the
claim uses to the end of the last, `note_ids` are the notes sounding in them and `uncertainty` is 1 less the library's
confidence. A degree is `{"degree", "alteration"}`: the library's letter distance from the tonic (1 to 7) and the
alteration in semitones against the major scale (-1 lowered, +1 raised). A piece with no key region has an empty `schemas`.

**Schema** (`label` `"schema"`):

| Field | Type | Meaning |
| --- | --- | --- |
| `name` | string | The library's schema name (`"romanesca"`, `"prinner"`, `"jupiter"`, ...). |
| `function` | string | The library's: `"opening-gambit"`, `"continuation"`, `"pre-cadential"`, `"cadence"` or `"post-cadential"`. |
| `measures` | object | `{"first", "last"}`: the measures of the first stage and of the last chord used. |
| `frame` | degree | The key the stages were read in, as a degree of the home key (`1` is the home key itself). |
| `bass_only_stages` | integer | Stages matched on the bass alone (the library allows one). |
| `confidence` | object | The library's `{"pair_share", "metric_fit", "mean_uncertainty", "overall"}`, each in [0, 1]. |
| `stages` | array | Per stage: `span`, `location`, `measure`, `soprano` (a degree, null for a bass-only stage), `second_soprano` (the second note of a two-note stage, else null), `bass` (a degree), `bass_only` and `credit` (1, 0.75 or 0.5). |

**Sequence** (`label` `"sequence"`):

| Field | Type | Meaning |
| --- | --- | --- |
| `kind` | string | The library's: `"parallel-chords"`, `"five-six"`, `"seven-six"`, `"two-three"` or `"cycle-of-fifths"`. |
| `pattern` | string or null | The library's pattern name; null for the cycle of fifths, which no pattern encodes. |
| `rule_backed` | boolean | The library's own rules hold over whole units; false for the cycle of fifths, found from the Roman numerals alone. |
| `direction` | string or null | `"ascending"` or `"descending"`; null for the cycle of fifths. |
| `unit_length`, `units` | integer | Chord segments in one model, and how often it sounds (3 or more). |
| `transposition_steps` | integer | The bass, in diatonic steps, from the start of one unit to the next (up is positive). |
| `measures` | object | `{"first", "last"}` as for a schema. |
| `confidence` | object | The library's `{"unit_share", "soprano_bonus", "mean_uncertainty", "overall"}`. |
| `stages` | array | Per segment: `span`, `location`, `measure`, `bass`, `soprano` and `root` (degrees in the home key; soprano and root null where the segment has none). |

The Markdown report renders `## Schemas and sequences` from this section: a table of the schemas with their stages, and a
`### Sequences` table.

## Counterpoint check

`counterpoint_check` is `null` unless the CLI ran with `--check-counterpoint <1-5>` on a two-voice input. It is then
`{"species", "cantus", "framework", "shape_error", "valid", "violations"}`. `src/pipeline/counterpoint_pipeline.c` finds
the two voices (the input's two streams, the higher-sounding one the upper voice, or the two voices the kernel's voice
separation finds in one stream, with none of its notes joined to another), builds the lines of the species and runs the
library's species check (`standard_counterpoint_first_species_check` to `_fifth_species_check`). Every verdict is the
library's (R-018). The tonic is the tonic of the home key region.

| Field | Type | Meaning |
| --- | --- | --- |
| `species` | integer | 1 to 5. |
| `cantus` | string | `"lower"` (the default) or `"upper"`: the voice that is the cantus firmus. |
| `framework` | string | `"standard-counterpoint-v1"`. |
| `shape_error` | string or null | Why the voices are not an exercise of the species (for example the cantus firmus is not one note to a measure, or the counterpoint has notes off the species' slots); `null` when they are. |
| `valid` | boolean or null | `true` when the library finds no hard rule broken; `null` when `shape_error` is set. |
| `violations` | array | One claim per rule group the exercise breaks, the hard one before the soft one in a group; empty when `shape_error` is set or nothing is broken. |

A violation is a claim (`label` is the group) with `group` (the library's group name: `cantus-firmus`, `melodic`,
`harmonic`, `harmony`, `ends`, `strong-beats`, ...) and `severity` (`"hard"`, the exercise is invalid, or `"soft"`,
advice). The library's check reports each group over the whole exercise and does not say where a rule broke, so a
claim's `span` and `location` are the two voices', its `note_ids` are every note of both voices, and its
`uncertainty` is 1 less the least confidence of the voice separation (0 when the input's own streams are the voices).
Species 2 to 5 need one cantus firmus note to a measure, from a measure start, with the counterpoint's notes starting on
the slots of its measures (2, 4, 2 and 8 slots to a measure for species 2 to 5).

## Markings

`markings` is an object: `{"source", "tempo", "dynamics", "articulations", "articulations_imported"}`. The MusicXML
importer's tempo and dynamics markings are kept on `ImportedSource` (`tempo_markings`, `dynamics`, each with its part)
and `src/pipeline/markings_pipeline.c` reads them with Ursatz's standard-markings rules. The words, the metronome marks,
the positions and the spans are the score's; every group, rank, change and comparison is the library's (R-018). A claim's
`framework` is `"standard-markings-v1"`, its `note_ids` are empty, and its `uncertainty` is 0 when the library reads the
marking and 1 when it does not.

| Field | Type | Meaning |
| --- | --- | --- |
| `source` | string | `"notated"` for MusicXML, `"none"` for MIDI (a MIDI file carries no markings; `tempo` and `dynamics` are then empty). |
| `tempo` | array | Tempo claims (`label` `"tempo"`) in time order. |
| `dynamics` | array | Dynamics claims (`label` `"dynamic"`) in time order. |
| `articulations` | null | Always `null`: the importer reads no articulations (R-056). |
| `articulations_imported` | boolean | Always `false`. |

**Tempo claim.** `span` runs from the marking to the next tempo marking, or to the end of the piece.

| Field | Type | Meaning |
| --- | --- | --- |
| `text` | string or null | The marking's words (`"Allegro"`, `"rit."`); `null` for a metronome mark alone. Words count only when the direction also holds a metronome mark or a sound tempo (the importer's rule). |
| `metronome` | object or null | `{"beat_unit", "per_minute"}`, both rationals: the beat unit in whole notes and the beats per minute. |
| `part` | integer | Index into `metadata.input.parts`. |
| `rank` | integer or null | The library's rank of a tempo word, 0 (grave) to 9 (presto); `null` for text that is not a tempo word. |
| `group` | string or null | `"slow"`, `"medium"` or `"fast"`, the library's group of the text's first word; `null` when `rank` is. |
| `change` | string | `"none"`, `"gradually-slower"` (rit.), `"gradually-faster"` (accel.) or `"return"` (a tempo). |

**Dynamic claim.** `span` is the importer's: from the marking to the next marking in its part, or to the part's end.

| Field | Type | Meaning |
| --- | --- | --- |
| `marking` | string | The score's text: `"p"`, `"mf"`, or an other-dynamics text such as `"cresc."`. |
| `part`, `staff` | integer | Index into `metadata.input.parts`, and the staff of its direction. |
| `rank` | integer or null | The library's loudness rank of a level marking (quiet marks negative, loud marks positive); `null` for a text that is not a level. |
| `change` | string | `"none"`, `"louder"` (crescendo) or `"quieter"` (decrescendo, diminuendo), as the library reads the text. |
| `relation` | string or null | `"louder"`, `"quieter"` or `"same"` against the previous level marking in the same part; `null` for the first level marking and for a text that is not a level. |

The importer reads no wedges (crescendo and diminuendo hairpins) and no articulations, so neither appears.
