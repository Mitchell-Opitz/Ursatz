# Analysis document

`ursatz-analyzer` writes `<stem>.analysis.json` next to each input file (see the README for the
`--json` and `--markdown` flags). It is the structured, versioned output that the Composer, the
GUI and corpus statistics read. The JSON Schema is
[`schema/analysis-document.v1.schema.json`](../schema/analysis-document.v1.schema.json)
(draft 2020-12). The library renders it with `analysis_document_render` from
`<ursatz_analyzer/analysis_document.h>`.

The output is pretty-printed with a 2-space indent, in the key order shown below, and ends with a
newline. The same input and build always give the same bytes.

## Top level

| Key | Type | v1 content |
| --- | --- | --- |
| `schema_version` | integer | Always `1`. |
| `metadata` | object | Filled. See below. |
| `meter` | object or null | Placeholder: `null`. |
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
Singular sections (`meter`, `form`) are `null` until filled; the rest are empty arrays.

## metadata

| Field | Type | Meaning |
| --- | --- | --- |
| `source_file` | string | Input file name without its directory. |
| `analyzer_version` | string | Ursatz-Analyzer version, from the CMake project version. |
| `ursatz_commit` | string | Full commit hash of the `external/ursatz` submodule at configure time, or `"unknown"` when `git` could not read it. |
| `note_count` | integer | Number of imported notes. |
| `ticks_per_quarter` | integer or null | The MIDI header's ticks per quarter note, or `null` when unavailable. |
| `key_found` | boolean | Whether a key was found. |

## Claims

Every entry in `keys` and `chords` is a claim. A claim mirrors the library's Interpretation.

| Field | Type | Meaning |
| --- | --- | --- |
| `framework` | string | Id of the source that made the claim, e.g. `"common-practice-minimal-v1"`. |
| `span` | object | `{"start": {"num", "den"}, "end": {"num", "den"}}`: rational times in whole notes from the start of the piece, in lowest terms, matching `time_numerator`/`time_denominator` in `AnalysisReport`. |
| `note_ids` | array of integers | The notes the claim covers, as indices into the imported notes. Index `N` is the note Ursatz names `midi-note-N`. |
| `label` | string | The claim's label. |
| `uncertainty` | number or null | The Interpretation's scalar confidence in [0, 1], or `null` when the library supplies none (unknown, or a candidate set at v1). |

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

`chords` holds one claim per group of notes that start together, in time order.

- `span` runs from the group's onset to the latest end of any note in the group. The library's own
  sonority window is zero-length (it starts and ends at the onset), so the document reports the
  real sounding extent instead.
- `note_ids` lists the notes in the group and is never empty.
- `label` is the Roman numeral, or `"no match"` when no chord was identified.
- `key_label` is the key the chord was read in, or `"no match"` when no key was found.

## Versioning

- `schema_version` changes only for breaking changes: removing or renaming a field, changing a
  field's type or meaning, or removing a section.
- Additive changes keep the version: new optional fields, new fields inside a claim, new `method`
  values, or filling a placeholder section.
- Consumers should ignore fields they do not recognize.
