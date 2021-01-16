# Analysis document

`ursatz-analyzer` writes `<stem>.analysis.json` next to each input file (see the README for the
`--json` and `--markdown` flags). It is the structured, versioned output that the Composer, the
GUI and corpus statistics read. The JSON Schema is
[`schema/analysis-document.v3.schema.json`](../schema/analysis-document.v3.schema.json)
(draft 2020-12). The library renders it with `analysis_document_render` from
`<ursatz_analyzer/analysis_document.h>`.

The output is pretty-printed with a 2-space indent, in the key order shown below, and ends with a
newline. The same input and build always give the same bytes.

## Top level

| Key | Type | v2 content |
| --- | --- | --- |
| `schema_version` | integer | Always `3`. |
| `metadata` | object | Filled. See below. |
| `meter` | meter claim | Filled. See below. |
| `keys` | array of key claims | Filled. |
| `chords` | array of chord claims | Filled. |
| `cadences` | array | Placeholder: `[]`. |
| `phrases` | array | Placeholder: `[]`. |
| `form` | object or null | Placeholder: `null`. |
| `motives` | array | Placeholder: `[]`. |
| `melody` | array | Placeholder: `[]`. |
| `rhythm` | array | Placeholder: `[]`. |
| `texture` | array | Placeholder: `[]`. |
| `voice_leading` | array | Placeholder: `[]`. |
| `schemas` | array | Placeholder: `[]`. |

Every key is always present, so consumers can read any section without checking for it first.
The singular section `form` is `null` until filled; the rest are empty arrays.

## metadata

| Field | Type | Meaning |
| --- | --- | --- |
| `source_file` | string | Input file name without its directory. |
| `analyzer_version` | string | Ursatz-Analyzer version, from the CMake project version. |
| `ursatz_commit` | string | Full commit hash of the `external/ursatz` submodule at configure time, or `"unknown"` when `git` could not read it. |
| `note_count` | integer | Number of imported notes. |
| `ticks_per_quarter` | integer or null | The MIDI header's ticks per quarter note, or `null` when unavailable. |
| `key_found` | boolean | Whether a key was found. |

## meter

`meter` is one claim (the shared claim fields below) for the whole piece. It is always present for
a successfully imported file, even one with no notes.

| Field | Type | Meaning |
| --- | --- | --- |
| `framework` | string | `"midi-time-signature"` when the file declares a time signature; `"ursatz-default-meter"` when it declares none. |
| `span`, `note_ids` | | As for every claim: the extent of all the notes. |
| `label` | string | The time signature, e.g. `"3/4"`. |
| `uncertainty` | null | Always `null`: the library supplies no confidence for a declared or defaulted meter. Use `source` to tell them apart. |
| `source` | `"file"` or `"inferred"` | `"file"` when the file declares at least one time signature. `"inferred"` when it declares none and the library's default 4/4 is used. An inferred meter is an assumption, not a fact about the piece. |
| `time_signature` | object | `{"numerator", "denominator"}` of the first full measure. |
| `measure_count` | integer | Measures that start before the last note ends, a pickup included (the library's count). `0` for a file with no notes. |
| `last_measure` | integer or null | The number of the last measure (see numbering below), or `null` when there are no measures. |
| `pickup` | object or null | `null` when the piece has no pickup. Otherwise `{"length", "beats"}`: `length` is a rational in whole notes; `beats` is the same length in beats of the signature the pickup is read in, a rational, or `null` when that signature has no beat grid. |
| `changes` | array | One entry per later time signature: `{"measure", "start", "time_signature", "inferred"}`. `measure` is the number of the first measure in the new signature; `start` is a rational time in whole notes. Empty when the signature never changes. |

A pickup is detected by the library from what the file says (a partial first measure ended by a
later time signature, or a short leading signature such as 1/8 before 12/8), never from note
content.

## Claims

Every entry in `keys` and `chords` is a claim. A claim mirrors the library's Interpretation.

| Field | Type | Meaning |
| --- | --- | --- |
| `framework` | string | Id of the source that made the claim, e.g. `"common-practice-minimal-v1"`. |
| `span` | object | `{"start": {"num", "den"}, "end": {"num", "den"}}`: rational times in whole notes from the start of the piece, in lowest terms, matching `time_numerator`/`time_denominator` in `AnalysisReport`. |
| `location` | object | `{"start", "end"}`: where the span starts and ends on the measure grid, as positions (see below). |
| `note_ids` | array of integers | The notes the claim covers, as indices into the imported notes. Index `N` is the note Ursatz names `midi-note-N`. |
| `label` | string | The claim's label. |
| `uncertainty` | number or null | The Interpretation's scalar confidence in [0, 1], or `null` when the library supplies none (unknown, or a candidate set at v1). |

### Measure and beat positions

`location.start` and `location.end` are positions, derived from `span` and the library's measure
grid (the same grid that the `meter` section reports). A position is an object, or `null` when it
cannot be read (a time signature with no beat grid):

| Field | Type | Meaning |
| --- | --- | --- |
| `measure` | integer | The measure number. |
| `beat` | integer | The 1-based beat in that measure. |
| `subdivision` | rational | The exact fraction of that beat past its start, `0/1` on the beat. |

The report writes a position as `m. N, beat B`, with the subdivision added to the beat (`beat 2.5`).

Conventions:

- **Measure numbers follow the score, as the library numbers them.** The first full measure is 1.
  A pickup (anacrusis) is measure 0, so the music after it starts at measure 1. A piece with no
  pickup has no measure 0.
- **A pickup sits at the end of an imagined measure.** A one-quarter pickup in 4/4 starts at beat 4
  of measure 0.
- **A beat is the signature's beat.** In 4/4 and 3/4 it is the quarter note; in 6/8 and 12/8 it is
  the dotted quarter.
- **`end` is the position of the span's exclusive end.** A span that ends on a barline ends at
  beat 1 of the next measure, not at the last beat of its own measure.
- **A barline belongs to the following measure.**
- The `meter` claim and the key claim cover the whole piece, so their positions run from the first
  note to the end of the last.

### Key claims

`keys` holds one whole-piece claim, or is empty when no key was found.

- `span` runs from the earliest note onset to the latest note end, and `note_ids` lists every note.
- `label` is the key, e.g. `"C major"` or `"Eb major"`.
- `method` names the step that found the key, in the README's key-estimation order:

| `method` | Step | `framework` |
| --- | --- | --- |
| `key_signature` | 1. MIDI key signature meta-event | `"midi-key-signature"` (no framework is involved) |
| `bass_fallback` | 2. Closing-chord, then opening-chord bass as tonic | the framework id |
| `pitch_class_scan` | 3. Brute-force scan of all twelve tonics | the framework id |

### Chord claims

`chords` holds one claim per sonority segment, in time order. The segments come from Ursatz's
adaptive sonority segmentation, which cuts on the beats of the file's meter and where the bass
changes, and merges neighbours that sound the same pitch classes over the same bass. The chords come
from the library's weighted chord identification, read in the piece's key.

- `span` is the segment's window, from its start to its (exclusive) end.
- `note_ids` lists every note sounding in the segment, attacked in it or held over from earlier, and
  is never empty.
- `label` is the library's label for the chord (a Roman numeral for a triad, the seventh quality for
  a seventh chord), or `"no match"` when no chord was identified.
