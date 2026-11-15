# Analysis document

The structure of the analysis JSON document: top level, metadata, meter, sources and the claim index. Back to the [docs index](README.md).

`ursatz-analyzer` writes `<stem>.analysis.json` next to each input file (see the README for the
`--json` and `--markdown` flags). It is the structured, versioned output that the Composer, the
GUI and corpus statistics read. The JSON Schema is
[`schema/analysis-document.v11.schema.json`](../schema/analysis-document.v11.schema.json)
(draft 2020-12). The library renders it with `analysis_document_render_result` from
`<ursatz_analyzer/analysis_document.h>`. The document is an export of an `AnalyzerResult`
(see [analysis-result.md](analysis-result.md)); `analysis_document_render` over a notes and report pair gives the
same bytes.

The output is pretty-printed with a 2-space indent, in the key order shown below, and ends with a
newline. The same input and build always give the same bytes.

## Top level

| Key | Type | v11 content |
| --- | --- | --- |
| `schema_version` | integer | Always `11`. |
| `metadata` | object | Filled. See below. |
| `meter` | meter claim | Filled. See below. |
| `keys` | object | Filled: the key regions, the home key and the opening and closing regions. See below. |
| `tonicizations` | array of tonicization claims | Filled. See below. |
| `modulations` | array of modulation claims | Filled. See below. |
| `pivot_chords` | array of pivot chord claims | Filled. See below. |
| `chords` | array of chord claims | Filled. |
| `chord_vocabulary` | array of vocabulary entries | Filled: each distinct Roman numeral with its count and length. See below. |
| `prolongations` | array of prolongation claims | Optional, after `chord_vocabulary`. Filled: the library's tonic and dominant prolongations. See below. |
| `non_chord_tones` | array of non-chord-tone claims | Filled. |
| `outer_voices` | object | Filled: the soprano and bass lines. See below. |
| `cadences` | array of cadence claims | Filled: the library's standard-harmony cadences. See below. |
| `phrases` | array of phrase claims | Filled: the library's standard-form phrases, with their ideas, subphrases, phrase model and expansions. See below. |
| `themes` | array of theme claims | Optional, after `phrases`. Filled: the library's themes (sentences, periods, hybrids, compound periods, repeated phrases). See below. |
| `form` | form object or null | Filled: the tree of sections, phrases, subphrases and ideas, and the thematic returns. See below; `null` when the library found no phrases. |
| `repeats` | array of repeat claims | Filled, right after `form`: the library's repeats and returns, in its order. See below; empty when it found none. |
| `motives` | array of motive claims | Filled: the library's motive families with every occurrence and their development. See below. |
| `melody` | array of melody claims | Filled: the melody of each phrase, read by the library's melody adapter. See below. |
| `rhythm` | array of rhythm claims | Filled: the meter fit, then the library's density, syncopation, hemiola, hypermeter and tuplet results, one flat list labelled by kind. See below. |
| `texture` | array of texture claims | Filled: the library's texture passages and the changes between them, one flat list labelled by kind. See below. |
| `voice_leading` | array of voice leading claims | Filled: the library's motion, parallel and direct perfect intervals, unresolved tendency tones, and doubling and spacing, one flat list labelled by kind. See below. |
| `schemas` | array of schema and sequence claims | Filled: the library's galant schemas and harmonic sequences, one flat list labelled by kind. See below. |
| `progressions` | array of progression entries | Filled: the recurring progressions of two to six chords. See below. |
| `harmonic_rhythm` | object or null | Filled: changes of harmony per measure and the approach to cadences. See below; `null` when the piece has no chords the meter can place. |
| `markings` | markings object | Filled: the notated tempo and dynamics markings, read with the library's standard-markings rules. See below. |
| `counterpoint_check` | object or null | Filled by `--check-counterpoint`: two-voice input checked against a species. See below; `null` otherwise. |
| `notes` | array of notes | Filled: where the notated and detected keys disagree. Empty when they do not, or there is no notated key signature. |

Every key is always present, so consumers can read any section without checking for it first.
The singular section `counterpoint_check` is `null` unless `--check-counterpoint` ran; the rest are empty arrays.
`harmonic_rhythm` is `null` only for a piece with no chords the meter can place, and `form` only for a piece with no
phrases.

## metadata

| Field | Type | Meaning |
| --- | --- | --- |
| `source_file` | string | Input file name without its directory. |
| `analyzer_version` | string | Ursatz-Analyzer version, from the CMake project version. |
| `ursatz_commit` | string | Full commit hash of the `external/ursatz` submodule at configure time, or `"unknown"` when `git` could not read it. |
| `note_count` | integer | Number of imported notes. |
| `ticks_per_quarter` | integer or null | The MIDI header's ticks per quarter note, or `null` when unavailable. |
| `input` | object | The input format and what it states or leaves to inference. See below. |
| `key_found` | boolean | Whether any key region was found. |
| `key_signatures` | array | The key signatures the file notates, in time order; empty when it has none. See below. |

