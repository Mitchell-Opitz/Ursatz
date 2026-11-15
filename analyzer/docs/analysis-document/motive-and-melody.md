# Motive and melody claims

Claims about motives and melody in the analysis document. Back to the [docs index](../README.md) and the [analysis document](../analysis-document.md).

## Motives

`motives` holds one claim per motive family the library's standard-form motive analysis finds
(`analysis/analysis_motive.h`), in the library's order: by prototype, longer first. The analyzer runs
`standard_form_find_motives` with the default config (3 to 8 notes, one mismatch for a varied motive, soprano only) in
`src/pipeline/motive_pipeline.c`. A motive is a short figure that recurs; its occurrences, their kinds and their scores are the
library's.

The motive analysis needs a note-level soprano, which the outer voices cannot give (they keep one note per voice
segment). The analyzer builds it as the highest sounding note at each attack, the later attack among equals, kept when
it is attacked there, with each note's scale degree in the key region it starts in. It builds a form input of its own
from that soprano, the chord segments and the cadences, and reruns the library's phrases, ideas and themes on it
(they are the motive analysis's input). Those are not the report's phrases, which come from the voice-segment
soprano, so each occurrence is placed in the report's phrases and ideas by span.

A motive is a claim (`framework` `"standard-form-v1"`; `span`, `location` and `note_ids` are the prototype's):

| Field | Type | Meaning |
| --- | --- | --- |
| `label` | `"motive"` | |
| `uncertainty` | number | The greatest uncertainty of its occurrences. |
| `index` | integer | The family's position in `motives`, from 0. |
| `note_count` | integer | The prototype's length in notes, 3 to 8. |
| `generic` | boolean | The pattern is the scale-step interval sequence, so a diatonic transposition matches; otherwise the semitone sequence. |
| `occurrences` | array | Every occurrence, at least two: the prototype first, then the library's order, which is not always time order (it searches continuations for fragments first, so a prototype can come after fragments of itself). |
| `development` | array | The occurrences in time order (see below). |
| `features` | object | Optional: what the family is made of, read from its prototype (see below). Absent when the first note has no key region or meter. |

An occurrence is a claim over its soprano notes. Its `label` is the library's kind of the occurrence against the
family's prototype, the transformation: `"exact"` (the same pitches and rhythm; the prototype is always exact),
`"real-transposed"` (the same semitone intervals from another pitch), `"tonal-transposed"` (the same scale steps), and
`"inverted"`, `"retrograde"`, `"augmented"`, `"diminished"`, `"fragmented"` and `"varied"`.

| Field | Type | Meaning |
| --- | --- | --- |
| `span`, `location`, `note_ids` | | From the first note's start to the last note's end; the soprano notes of the occurrence. |
| `uncertainty` | number | 1 minus `score`. |
| `index` | integer | Within the family; 0 is the prototype. |
| `measures` | object | `{"first", "last"}`: the measure the first note starts in, and the last note. |
| `beat` | number | The 1-based beat of the first measure the first note starts on, fractional off the beat. |
| `score` | number | The library's match score, 1 minus `mismatches` over `intervals`, times its rhythm factor. |
| `mismatches`, `intervals` | integer | The interval mismatches of a varied occurrence (else 0), and the intervals compared. |
| `fragment` | string or null | A fragmented occurrence: `"prefix"` or `"suffix"` of the prototype. |
| `in_continuation` | boolean | A fragment found inside a continuation span of a theme. |
| `phrase` | integer or null | The index into `phrases` of the phrase the whole occurrence lies in; `null` when it crosses a phrase boundary. |
| `idea` | integer or null | The index (within that phrase's `ideas`) of the idea it lies in; `null` when it lies in none. |
| `degrees` | array of strings | Optional: each note's degree against the major scale of the tonic of the key region the note starts in (see `features`). |

`development` is the family's occurrences in time order, one step per occurrence, the sequence of transformations the
figure goes through in the piece; the analyzer only orders them. A step is `{"occurrence", "measure", "beat",
"transformation", "from"}`: `occurrence` is the index into the family's `occurrences`, `transformation` the library's
kind of that occurrence (`null` for the prototype, the original, which is not always the earliest), and `from` the
occurrence before it in time (`null` for the first).

`features` describes the family by its prototype so families of different pieces can be compared and linked. It is
optional and additive (the schema version stays 9); the analyzer version is raised with it, so a corpus `--resume`
and the GUI re-analyze older documents.

| Field | Type | Meaning |
| --- | --- | --- |
| `notes` | array | Per prototype note `{"note_id", "pitch", "onset", "duration"}`: the ImportedNotes index, the MIDI pitch, and the onset (from the first note) and duration as exact `"n/d"` whole-note strings. |
| `tonic_pc`, `minor` | integer, boolean | The key region the first note starts in. |
| `bar`, `beat` | string | The meter at the first note as `"n/d"` whole notes; the beat is three units in 6/8, 9/8 and 12/8. |
| `pickup` | string | The first note's position in its bar, `"0/1"` on a downbeat. |
| `degrees` | array of strings | Per note, its pitch class against the tonic's major scale: `1 b2 2 b3 3 4 #4 5 b6 6 b7 7`. A minor-key `1 b6 5` reads the same as one in major. |
| `intervals`, `steps` | array of integers | Signed semitones and signed scale steps (of the key region's scale) between neighbours. |
| `rhythm` | array of strings | The durations as reduced `"n/d"` strings. |
| `ncts` | array | Per note, the non-chord-tone type of the segment the note starts in; `"chord-tone"` for a chord tone there; `null` when the segment has no chord or the library left the tone unclassified. |
| `fingerprint` | string | 16 lowercase hex digits: the library's `motive_sketch_fingerprint` of the prototype as a sketch (the notes, the scale, the meter and the pickup; no chords, features or role). Equal for equal sketches, so a cache can key on it. |

A sketch cannot hold overlapping notes, so a prototype note held past the next onset is cut at it in the sketch; the
features keep the real durations. The sketch is built by `motive_features_to_sketch` (`src/pipeline/motive_sketch_map.c`),
from these fields alone.

The Markdown report renders `## Motives` from this section, a row per family (notes, degrees, pattern, occurrence count,
measures, phrases, kinds), and `### Development`, a line per family as `m1 original > m3 real-transposed > m5
fragmented`.

## Melody

`melody` holds one claim per phrase that has soprano notes, in phrase order: the melody of that phrase, read by the
melody adapter of Ursatz's standard-counterpoint (`analysis/analysis_melody.h` and the files beside it). The analyzer
builds the adapter's neutral line in `src/pipeline/melody_pipeline.c`, and groups the results; every interval, direction,
climax and verdict is the library's. The line of a phrase is the outer voice soprano (the highest sounding line, one
entry per voice segment) for the notes whose first segment starts inside the phrase's span: consecutive segments with
the same note id are one note, a note held in from the previous phrase is left out, and a gap in the line is a rest that
breaks the chain, so no interval is formed across it. Each note carries its scale degree in the home key
and the beat strength of its attack. The bass is not read.

A melody is a claim (`framework` `"standard-counterpoint-v1"`; `span` is the phrase's, `location` its position and
`note_ids` the soprano notes of the line, in time order):

| Field | Type | Meaning |
| --- | --- | --- |
| `label` | `"melody"` | |
| `uncertainty` | number | The library's, from the confidence of the notes (the line gives each note full confidence, so 0). |
| `index` | integer | The phrase's index into `phrases`. |
| `measures` | object | `{"first", "last"}`: the phrase's measures, as in `phrases`. |
| `note_count` | integer | The notes of the line. |
| `range` | object | `{"lowest", "highest", "semitones"}`; `lowest` and `highest` are `{"pitch", "name", "note_id"}`: the MIDI number, the name as the outer voices spell it (`"F#4"`), and the first note at that pitch. |
| `climax` | claim | The climax, the first note at the highest pitch (the melodic rules' definition), as a claim over that note (`label` `"climax"`) with `pitch`, `occurrences` (the notes at the highest pitch), `unique`, `relative_position` (its rank among the notes over the notes less one, by note order, not by time), `strength` (the library's beat strength name of its attack, or `null`), `by_leap` (a leap approaches it) and `final_note` (it is the phrase's last note). |
| `contour` | object | The intervals between consecutive notes: `up`, `down` and `same` counts, `steps` and `leaps` counts (a step is a second, a leap a third or larger, by letter names where the line has them), `net_direction` (`"up"`, `"down"` or `"same"`: the last note against the first), `net_semitones`, and `directions`, the library's direction of each interval in time order. No shape is named. |
| `leaps` | object | `{"recovered", "unrecovered", "not_required", "not_judged", "items"}`: the counts of the library's verdicts and `items`, one claim over the two notes of every leap (`label` `"leap"`) with `semitones`, `direction`, `generic_size` (the size in letter names, a third is 3, or `null`), `approximate` (read from the semitones alone) and `status`. The verdict is the cantus firmus rule LEAP_RECOVERY: a leap of a fourth or larger is `"recovered"` when a step in the opposite direction follows, `"unrecovered"` when a step the same way, a repeated note or another leap does, and `"not-judged"` when nothing follows; a third is `"not-required"`. |
| `consecutive_leaps` | array | The pairs of leaps in a row that the library's rule CONSECUTIVE_LEAPS finds fault with, each a claim over the three notes (`label` `"consecutive-leaps"`) with `same_direction` (the two go the same way and are not two thirds outlining a perfect fifth) and `three_in_a_row`. |
| `non_chord_tones` | object | `{"chord_tones", "non_chord_tones", "unclassified", "by_type", "items"}`: each note set against the chord of the segment it starts in, as the library's chord identification reads it. `unclassified` counts the notes in a segment with no chord, or that the library left unnamed. `by_type` is `[{"type", "count"}]` for the types that occur, in the library's order; `items` is a claim over each non-chord tone labelled with the library's type (`"passing"`, `"neighbor"`, `"escape-tone"`, ...). The adapter labels nothing itself: it attaches the chord identification's result to the note. |
| `tendencies` | object | `{"line", "seventh_degree"}`; see below. |

The library names its tendencies in two places, and neither is a table of scale-degree pairs, so the analyzer reports
what is there. `line` is `[{"tendency", "holds"}]` for the five melodic tendencies of `tendency_rules.h` (after Huron):
`"pitch-proximity"`, `"step-declination"`, `"step-inertia"`, `"melodic-regression"` and `"melodic-arch"`, each judged by
the library over the phrase's notes (the line of notes with rests dropped), spelled as the outer voices spell them (empty
when a note's name does not spell). `seventh_degree` is `{"rule", "followed", "broken", "breaks"}`, or `null` when the
notes do not spell: every note the library labels the seventh degree of its key region, with a note after it, is judged by
the cantus firmus rule LEADING_TONE (`"leading-tone"`) on the pair, in the key of the note's region. A leading tone (a
half step below the tonic) goes up to the tonic by step, and a seventh a whole step below the tonic does not step up to
it. `followed` counts the notes that follow the rule and `broken` those that do not, and `breaks` has a claim over each
broken seventh and the note after it (`label` `"leading-tone"`). The library has no rule for 4 to 3 or any other pair.

The Markdown report renders `## Melody` from this section, a row per phrase (notes, range, climax, contour counts), and
the subsections `### Leaps`, `### Non-chord tones` and `### Tendencies`.