- `uncertainty` is the library's confidence in the best chord, in [0, 1]. Higher is more confident,
  despite the name (the name is shared with every claim).
- `key_label` is the key the chord was read in, or `"no match"` when no key was found.

Beyond the shared claim fields, a chord claim has the fields below. All but `duration` are `null`
when no chord was identified.

| Field | Type | Meaning |
| --- | --- | --- |
| `root` | string or null | The chord's root as a pitch class name, e.g. `"G"`. Flats are used in keys that read in flats. |
| `quality` | string or null | `"major"`, `"minor"` or `"diminished"` for a triad; the library's seventh quality (`"dominant7"`, `"major7"`, `"minor7"`, `"half-diminished7"`) for a seventh chord. |
| `bass` | string or null | The lowest sounding note of the segment, as a pitch class name. |
| `inversion` | integer or null | Where the bass sits among the chord's tones: `0` root position, `1` first inversion (the third in the bass), `2` second, `3` third (sevenths only). `null` when the bass is not a chord tone. |
| `duration` | object | `{"whole_notes", "beats"}`: the span's length as a rational in whole notes, and in beats of the signature the segment starts in (`null` when that signature has no beat grid). Where the chord starts and ends in measures and beats is `location`. |
| `score` | number or null | The library's score for the chord. Scores run from below 0 to 1.05. |
| `runner_up` | object or null | The best-scoring candidate on a different root, with `label`, `root`, `quality`, `score` and `uncertainty` (the library's plausibility of it relative to the best chord, in [0, 1]); `null` when no other root sounds. |

The runner-up is always recorded when the library provides one. The Markdown report shows it only
when its score is within 0.10 of the best chord's.

## Versioning

- `schema_version` changes only for breaking changes: removing or renaming a field, changing a
  field's type or meaning, removing a section, or making a field required that an earlier version
  did not require.
- Additive changes keep the version: new optional fields, new fields inside a claim, new `method`
  values, or filling a placeholder section.
- Consumers should ignore fields they do not recognize.

### Version history

- **3**: Chord claims gain required `root`, `quality`, `bass`, `inversion`, `duration`, `score` and
  `runner_up`, and are cut by the library's sonority segmentation, so `span` and `note_ids` cover the
  segment's window and every note sounding in it. A required field is a breaking change under the
  rule above.
- **2**: `meter` is filled (it was `null`). Every claim has a required `location`. v1 documents had
  `"meter": null` and no `location`.
- **1**: First version.