### Input

`metadata.input` says which input the analysis read and how much of what it reports the input
itself states.

| Field | Type | Meaning |
| --- | --- | --- |
| `format` | `"midi"` or `"musicxml"` | The input format that was read (see `--input-format` in the README). |
| `source_type` | `"performance"` or `"score"` | `"performance"` for MIDI (timed events, no notated spelling or barlines), `"score"` for MusicXML. |
| `meter` | `"notated"`, `"embedded"` or `"inferred"` | `"notated"`: the score's `<time>`. `"embedded"`: a MIDI time signature event. `"inferred"`: the input declares none and 4/4 is assumed. |
| `key` | `"notated"`, `"embedded"` or `"inferred"` | The same, for the key signature. `"inferred"` means the input has no key signature, so keys come from the notes alone. |
| `spelling` | `"notated"` or `"rule"` | `"notated"`: pitch names in chord roots, chord basses and outer voices follow the score. `"rule"`: spelled by the pipeline's minimal-accidental rule (MIDI). |
| `parts` | array | The score's parts, merged into one texture for the analysis; empty for MIDI. Each has `id`, `name`, `staff_count` and `note_count`. |
| `tempo` | object | `declared`, `quarters_per_minute` (`null` when not declared) and `marking_count`. The markings themselves are in `markings`. MusicXML input declares the tempo of its first metronome mark; MIDI input declares it from the first set-tempo event when that event is at tick 0 (a file whose first set-tempo comes later, or has none, leaves it undeclared). |
| `dynamics_count` | integer | Dynamics markings the input carries. The markings themselves are in `markings`. |
| `warning_count` | integer | Importer warnings (skipped grace notes, rounded values, and so on). |

### Notated key signatures

`metadata.key_signatures` is input metadata: what the file says, not what was detected. One entry
per MIDI key signature meta-event or MusicXML `<key>`.

| Field | Type | Meaning |
| --- | --- | --- |
| `start` | rational | Where the event sits, in whole notes. |
| `location` | position or null | The measure and beat of `start`. |
| `sharps_flats` | integer | The signature's count, -7 to 7; negative is flats. |
| `mode_hint` | `"major"`, `"minor"` or `"unknown"` | The file's major/minor byte. It is unreliable and never decides a key. |
| `major_key`, `minor_key` | string | The major key and the relative minor the signature allows, e.g. `"Eb major"` and `"C minor"`. |

## meter

`meter` is one claim (the shared claim fields below) for the whole piece. It is always present for
a successfully imported file, even one with no notes.

| Field | Type | Meaning |
| --- | --- | --- |
| `framework` | string | `"midi-time-signature"` when the file declares a time signature; `"musicxml-time-signature"` for a score's; `"ursatz-default-meter"` when it declares none; `"notation-reading-meter"` when a notation reading set the meter. |
| `span`, `note_ids` | | As for every claim: the extent of all the notes. |
| `label` | string | The time signature, e.g. `"3/4"`. |
| `uncertainty` | null | Always `null`: the library supplies no confidence for a declared or defaulted meter. Use `source` to tell them apart. |
| `source` | `"file"`, `"notated"`, `"inferred"` or `"override"` | `"file"` when a MIDI file declares at least one time signature; `"notated"` when a MusicXML score does. `"inferred"` when it declares none and the library's default 4/4 is used. An inferred meter is an assumption, not a fact about the piece. `"override"` when a notation reading (`--meter`) set the meter. |
| `time_signature` | object | `{"numerator", "denominator"}` of the first full measure. |
| `measure_count` | integer | Measures that start before the last note ends, a pickup included (the library's count). `0` for a file with no notes. |
| `last_measure` | integer or null | The number of the last measure (see numbering below), or `null` when there are no measures. |
| `pickup` | object or null | `null` when the piece has no pickup. Otherwise `{"length", "beats"}`: `length` is a rational in whole notes; `beats` is the same length in beats of the signature the pickup is read in, a rational, or `null` when that signature has no beat grid. |
| `changes` | array | One entry per later time signature: `{"measure", "start", "time_signature", "inferred"}`. `measure` is the number of the first measure in the new signature; `start` is a rational time in whole notes. Empty when the signature never changes. |
| `reading` | object or null | `null` unless a notation reading (`--meter`, `--value-scale`, `--pickup`; see the README) was applied. Otherwise `{"scale", "meter_overridden"}`: `scale` is the rational every start and duration was multiplied by, `meter_overridden` is true when the reading set the meter. Added to schema version 9 without a version bump. |

A pickup is detected by the library from what the file says (a partial first measure ended by a
later time signature, or a short leading signature such as 1/8 before 12/8), never from note
content.

## Sources of each section

The Analyzer runs the library's analysis chain (`analysis_chain_run` of `ursatz_chain`) once and collects its results.
Each section comes from one accessor, or from a library call over the chain's results where the chain runs none.

| Section | Source |
| --- | --- |
| `keys`, `tonicizations`, `modulations` | `analysis_chain_key_regions`; the home key is the region key with the longest total duration |
| `pivot_chords` | the weighted chord identification of the pivot segment in the old key (not a chain result) |
| `chords`, `non_chord_tones` | `analysis_chain_chords`, with the Roman numerals of `analysis_chain_numerals` |
| `outer_voices` | `analysis_chain_outer_voices` |
| `cadences` | `analysis_chain_cadences` |
| `prolongations`, harmonic functions, six-four types, the phrase model | standard-harmony over `chain_build_harmony_segments` (the chain runs none of them) |
| `phrases`, `themes` | `analysis_chain_phrases`, `_ideas`, `_themes`, `_expansions` |
| `form` | standard-form structure detection over the chain's themes (Analyzer-only) |
| `motives` | `analysis_chain_motives`; degrees are in the home key |
| `melody` | each phrase's part of `analysis_chain_melody`; degrees are in the home key |
| `rhythm` | standard-meter over the notes, chord changes and phrases (Analyzer-only) |
| `texture` | `analysis_chain_texture` |
| `voice_leading` | `analysis_chain_voice_leading` |
| `schemas` | `analysis_chain_schemas`, `analysis_chain_sequences` |
| `progressions`, `harmonic_rhythm`, `markings`, `counterpoint_check` | the collected report (Analyzer-only) |

## Claims

Every key region and every entry in `chords` and `non_chord_tones` is a claim. A claim mirrors the library's Interpretation.

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
- The `meter` claim covers the whole piece, so its positions run from the first note to the end of
  the last.
- A key region runs from a barline to a barline, so its positions are always on beat 1.

## Claim kinds

Each claim section lives in its own file.

| Claim kind (section) | File |
| --- | --- |
| Keys | [keys.md](analysis-document/keys.md#keys) |
| Tonicizations, modulations and pivot chords | [keys.md](analysis-document/keys.md#tonicizations-modulations-and-pivot-chords) |
| Notes | [chords.md](analysis-document/chords.md#notes) |
| Chord claims | [chords.md](analysis-document/chords.md#chord-claims) |
| Chord vocabulary | [chords.md](analysis-document/chords.md#chord-vocabulary) |
| Non-chord-tone claims | [chords.md](analysis-document/chords.md#non-chord-tone-claims) |
| Outer voices | [harmony-structure.md](analysis-document/harmony-structure.md#outer-voices) |
| Cadences | [harmony-structure.md](analysis-document/harmony-structure.md#cadences) |
| Prolongations | [harmony-structure.md](analysis-document/harmony-structure.md#prolongations) |
| Progressions | [harmony-structure.md](analysis-document/harmony-structure.md#progressions) |
| Harmonic rhythm | [harmony-structure.md](analysis-document/harmony-structure.md#harmonic-rhythm) |
| Phrases | [form.md](analysis-document/form.md#phrases) |
| Themes | [form.md](analysis-document/form.md#themes) |
| Form plan | [form.md](analysis-document/form.md#form-plan) |
| Repeats and returns | [form.md](analysis-document/form.md#repeats-and-returns) |
| Motives | [motive-and-melody.md](analysis-document/motive-and-melody.md#motives) |
| Melody | [motive-and-melody.md](analysis-document/motive-and-melody.md#melody) |
| Rhythm and meter | [rhythm-and-texture.md](analysis-document/rhythm-and-texture.md#rhythm-and-meter) |
| Texture | [rhythm-and-texture.md](analysis-document/rhythm-and-texture.md#texture) |
| Voice leading | [voice-leading.md](analysis-document/voice-leading.md#voice-leading) |
| Schemas and sequences | [voice-leading.md](analysis-document/voice-leading.md#schemas-and-sequences) |
| Counterpoint check | [voice-leading.md](analysis-document/voice-leading.md#counterpoint-check) |
| Markings | [voice-leading.md](analysis-document/voice-leading.md#markings) |
| Version history | [versions.md](analysis-document/versions.md) |

## Versioning

- `schema_version` changes only for breaking changes: removing or renaming a field, changing a
  field's type or meaning, removing a section, or making a field required that an earlier version
  did not require.
- Additive changes keep the version: new optional fields, new fields inside a claim, new `method`
  values, or filling a placeholder section.
- Consumers should ignore fields they do not recognize.

The history of every version is in [versions.md](analysis-document/versions.md).
