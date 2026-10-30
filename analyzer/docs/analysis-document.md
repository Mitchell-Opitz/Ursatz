# Analysis document

`ursatz-analyzer` writes `<stem>.analysis.json` next to each input file (see the README for the
`--json` and `--markdown` flags). It is the structured, versioned output that the Composer, the
GUI and corpus statistics read. The JSON Schema is
[`schema/analysis-document.v9.schema.json`](../schema/analysis-document.v9.schema.json)
(draft 2020-12). The library renders it with `analysis_document_render` from
`<ursatz_analyzer/analysis_document.h>`.

The output is pretty-printed with a 2-space indent, in the key order shown below, and ends with a
newline. The same input and build always give the same bytes.

## Top level

| Key | Type | v9 content |
| --- | --- | --- |
| `schema_version` | integer | Always `9`. |
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

### Keys

`keys` is an object, not a claim:

| Field | Type | Meaning |
| --- | --- | --- |
| `regions` | array of key regions | The local keys, in time order, gap-free. Empty when no key was found. |
| `home_key` | object or null | The piece's home key, derived from the regions; `null` with no regions. |
| `opening_region` | integer or null | Index into `regions` of the first region. |
| `closing_region` | integer or null | Index into `regions` of the last region. |

The regions come from Ursatz's key-region detection (`key_regions_estimate`, in the
`common-practice-minimal` framework): windowed correlation of the weighted pitch classes against the
24 major and natural-minor keys, a path through the windows that charges for each key change, and
regions merged from neighbouring measures with the same key. The analyzer adds no key logic of its
own. A key signature the file notates is passed to the library as a boundary: the region that starts
there is restricted to the signature's major key and its relative minor.

A key region is a claim, plus:

| Field | Type | Meaning |
| --- | --- | --- |
| `framework` | string | The framework id, e.g. `"common-practice-minimal-v1"`. |
| `span`, `location` | | The region, barline to barline. `span.end` is exclusive. |
| `note_ids` | array of integers | The notes that start in the region. |
| `label` | string | The key, e.g. `"C major"` or `"Eb major"`. Flats are used when the tonic needs fewer flats than sharps (minimal-accidental spelling); the one tie, F#/Gb, reads in sharps. Every region label is spelled this way. |
| `uncertainty` | number | The library's uncertainty for the region, in [0, 1]. Higher is LESS certain: 0 when the chosen key beats every other by a full unit, 1 when another key fits as well or better. This is the opposite sense from a chord claim's `uncertainty`, which carries the library's confidence (see Chord claims). |
| `index` | integer | The region's position in `regions`. |
| `tonic` | string | The tonic's pitch class name, e.g. `"Eb"`. |
| `mode` | `"major"` or `"minor"` | The mode. Minor is the profile's minor (raised sixth and seventh weigh in); chord identification treats it as natural minor. |
| `measures` | object | `{"first", "last"}`: the measure numbers the region covers, inclusive. |
| `source` | `"profile"` or `"key_signature"` | `"key_signature"` when the region begins at a notated key signature and was restricted to its two keys; `"profile"` otherwise. |

`home_key` is `{"rule", "label", "region_indices", "duration"}`. The library offers no determination of
its own, so the rule is `"longest_total_duration"`: the key (tonic and mode) with the greatest total time over
all the regions that are in it, a tie going to the key that appears first. `duration` is that total, a rational
in whole notes, and `region_indices` lists those regions in time order. Totalling over a key's regions, not
taking the single longest region, keeps a key the detection splits into several regions from being outweighed
by one long stretch of another. The rule is recorded so a consumer can tell which applied should another be
added. The home key is not necessarily the opening or the closing key.

### Tonicizations, modulations and pivot chords

The library classes the change between each pair of neighbouring key regions, and the analyzer reports
what it establishes without adding a rule of its own. It reads the extent from the length of the region
the change goes INTO and whether the key then returns:

- shorter than four measures and followed by a return to the previous key: a tonicization;
- longer, and followed by a return: a weak modulation;
- not followed by a return: a modulation.

That is the extent from region length alone. The analyzer then confirms it with the cadences found
(R-044, `harmony_key_change_confirm`): a change into a new key is a modulation only when a cadence arrives in
the new region. A cadence inside a region the length rule calls a tonicization makes it a weak modulation; no
cadence inside makes any change a tonicization, however long. `extent` in the claims is the confirmed extent,
and `cadence_confirmed` says a cadence confirmed it. The change back out of an excursion (`is_return`)
carries the extent of the excursion it closes. The library's extended tonicization (it needs a chord count) is
not produced today, and would be listed under `tonicizations`.

`tonicizations` holds one claim per tonicization, in time order. The return out of a tonicization is part
of it and is not listed again. Beyond the claim fields:

| Field | Type | Meaning |
| --- | --- | --- |
| `span`, `location`, `note_ids` | | The excursion: the stretch in the target key. |
| `label` | string | `"tonicization of G major"`. |
| `uncertainty` | null | The library supplies none. |
| `extent` | string | `"tonicization"` (or `"extended-tonicization"`), as confirmed by the cadences. |
| `cadence_confirmed` | boolean | Optional. Always `false` here: no cadence arrives in the target region. |
| `from_region`, `target_region` | integer | Indices into `keys.regions`: the key it leaves and the key it tonicizes. |
| `from_key`, `to_key` | string | Those regions' labels. |

`modulations` holds one claim per change the library gives a modulation extent, in time order, each a
lasting new key (a weak modulation goes right back to the home key). A return from an excursion that
was itself a modulation is listed, with `is_return` true; the return out of a tonicization is not.

| Field | Type | Meaning |
| --- | --- | --- |
| `span`, `location`, `note_ids` | | The passage in the new key. `location.start` is where it arrives. |
| `label` | string | `"C major to G major"`. |
| `uncertainty` | null | The library supplies none. |
| `extent` | `"weak-modulation"` or `"modulation"` | The library's extent, as confirmed by the cadences. |
| `cadence_confirmed` | boolean | Optional. `true` when a cadence arrives in the new region (a return carries the excursion's value). |
| `from_region`, `to_region` | integer | Indices into `keys.regions`. |
| `from_key`, `to_key` | string | Those regions' labels. |
| `type` | `"pivot"` or `"direct"` | The library's classification: `"pivot"` when it found a pivot chord, `"direct"` when it found none. It does not classify further (common-tone modulation, for one, is not distinguished). |
| `is_return` | boolean | The change goes back to the key before an excursion. |
| `pivot_chord_index` | integer or null | Index into `pivot_chords`; `null` for a direct modulation. |

`pivot_chords` holds one claim per pivot chord, in time order: the last segment before the new key
arrives, within two measures, whose chord is diatonic in both keys. The library's collection for a
minor key is its relative major's, so a chord needing the raised sixth or seventh does not count.
The chord is identified by the library's weighted identification in the OLD key (a minor old key is read in
its relative major, as the library does) and named without a Roman numeral.

| Field | Type | Meaning |
| --- | --- | --- |
| `span`, `location`, `note_ids` | | The chord's segment. |
| `label` | string | The root and quality, e.g. `"D minor"`. |
| `uncertainty` | number or null | The library's confidence in the chord, as for a chord claim. |
| `modulation_index` | integer | Index into `modulations`. |
| `chord_index` | integer | Index into `chords` of the same segment. That entry is read in the key of the region the segment starts in, which can differ from the old key if the pivot is the segment that starts at the boundary. |
| `root`, `quality`, `bass` | string | As for a chord claim. |
| `inversion` | integer or null | As for a chord claim. |
| `from_key`, `to_key` | string | The two keys the chord belongs to. |

### Notes

`notes` lists observations about the analysis that no claim carries. The only kind is
`"key_signature_disagreement"`: a notated key signature that the detected keys do not fit. Neither side
is preferred and nothing is overwritten: both are reported. A note has:

| Field | Type | Meaning |
| --- | --- | --- |
| `kind` | string | `"key_signature_disagreement"`. |
| `scope` | `"region_at_signature"` or `"home_key"` | What was compared. `"region_at_signature"`: the region in effect where a signature starts is not that signature's major key or relative minor. `"home_key"`: the home key is not the major key or relative minor of any notated signature. Regions inside a signature are not compared, since a piece modulates inside one signature all the time. A signature repeated unchanged is not compared again. |
| `signature_index` | integer or null | Index into `metadata.key_signatures`; `null` for `"home_key"`. |
| `region_index` | integer | The region concerned. |
| `notated` | string or null | The two keys the signature allows, e.g. `"Eb major or C minor"`; `null` for `"home_key"`. |
| `detected` | string | The region's label. |
| `message` | string | The note in words, as the report shows it. |

### Chord claims

`chords` holds one claim per sonority segment, in time order. The segments come from Ursatz's
adaptive sonority segmentation, which cuts on the beats of the file's meter and where the bass
changes, and merges neighbours that sound the same pitch classes over the same bass. The chords come
from the library's weighted chord identification, each segment read in its key region's key.

- `span` is the segment's window, from its start to its (exclusive) end.
- `note_ids` lists every note sounding in the segment, attacked in it or held over from earlier, and
  is never empty.
- `label` is the library's label for the chord (a Roman numeral for a triad, the seventh quality for
  a seventh chord), or `"no match"` when no chord was identified.
- `uncertainty` is the library's confidence in the best chord, in [0, 1]. Higher is more confident,
  despite the name (the name is shared with every claim).
- `key_label` is the key of the region the chord's segment starts in, the key it was read in, or `"no match"`
  when no key was found.

Beyond the shared claim fields, a chord claim has the fields below. All but `duration` are `null`
when no chord was identified.

| Field | Type | Meaning |
| --- | --- | --- |
| `root` | string or null | The chord's root as a pitch class name, e.g. `"G"`. Flats are used in keys that read in flats. |
| `quality` | string or null | `"major"`, `"minor"`, `"diminished"` or `"augmented"` (minor regions only) for a triad; the library's seventh quality (`"dominant7"`, `"major7"`, `"minor7"`, `"half-diminished7"`) for a seventh chord. |
| `bass` | string or null | The lowest sounding note of the segment, as a pitch class name. |
| `inversion` | integer or null | Where the bass sits among the chord's tones: `0` root position, `1` first inversion (the third in the bass), `2` second, `3` third (sevenths only). `null` when the bass is not a chord tone. |
| `duration` | object | `{"whole_notes", "beats"}`: the span's length as a rational in whole notes, and in beats of the signature the segment starts in (`null` when that signature has no beat grid). Where the chord starts and ends in measures and beats is `location`. |
| `score` | number or null | The library's score for the chord. Scores run from below 0 to 1.05. |
| `outer` | object | `{"soprano", "bass"}`: the highest and lowest sounding note of the segment, each `{"status", "note_id", "pitch", "degree", "uncertainty"}`. `status` is `"present"` when a note sounds as the line, else `"absent"` (only other voices sound), `"bridged"` (a short gap inside the line) or `"guarded"` (dropped by the library's register guard). `note_id` and `pitch` (a name with octave, e.g. `"E5"`, spelled with flats in keys that read in flats) are `null` unless present. `degree` is the library's scale degree label in the local key of the chord's key region (`"1"` to `"7"`, with `b` or `#` for a chromatic alteration), `null` unless present and the segment lies in a key region. `uncertainty` is that key region's uncertainty, in [0, 1], higher LESS certain; `null` when `degree` is. The chord's `bass` field above is a different thing: it is the pitch class the chord is read over. |
| `roman_numeral` | object | The chord's Roman numeral in its local key. See below. |
| `runner_up` | object or null | The best-scoring candidate on a different root, with `label`, `root`, `quality`, `score` and `uncertainty` (the library's plausibility of it relative to the best chord, in [0, 1]); `null` when no other root sounds. |
| `context` | string or null | Optional. Where the library took the reading from: `"none"` (the segment itself), `"previous"` or `"next"` (a sparse segment read as its neighbour's chord), or `"arpeggio"` (a run of incomplete segments inside one beat read as one chord). `null` when no chord was identified. |

`function`, `six_four` and `phrase_model` are optional; a chord the library gives none has no such member.

| Field | Type | Meaning |
| --- | --- | --- |
| `function` | object | The library's harmonic function of the chord (`analysis/analysis_function.h`): `name` (`"tonic"`, `"predominant"` or `"dominant"`), `applied_to` (the target degree 1 to 7 of an applied chord, which is a dominant toward it; `null` otherwise), `predominant` (`"strong"` for IV and ii, `"weak"` for iii and vi, the library's `harmony_predominant_strength`; `null` unless the chord is a diatonic predominant), `framework` (`"standard-harmony-v1"`) and `uncertainty` (the chord's, in [0, 1], higher is LESS certain). Absent when the library gives no function. |
| `six_four` | object | `{"type"}`: the library's six-four type for a triad in second inversion, `"cadential"`, `"passing"`, `"neighbor"` or `"arpeggiating"`. Absent for every other chord and for a six-four the library leaves unclassified. |
| `phrase_model` | string | The library's phrase model label of the chord (`harmony_analysis_phrase_model_label`), as it abbreviates it: `"Tb"` (the tonic that starts the phrase), `"PD"` (the first strong predominant before the cadential dominant), `"D"` (the cadential dominant) or `"Te"` (the tonic of an authentic cadence). Absent for a chord with no label: outside a phrase the library labels, or not a chord of the model. |

`roman_numeral` is the numeral Ursatz's standard-tonality Roman numeral adapter reads for the chord in the key of
the region the segment starts in, so it is relative to the local key and not the home key. The label, its
inversion figure and its quality marks are the library's, unaltered; the analyzer only passes the library the
identified chord, the region's key and the segment's pitch-class weights.

| Field | Type | Meaning |
| --- | --- | --- |
| `label` | string or null | The library's label with its figure: case for quality (`"I"`, `"ii"`), the figures `6`, `6/4` for triads and `7`, `6/5`, `4/3`, `4/2` for sevenths (`"V6/5"`), and the library's quality marks (`"vii°"`, `"ii∅7"`). A chord the library reads as an applied chord carries its slash form (`"V7/V"`). `null` when the library gives no numeral. |
| `base_numeral` | string or null | The same numeral in root position, formatted by the library with no inversion figure: `"V7"` for `"V6/5"`, `"I"` for `"I6/4"`, `"V7/V"` for `"V4/3/V"`. `null` when unlabelled. |
| `key_region` | integer or null | Index into `keys.regions` of the region the numeral is relative to; `null` when the segment lies in no key region. |
| `key` | string or null | That region's label, e.g. `"G major"`. |
| `degree` | integer or null | The chord's root as a degree (1 to 7) of the local key (for an applied chord, of the tonicized key); `null` when unlabelled. |
| `uncertainty` | number or null | The chord's and the key region's uncertainty combined, `1 - (1 - chord uncertainty) * (1 - key uncertainty)`, in [0, 1]; higher is LESS certain. `null` when unlabelled. |
| `applied` | object or null | `null` unless the library reads the chord as an applied (secondary) chord. See below. |
| `unlabelled_reason` | string or null | Why the chord has no numeral; `null` when it has one. A chord is never dropped or guessed at: it stays in `chords` with `label` `null`. The reasons are: no key region; the library identified no chord; and the library gives no numeral for the chord in its key (a chord that is not a degree of the key, or a quality the key does not give that degree). |

An applied chord is the library's own reading (its applied-chord re-check: the chord's evidence outside the key,
its score against the diatonic chord, and its resolution); the analyzer adds no rule for what counts as one. `label`
is then the whole label (`"V6/5/V"`), and `applied` holds its parts:

| Field | Type | Meaning |
| --- | --- | --- |
| `numeral` | string | The chord as it reads in the tonicized key, with its figure: `"V"`, `"V7"`, `"V6/5"`, `"vii°"`. |
| `target_numeral` | string | The chord tonicized, as a numeral of the local key: `"V"`, `"ii"`. |
| `target_degree` | integer | The degree (1 to 7) of the local key that is tonicized. |
| `target_tonic` | string or null | The pitch class name of that degree in the local key (the tonic of the key tonicized). |
| `kind` | string | `"secondary-dominant"` or `"secondary-leading-tone"`, the library's names. |
| `resolves_to` | integer | Index into `chords` of the chord the library finds it resolving to. |
| `deceptive` | boolean | The library finds it resolving to the target's submediant rather than the target. |
| `tonicization_agreement` | string | The cross-reference to `tonicizations`: `"agrees"`, `"target_differs"` or `"no_tonicization"`. |
| `tonicization_index` | integer or null | Index into `tonicizations` of the claim cross-referenced; `null` for `"no_tonicization"`. |
| `target_region` | integer or null | Index into `keys.regions` of that claim's target region; `null` for `"no_tonicization"`. |
| `tonicization_key` | string or null | That region's label. |

The cross-reference looks at the tonicization claims whose span overlaps the chord's span or begins where it ends
(the chord that leads into a tonicized key). A claim whose target key has the tonic `target_tonic` names makes it
`"agrees"`; if only claims with another tonic touch the chord it is `"target_differs"`, and the claim it reports is
the one that touches; if none touches it is `"no_tonicization"`. A disagreement is reported, never resolved: the
chord keeps its applied reading and the tonicization claims are unchanged. The only scale arithmetic involved is
locating the tonic a degree of the local key names (major scale; natural minor in a minor key).

The runner-up is always recorded when the library provides one. The Markdown report shows it only
when its score is within 0.10 of the best chord's.

### Chord vocabulary

`chord_vocabulary` lists the Roman numerals of the piece, for the whole piece and for each key region. Only
chords with a numeral are counted; the ones without stay in `chords` with their reason. Each entry:

| Field | Type | Meaning |
| --- | --- | --- |
| `scope` | string | `"overall"` (the whole piece) or `"region"` (one key region). |
| `key_region`, `key` | integer or null, string or null | For a region scope, the index into `keys.regions` and its label; `null` for `"overall"`. |
| `group` | string | `"diatonic"`, `"applied"` (the chord's `roman_numeral.applied` is not null) or `"mixture"` (borrowed chords). A chord is in exactly one group. The library's Roman numeral adapter does not name borrowed chords yet, so nothing is placed in `"mixture"` today; a chord from outside the key has no numeral and is counted as unlabelled. |
| `level` | string | `"numeral"`: the numeral as the library names it, inversion figure included (`"V6/5"`, `"I6/4"`). `"base_numeral"`: the same chords grouped by `base_numeral` (`"V7"`), so every inversion of a chord counts together. |
| `numeral` | string | The numeral or base numeral. |
| `count` | integer | The number of chord claims with it: the counting rule below. |
| `runs` | integer | The number of runs of it: adjacent chord claims with the identical numeral in one key region counted once. |
| `duration` | object | `{"whole_notes", "beats"}`: their total length, as in a chord claim; `beats` is `null` if any of them has none. |
| `share` | number | `duration` over the total length of all numbered chords in the scope, in [0, 1]. Chords without a numeral are not in the total. |
| `first` | object | `{"chord_index", "position"}`: the earliest chord claim with it (index into `chords`) and where it starts (measure, beat, subdivision, as `location.start`). |

Counting rule. Every labelled chord claim counts once, whatever its length, so the `count`s of one scope, level and
group-partition add up to the number of chords with a numeral in that scope. A chord claim is a sonority segment,
so a chord held across a beat or re-struck over a moving bass is several claims; `runs` is the count with such
repeats merged: a claim continues a run only when the claim immediately before it (no unlabelled claim between)
has the same `numeral` and lies in the same key region. A key change therefore always starts a new run, and the
per-region `runs` add up to the overall `runs`.

Entries are ordered overall first, then by region; within a scope by level (`numeral`, then `base_numeral`), then by
group (`diatonic`, `applied`, `mixture`); and within those by `count`, then `duration`, longest first, then `first`,
earliest first. A scope, level or group with no chords has no entries.

### Outer voices

`outer_voices` holds the two outer lines of the piece, from Ursatz's outer-voice extraction:
`{"soprano": [...], "bass": [...]}`. Each line has one entry for every chord, in time order, so the
entry at index `i` of either line belongs to the chord at index `i` of `chords` (`chord_index` says so
too). A line is not a melody: it is the highest (soprano) or lowest (bass) sounding note at the start
of each chord's span (the library extracts the lines over segments that are not merged over unchanged
harmony, one per melody note, and each chord takes the entry at its start), theory-neutral pitch height with the scale degree read against the local key. It is the
shared input for voice-leading (A6.1), melody (A5.1, whose `melody` section is read from these lines) and schema
(A6.2) analysis, and for the Composer's schema recognition. Nothing in it is a voice-leading, melodic
or schema claim.

Each entry is a claim (see Claims) over the chord's span, plus:

| Field | Type | Meaning |
| --- | --- | --- |
| `chord_index` | integer | Index into `chords` of the chord this entry belongs to. |
| `status` | string | `"present"` when a note sounds as the line in the chord's span; otherwise `"absent"` (only other voices sound), `"bridged"` (a short gap inside the line) or `"guarded"` (the library dropped the note rather than adopt an accompaniment note). A line is never filled from another voice. |
| `key_label` | string | The key of the region the chord lies in, the key the degree is read in, or `"no match"`. |
| `note_id` | integer or null | The imported note index, as `note_ids` in every claim; `null` unless present. `note_ids` holds the same note, or is empty. |
| `pitch` | object or null | `{"name", "midi"}`: the pitch name with octave (`"E5"`, spelled with flats in keys that read in flats) and the MIDI note number. `null` unless present. |
| `degree` | string or null | The library's scale degree label in the local key: `"1"` to `"7"`, with `b` or `#` for a chromatic alteration. `null` unless present and the chord lies in a key region. In a minor region the diatonic degrees are the natural minor's. |
| `held` | boolean or null | Whether the note was attacked before the chord began. `null` unless present. |
| `onset` | object or null | `{"time", "location"}`: where the note itself is attacked, as a rational in whole notes and as a measure and beat. `null` unless present. |
| `duration` | object or null | `{"whole_notes", "beats"}`: the note's whole length, not just its part in the chord, in whole notes and in beats of the signature it is attacked in (`null` when that signature has no beat grid). `null` unless present. |

In a claim, `label` is the degree, failing that the pitch name, failing that the status. `uncertainty`
is the uncertainty of the key region the degree is read in, in [0, 1], higher LESS certain, as for
`keys` (note this is the other way round from a chord's `uncertainty`); `null` when `degree` is.

One note can appear in consecutive entries: a note held across several chords is the line's note in
each. Where one note sounds, the soprano and the bass are that note. Where voices cross, the soprano is
whatever sounds highest and the bass whatever sounds lowest, whichever voice plays them. A chord also
carries the same summary in its `outer` field (see Chord claims).

### Non-chord-tone claims

`non_chord_tones` holds one claim for every note the library finds sounding in a chord's segment
without being one of the chord's tones, in time order, then note order. A note held across several
segments is listed once for each segment in which it is non-chordal. Segments with no chord have no
non-chord tones.

- `span` and `location` are the chord segment the note is non-chordal against.
- `note_ids` holds the one note.
- `label` is the figure's type, or `"unclassified"`.

| Field | Type | Meaning |
| --- | --- | --- |
| `type` | string or null | The library's name for the figure, as `non_chord_tone_name` gives it: `"passing"`, `"neighbor"`, `"double-neighbor"`, `"changing-tone"`, `"suspension"`, `"anticipation"`, `"appoggiatura"`, `"escape-tone"`, `"retardation"` or `"pedal"`. `null` when the library left the tone unclassified. The library recognizes only some of these today. |
| `classified` | boolean | `false` for an unclassified tone. Unclassified tones are listed, never dropped. |
| `chord_index` | integer | Index into `chords` of the chord the note is non-chordal against. |
| `chord_label` | string | That chord's `label`. |

### Cadences

`cadences` holds one claim per cadence the library's standard-harmony analysis finds
(`analysis/analysis_cadence.h`), in time order. The analyzer builds the library's neutral inputs from the chain
(each chord segment's Roman numeral degree, quality, inversion, applied target, mode and uncertainty, the soprano
and bass scale degrees, the meter) and reports what comes back: types, arrivals, dominants and beat strengths are
the library's. After the cadences are found, a dominant that ends the run before a section start becomes a half
cadence of the earlier section (R-045, R-054; a section starts at each key region that returns to the home key
after leaving it), and the key changes are confirmed (above). The cadence plan is this array in order; the
Markdown report renders it from here.

| Field | Type | Meaning |
| --- | --- | --- |
| `framework` | string | `"standard-harmony-v1"`, the library's framework name. |
| `span`, `location`, `note_ids` | | From the start of the dominant to the end of the arrival; the notes sounding in those segments. |
| `label` | string | The type. |
| `uncertainty` | number | The library's, from the chords involved (higher is less certain). |
| `type` | string | The library's cadence type name: `"perfect-authentic"`, `"imperfect-authentic"`, `"half"`, `"subverted"`, `"plagal"`, `"deceptive"`, `"authentic-unspecified"`. |
| `arrival` | object | `span` and `location` of the arrival (the chord segments from its first to its last). |
| `dominant` | object | `span` of the dominant (with its cadential 6/4, when it has one). |
| `soprano_degree` | string or null | The outer voice's scale degree label at the arrival, in the arrival's key region; `null` when the soprano is absent. |
| `arrival_beat_strength` | string | The library's beat strength of the arrival: `"strong"`, `"medium"` or `"weak"`. |
| `boundary` | boolean | A half cadence the boundary step added before a section start. |
| `region` | integer or null | Index into `keys.regions` of the arrival's key region; `null` outside every region. |
| `share` | integer | Thousandths of the evidence for this reading, 0 to 1000. 1000 when the cadence has no `alternative`. |
| `alternative` | object | Optional; present only when the library reports a competing reading (below). |

A cadence has a competing reading in two places (R-090, R-091). A root-position V or V7 followed by a I6 at a
cadence point is an evaded cadence (`type` `"subverted"`) or a prolongation of the dominant. A dominant before a
section start is a half cadence or the authentic cadence into the tonic that starts the next section. The claim
describes the likelier reading and `alternative` the other; the two `share` values sum to 1000. For an evaded
cadence the claim stays `"subverted"` even when the prolongation is the likelier reading (`share` below 500).

| `alternative` field | Type | Meaning |
| --- | --- | --- |
| `reading` | string | `"cadence"` or `"prolongation"`. |
| `type` | string or null | The cadence type when `reading` is `"cadence"`; `null` for a prolongation. |
| `measure`, `beat` | integer or null | Where the alternative's arrival begins; `null` when the meter cannot place it. |
| `share` | integer | Thousandths of the evidence for the alternative. |
| `note_ids` | array of integer | The notes sounding in the alternative's arrival. |

The Markdown report lists each cadence with an alternative under "Competing readings", the claim first, with
percentages: `m23 b1 half cadence (60%), or perfect authentic cadence at m24 b1 (40%)` and
`m4 b1 evaded cadence (55%), or tonic prolongation (45%)`. The report calls a `"subverted"` cadence "evaded"
everywhere it names a cadence type; the document keeps the library's word.

### Prolongations

`prolongations` holds one claim per prolongation the library finds (`analysis/analysis_prolongation.h`), in order
of the prolonged chord: a tonic or dominant that lasts through the chords between it and the harmony that closes
it. The analyzer runs `harmony_analysis_assign_functions`, `harmony_analysis_mark_cadential`,
`harmony_analysis_classify_six_fours` and `harmony_analysis_find_prolongations` over the same prepared segments
the cadences use; the functions, types and reasons are the library's. The section is optional and additive (no
version bump), and empty when the library finds none or declines the piece.

| Field | Type | Meaning |
| --- | --- | --- |
| `framework` | string | `"standard-harmony-v1"`. |
| `span`, `location`, `note_ids` | | From the start of the prolonged chord to the end of the first segment of the closing chord; the notes sounding in those segments. |
| `label` | string | The function. |
| `uncertainty` | number | The least certain function among the chords in the span (higher is less certain). |
| `function` | string | The prolonged harmony: `"tonic"` or `"dominant"`. |
| `numeral` | string or null | The prolonged chord's Roman numeral; `null` when the library gave it none. |
| `first_chord`, `last_chord` | integer | Indices into `chords` of the prolonged chord and the first chord of the closing harmony. |
| `reasons` | array of strings | One library reason name per chord between them: `"passing-six-four"`, `"neighbor-six-four"`, `"arpeggiating-six-four"`, `"tonic-line"` or `"applied"`. |
| `region` | integer or null | Index into `keys.regions` of the prolonged chord's key region; `null` outside every region. |


### Phrases

`phrases` holds one claim per phrase the library's standard-form analysis finds (`analysis/analysis_phrase.h`,
`analysis_ideas.h`), in time order. The analyzer builds the library's neutral input from the chain (the outer-voice
soprano as notes, with consecutive segments of one note merged, the chord segments, the meter and the cadences of the
cadence step, each with its arrival span) and injects the kernel's Phrase Boundary Detection as the boundary detector
(`src/pipeline/form_pipeline.c`, `form_boundary.c`, ported from Ursatz's test glue). Phrases, ideas and subphrases
are the library's; the analyzer only reads them out. A phrase runs from the measure after the one before it to the end
of its cadence's arrival chord, snapped to the next beat, so the phrases tile the measures of the analysed range and
their spans may leave the rest of a phrase's last measure out.

The ends of all but the last phrase are the boundaries of the second key pass (R-039): the chain runs once, reads the
phrases, and runs again with those ends as phrase boundaries and its chords as evidence for the key regions, so
everything in the document comes from the second pass. There is no further iteration.

| Field | Type | Meaning |
| --- | --- | --- |
| `framework` | string | `"standard-form-v1"`, the library's framework name. |
| `span`, `location`, `note_ids` | | The phrase's span, an extension after its cadence included; the notes sounding in the chords that start in it. |
| `label` | string | `"phrase"`. |
| `uncertainty` | number | The library's: the cadence's, 0.25 for a boundary made by a break in the melody, 0.5 for a final phrase with neither (higher is less certain). |
| `index` | integer | The phrase's position, from 0. |
| `measures` | object | `{"first", "last"}`: the library's measure numbers (a pickup is 0) from the phrase's first measure to the measure its cadence's arrival chord ends in; the phrase proper. |
| `length` | integer | The phrase proper in measures, a pickup not counted. |
| `extension_measures` | integer | The measures a post-cadential extension or codetta adds after `measures.last`; 0 with none. |
| `cadence` | integer or null | Index into `cadences` of the cadence that ends the phrase; `null` for a phrase no cadence ended. |
| `signal` | string | What ended the phrase: `"cadence"`, `"discontinuity"` (a break in the melody after the longest run without a cadence) or `"none"` (the end of the input). |
| `role` | string | `"beginning"` for the first phrase, `"ending"` for the last, `"middle"` between. The library names no role for a phrase, so this is position alone (a single phrase is a `"beginning"`). |
| `phrase_model` | array | The chords the library labelled for the phrase model, in order: `{"chord": index into chords, "label": "Tb" \| "PD" \| "D" \| "Te"}`. Empty when the library labels none: a phrase with no cadence, or one its rules cannot label. A phrase spans the chords that start in it up to its cadence's arrival; an extension takes no label. |
| `subphrases` | array | Optional. A phrase of at least three ideas has two: its first two ideas and the rest. Each is a claim (`label` `"subphrase"`) with `index` (0 or 1), `first_idea` and `idea_count` (into this phrase's `ideas`), `measures` and `length`. |
| `ideas` | array | Optional. The phrase's ideas, in order, cut by the library's nominal two-measure length. Each is a claim whose `label` is the library's idea label (`"basic-idea"`, `"contrasting-idea"`, `"fragment"` or `"unit"`) with `index`, `measures`, `length` and `similarity` (the share of matching intervals against the phrase's first idea; `null` for the first idea). Spans are clipped to the phrase's span. |

| `expansions` | array | Optional, absent with none. The expansions and contractions that concern the phrase (see "Themes"): each is a claim whose `label` is `"internal"`, `"external"` or `"contraction"` (the library's `ExpansionPlacement` and `PhraseLengthChange` names; its `StandardFormExpansionKind` has none of its own) with `index`, `change` (`"expansion"` or `"contraction"`), `theme` (index into `themes`, `null` for an external expansion of a phrase in no theme), `expected_length` and `actual_length` in measures, `measures_changed` (at least 1), `measures`, and `techniques` (the library's `"repetition"`, `"stretching"`, `"one-more-time"` and `"alternative-path"` it finds evidence for; the library detects only repetition and one-more-time, and none when it finds no evidence). An external one adds `external_kind` (`"post-cadential-extension"`, `"codetta"`, `"coda"` or `"prefix"`) and the booleans `suffix_length_typical` and `suffix_agrees_with_location`. |

`subphrases`, `ideas`, `expansions` and the chord claims' `phrase_model` are additive. The Markdown report renders
`## Phrases` from this array: a row per phrase (with an Expansion column), then a line per phrase with its ideas,
subphrases and phrase model.

### Themes

`themes` holds one claim per theme the library's standard-form analysis finds (`analysis/analysis_theme.h`) over the
phrases, ideas and subphrases above, in the library's order: a compound period comes after its two sentences, so a
theme may start before the one before it. The analyzer runs `standard_form_identify_themes` and
`standard_form_detect_expansions` with the default config (`src/pipeline/form_pipeline.c`); every form, part name and
judgement is the library's. The form's bass is not part of the input: the library reads the bass only for motives,
and the analyzer finds motives in the soprano only (see Motives), so no bass notes are added and the cadential ending
is never identified (README, Known limits).

| Field | Type | Meaning |
| --- | --- | --- |
| `framework` | string | `"standard-form-v1"`. |
| `span`, `location`, `note_ids` | | From the first measure to the end of the last phrase proper (an extension after its cadence is not part of the theme); the soprano notes starting in it. |
| `label` | string | The library's form name: `"sentence"`, `"period"`, `"hybrid"`, `"compound-period"` or `"repeated-phrase"`. |
| `uncertainty` | number | The library's: the greatest phrase uncertainty combined with the uncertainty of the similarity that decided the form. |
| `index` | integer | The theme's position in this array, from 0. |
| `beginning`, `ending` | string or null | The library's names for the two parts (`"presentation"`, `"compound-basic-idea"`, `"antecedent"`; `"continuation"`, `"consequent"`, `"cadential"`); `null` for a repeated phrase and a compound period. |
| `phrases` | object | `{"first", "count"}`: the phrases it spans (index into `phrases`, 1 or 2). |
| `measures`, `length` | | As for a phrase: the library's measure numbers from the first measure to the last of the last phrase proper, and the length in measures. |
| `parts_are_phrases` | boolean | The two parts are phrases (a two-phrase theme) and not subphrases (a one-phrase theme). |
| `commonality` | string or null | How common the pairing of the two parts is: `"very-common"`, `"slightly-less-common"`, `"occasional"` or `"rare"`; `null` when the library gives none. |
| `part_of_compound` | boolean | A sentence that is a half of a compound period. |
| `parts` | array | The two parts, in order: `{"name", "measures", "length", "cadence"}`. `name` is `beginning` or `ending`; `cadence` is the index into `cadences` of the cadence that ends the part, `null` for a presentation or compound basic idea and for the first half of a theme whose first phrase has no cadence. |
| `traits` | array | What the library's claim finds in the continuation: `"fragmentation"` or none. The claim gives no acceleration or sequence, so the analyzer reports none. |
| `archetypal`, `cadences_usual`, `lengths_usual` | boolean | The claim's own judgements: a sentence of 4 or 8 measures with a balanced continuation that has fragmentation; the usual cadences (a period's half cadence then perfect authentic cadence; a sentence's goal cadence); the usual lengths. |
| `halves` | array | Optional, compound periods only: the indices into `themes` of its two sentences. |

The expansion and contraction claims are in the phrase they concern, in `expansions` (see "Phrases"): an ending longer
or shorter than its beginning is an internal expansion or a contraction by the measures of the difference, and the
extension after a phrase's cadence is an external expansion. The Markdown report renders `## Themes` from this array (a
row per theme with its parts, each with the cadence that ends it, and its traits) and lists the compound periods.

### Form plan

`form` is the plan of the piece: its sections, the phrases, subphrases and ideas inside them, and the thematic returns
between its themes. The sections are the kernel's form structure detection (`analysis/form_structure_detection.h`)
over one phrase occurrence per phrase with its cadence type, and the returns are the kernel's thematic return
detection (`analysis/thematic_return_detection.h`, default config) over the soprano notes of each theme, both in
`src/pipeline/form_plan_pipeline.c`. The analyzer adds only bookkeeping: the phrases after the last closing cadence,
which the kernel leaves out, become a last section; returns between themes that share a phrase are dropped; and the
section letters, which follow from the returns alone. The sections are reported only and do not feed back into the
cadences (R-054).

The tree's nodes carry a `level` field with the exact names `form`, `section`, `phrase`, `subphrase` and `idea`, the
levels of the Composer's plan, so the Composer can load it (R-019). A phrase, subphrase or idea node points at its full
claim in `phrases` and repeats only what reading the outline needs; each idea appears once in the tree, in its
subphrase or, for a phrase of fewer than three ideas (which has no subphrases), in the phrase.

| Field | Type | Meaning |
| --- | --- | --- |
| `level` | `"form"` | |
| `measures` | object | `{"first", "last"}`: from the first section's first measure to the last section's last. |
| `sections` | array | The sections in time order, at least one (see below). |
| `returns` | array | The thematic returns (see below); empty when the library found none. |

A section is a claim (`framework` `"standard-form-v1"`, the framework of the phrases it groups):

| Field | Type | Meaning |
| --- | --- | --- |
| `span`, `location`, `note_ids` | | From the first phrase's start to the last phrase's end; the notes sounding in the chords that start in it. |
| `label` | string | The letter: `"A"`, `"B"`; a section whose opening theme returns an earlier section's material takes that section's letter with one prime more than the sections of that letter already have (`"A'"`, `"A''"`). |
| `uncertainty` | number | The greatest uncertainty of its phrases. |
| `level` | `"section"` | |
| `index` | integer | The section's position in `sections`, from 0. |
| `measures` | object | `{"first", "last"}`: the library's measure numbers, the last including the last phrase's extension. |
| `key` | string or null | The label of the key region the section's first chord lies in (not the key it ends in); `null` outside every region. |
| `region` | integer or null | The index of that region in `keys.regions`. |
| `closed` | boolean | A section-closing cadence (perfect or imperfect authentic) of the library's ended it; `false` only for the last section when phrases follow the last such cadence or there is none. |
| `cadence` | integer or null | The index into `cadences` of the cadence that ends its last phrase. |
| `returns_from` | integer or null | The index into `sections` of the earlier section whose material its opening theme returns; `null` for a section with a new letter. |
| `return` | integer or null | The index into the form's `returns` of the return that set the letter. |
| `phrases` | array | The phrase nodes, at least one. |

A phrase node is `{"level": "phrase", "phrase", "measures", "length", "cadence", "subphrases", "ideas"}`: `phrase` is
the index into `phrases`, `measures`, `length` and `cadence` are that claim's, `subphrases` are nodes
`{"level": "subphrase", "phrase", "index", "measures", "ideas"}` and `ideas` are nodes
`{"level": "idea", "phrase", "index", "label", "measures"}` (the library's `IdeaLabel` names), `index` being the
position within the phrase.

A return is a claim over the later theme, labelled `"thematic-return"`, in the order of the later theme and then the
earlier:

| Field | Type | Meaning |
| --- | --- | --- |
| `span`, `location`, `note_ids` | | The later theme's. |
| `uncertainty` | number | 1 minus the similarity. |
| `earlier_theme`, `later_theme` | integer | Indices into `themes`; the earlier theme's phrases come before the later's. |
| `earlier_section`, `later_section` | integer | Indices into the form's `sections`: the section holding each theme's first phrase. |
| `earlier`, `later` | object | `{"first", "last"}`: the measures of the two themes. |
| `similarity` | object | The kernel's evidence for the best stretch pair: `value` (the lower of `contour`, the share of equal up, down and same directions, and `rhythm`, the share of equal durations; at least 0.8), `window` (the notes compared), and `earlier_offset` and `later_offset` (where the stretches begin inside each theme, in notes). |

The Markdown report renders `## Form plan` from this object: a row per section (letter, measures, key, phrases, closing
cadence, the return that lettered it), the returns, and the tree as an indented outline.

### Motives

`motives` holds one claim per motive family the library's standard-form motive analysis finds
(`analysis/analysis_motive.h`), in the library's order: by prototype, longer first. The analyzer runs
`standard_form_find_motives` with the default config (3 to 8 notes, one mismatch for a varied motive, soprano only) in
`src/pipeline/motive_pipeline.c`, ported from Ursatz's `tests/integration/motive_util.h`, `form_kernel_bridge.h` and
`test_motive_mozart545.c`. A motive is a short figure that recurs; its occurrences, their kinds and their scores are the
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

### Melody

`melody` holds one claim per phrase that has soprano notes, in phrase order: the melody of that phrase, read by the
melody adapter of Ursatz's standard-counterpoint (`analysis/analysis_melody.h` and the files beside it). The analyzer
builds the adapter's neutral line in `src/pipeline/melody_pipeline.c`, ported from Ursatz's
`tests/integration/melody_util.h` and `test_melody_bach114.c`, and groups the results; every interval, direction,
climax and verdict is the library's. The line of a phrase is the outer voice soprano (the highest sounding line, one
entry per voice segment) for the notes whose first segment starts inside the phrase's span: consecutive segments with
the same note id are one note, a note held in from the previous phrase is left out, and a gap in the line is a rest that
breaks the chain, so no interval is formed across it. Each note carries its scale degree in the key region it starts in
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

### Rhythm and meter

`rhythm` is one flat array of claims, each labelled by the kind of result it is: the meter fit first, then the densities
(phrase by phrase), then the syncopations, hemiolas, hypermeters and tuplets, each kind in time order. A flat list keeps
the placeholder's type (an array) from version 9; `label` says which of the six a claim is. The analyzer builds the neutral input of
Ursatz's standard-meter adapter (`analysis/meter_analysis_input.h`) in `src/pipeline/rhythm_pipeline.c`, ported from
`tests/integration/rhythm_util.h`, and copies the library's results; every count, level, share and verdict is the
library's. The input holds every imported note (its onset to its release; `line` 0 when it is a note of the outer voice
soprano, 1 for the bass, 2 for any other), a chord change at the start of every chord segment whose base numeral or key
region differs from the segment before (the harmonic rhythm's definition) and the phrases of `phrases` with the
arrival of their cadence and the length change of their expansion. The form adapter does not report elided cadences, so
no phrase is elided and the library reports no hypermetric reinterpretation. A piece without chord segments has no
`meter-fit` claim, and one without phrases has no other. A claim's `framework` is `"standard-meter-v1"`.

**Meter fit** (`label` `"meter-fit"`): the meter checked against the harmonic rhythm and the cadences
(`src/pipeline/meter_check_pipeline.c`). The analyzer's own result, not the library's: `note_ids` is empty and `span` is the
piece. Candidates are every meter of 2/4, 3/4, 4/4, 3/8, 6/8, 9/8 and 12/8 at the scales 1/2, 1 and 2 with a pickup of 0 to
one beat less than the measure; their barlines are placed in the file's time. A candidate with the same barlines and beats
as another is one (3/4 at scale 2 and 3/8 at scale 1); the reading shown is the one whose most common interval between
onsets, scaled, is nearest an eighth note, then the one with denominator 4. A candidate whose measure is no longer than the
beat of the meter in use is not tried. A reading scores from 0 to 1 on the harmony changes (the downbeat share is the
harmonic mean of the share of changes on a downbeat and the share of downbeats with a change; the share on any beat counts
once) and the cadence arrivals on a downbeat (three times); the third beat of a four-beat meter counts half a downbeat.
A reading applied with `--meter`, `--value-scale` or `--pickup` is the one checked (`declared`). The check never applies the
alternative.

| Field | Type | Meaning |
| --- | --- | --- |
| `declared` | object | The reading the analysis used: `meter` (`numerator`, `denominator`), `scale` and `pickup` (rationals; the pickup in whole notes after scaling) and `score`. |
| `alternative` | object or null | The best other reading, same shape; `null` unless its score is at least 0.15 above `declared`'s. |
| `uncertainty` | number | 1 minus the gap between the two best scores. |

**Density** (`label` `"density"`): the library's profile of a phrase's notes. One claim of all the notes of each phrase
(`line` `"all"`) and one for each line that has a note in it (`"soprano"`, `"bass"`, `"other"`); `span` is the phrase's
and `note_ids` the notes counted, whose onset (snapped to the library's grid) is in the phrase. The recurring rhythm
patterns are the motive families' rhythms (`motives`), not a second detector.

| Field | Type | Meaning |
| --- | --- | --- |
| `uncertainty` | number | The library's: 0, the profile describes the notes as given. |
| `phrase` | integer | The phrase's index into `phrases`. |
| `measures` | object | `{"first", "last"}`: the phrase's measures, as in `phrases`. |
| `line` | string | `"all"`, `"soprano"`, `"bass"` or `"other"`. |
| `notes` | integer | The notes counted. |
| `attacks` | integer | The distinct onsets among them (a chord is one attack). |
| `measure_count` | integer | The measures the phrase touches. |
| `attacks_per_beat` | object | `{"mean", "max"}`: for each measure the phrase touches, the attacks inside the phrase's part of it over the beats that part spans (the beat of the measure's reading, a dotted quarter in 6/8 and 12/8); the mean over the measures and the largest. |
| `mean_inter_onset` | number | The mean distance between consecutive attacks, in whole notes; 0 with fewer than two attacks. |
| `on_beat_share`, `off_beat_share` | number | The shares of the attacks that fall exactly on a beat, and the rest. |
| `durations` | array | `[{"duration": {"num", "den"}, "count"}]`: the notes' lengths in whole notes, ascending, a chord's notes each counted. At most 16 are kept, the shortest first. |
| `durations_truncated` | boolean | Some longer durations were left out. |

**Syncopation** (`label` `"syncopation"`): the library's THROUGH_STRONGER rule. A note attacked at a weaker metric level
and held through a stronger one that no note of its line attacks. Each line is read on its own. `span` and `note_ids`
are the one note's. `line` is `"soprano"`, `"bass"` or `"other"`; `measure` and `beat` (1-based, fractional) are where it
is attacked, `attack_level` its level and `strength` the difference of the two levels (1 to 4: an off-beat note held over
the beat is 1, a note on a beat held over the barline 2, an off-beat note held over the barline 3). `held_through` is
`{"level", "measure", "beat"}`. The levels are the library's `StandardMeterLevel`: `"downbeat"`, `"secondary-beat"`,
`"beat"`, `"offbeat"` and `"finer"`. `uncertainty` is 0.1 when the library's snap moved the onset, otherwise 0.

**Hemiola** (`label` `"hemiola"`): six pulses of two measures of a simple triple meter, or one of 6/8, read as three duple
units. `measures` is `{"first", "last"}` and `note_ids` the notes attacked on the window's pulses. `evidence` is
`{"duration", "chord", "accent", "agreeing", "available"}`: each type is `true` when it agrees, `false` when it does not
and `null` when it is unavailable (the chord evidence, with no chord changes), and the claim is made when `agreeing`
reaches 2. `uncertainty` is the library's, 1 - `agreeing` / 3. Signatures with no hemiola (12/8, 4/4) have none.

**Hypermeter** (`label` `"hypermeter"`): one claim per phrase that has a measure; `span` and `note_ids` are the
phrase's. `phrase` is its index, `measures` its measures (the span's, an extension included), `length` the hypermeasure
length the library chose (2 or 4 measures: the one that fills the phrase and holds the cadence arrival in the last
hypermeasure), `score` its confidence and `arrival_measure` the measure the cadence arrives in (`null` with none).
`hyperdownbeats` lists the first measure of every hypermeasure, and `hypermeasures` has one object for each:
`{"index", "measures", "kind", "extra_hyperbeats", "contains_cadence", "score", "uncertainty"}`. `kind` is the library's
`"regular"`, `"short"` (a remainder), `"expanded"` (an internal expansion added hyperbeats), `"extension"` (an external
expansion is a hypermeasure of its own) or `"contracted"`. The hypermeasures follow one another from the phrase's first
measure; the library fills the last to its length, so it can run past `measures`. `uncertainty` of the claim is 1 -
`score`.

**Tuplet** (`label` `"tuplet"`): a run of two or more consecutive attacks of one ratio, found from the notes' times
alone (the importers carry no notated tuplets, R-056), so only a ratio that falls on a grid of 96 to the whole note is
read: MIDI times are rounded to that grid, and a time further than a quarter of a step from it is not read. A note whose
attack, or distance to the next attack, is not a whole step of the library's 1/32 grid is a triplet note (3:2, which
includes sextuplets); in a compound meter, two notes that halve a beat are a duplet (2:3). Other ratios are not looked
for. All the notes are one stream. The library names the ratio and places it (`borrowed_rules.h`). `uncertainty` is
`null`: the borrowed-division rules are rules, not a detector.

| Field | Type | Meaning |
| --- | --- | --- |
| `ratio` | object | `{"actual", "normal"}`: 3 and 2 for a triplet, 2 and 3 for a duplet. |
| `name` | string | `"triplet"`, `"duplet"` or `"tuplet"` (the library names the first two only). |
| `meter` | string | The meter of the first note: `"simple"`, `"compound"` or `"unclassified"`. |
| `borrowed` | boolean | The library's BORROWED rule: a triplet in a simple meter or a duplet in a compound one. |
| `level` | string or null | The library's LEVEL rule for the span of the first group: `"beat"`, `"subdivision"` or `"multi-beat"`; `null` when it cannot place it. |
| `level_typical` | boolean or null | The library's TYPICAL_LEVEL rule; `null` with `level`. |
| `measure`, `beat` | number | Where the first note is attacked. |
| `attacks` | integer | The attacks in the run, at least 2. |

The Markdown report renders `## Rhythm and meter` from this section: `### Density` (a row per phrase and line),
`### Syncopation`, `### Hemiola`, `### Hypermeter` and `### Tuplets and borrowed divisions`, after one `**Meter fit:**` line.

### Texture

`texture` is one flat array of claims: the passages in time order (`label` `"passage"`), then the changes between
consecutive passages (`label` `"change"`). The analyzer builds the neutral input of Ursatz's standard-texture adapter
(`analysis/texture_analysis_input.h`) in `src/pipeline/texture_pipeline.c`, ported from
`tests/integration/texture_util.h`, and copies the library's results; every texture, category, feature and confidence is
the library's. The input holds every imported note with its stream (`ImportedNotes.streams`: the MIDI track chunk, the
MusicXML part), the voice the kernel's voice separation gave it inside that stream (0 the top) and the separation's
confidence. The library measures every measure from the first that has a note to the last, classifies each with its
default configuration and merges consecutive measures of the same texture into passages. A claim's `framework` is
`"standard-texture-v1"`. A piece the separation or the library declines has an empty `texture`.

**Passage** (`label` `"passage"`): `span` runs from the start of the passage's first measure to the end of its last, and
`note_ids` are the notes attacked in it, in time order.

| Field | Type | Meaning |
| --- | --- | --- |
| `uncertainty` | number | The library's, in [0, 1]; higher is less certain. |
| `texture` | string | `"monophony"`, `"heterophony"`, `"homophony"`, `"homorhythm"`, `"melody and accompaniment"` or `"polyphony"`. |
| `category` | string | The main texture: `"monophony"`, `"heterophony"`, `"homophony"` or `"polyphony"`; homorhythm and melody and accompaniment are homophony. |
| `measures` | object | `{"first", "last"}`, as the library numbers them (a pickup is 0). |
| `windows` | integer | The measures in the passage. |
| `unclassified_windows` | integer | Measures whose features fit no texture and took their neighbour's. |
| `confidence` | object | `{"texture", "voices", "overall"}`: the texture's margin from its thresholds, the voice separation's, and both together. |
| `features` | object | The library's means over the passage's measures: `voices_max` (the largest), `voices_mean`, `synchrony`, `independence`, `prominence`, `top_gap`, `register_span`, `spacing`, `density` (attack times per beat) and `pattern`. |

`features.pattern` is the library's repeated-cell score, 0 to 1: evidence for an accompaniment figure. The library names
no accompaniment pattern (block, arpeggiated, Alberti, oom-pah are not categories or subtypes of standard-texture), so
the analyzer reports the score and names none (R-018).

**Change** (`label` `"change"`): the boundary between two consecutive passages; `span` is the passage changed to and
`note_ids` is empty.

| Field | Type | Meaning |
| --- | --- | --- |
| `uncertainty` | number | The library's. |
| `from`, `to` | integer | Indices of the two passages among the passage claims (`to` is `from + 1`). |
| `boundary_measure` | integer | The first measure of the passage changed to. |
| `from_texture`, `to_texture` | string | The two textures. |
| `category_changed` | boolean | False for homorhythm to melody and accompaniment, both homophony. |

### Voice leading

`voice_leading` is one flat array of claims, each labelled by the result it is: one `"motions"` claim, then the
`"violation"` claims in time order, the `"resolution"` claims, and the `"doubling"` claims. The analyzer builds the
voice-pitch grid of the adapters of Ursatz's standard-counterpoint and standard-harmony in
`src/pipeline/voice_leading_pipeline.c`, ported from `tests/integration/voice_leading_util.h`, and copies the library's
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

### Schemas and sequences

`schemas` is one flat array of claims, each labelled by the result it is: the `"schema"` claims in order of their first
stage, then the `"sequence"` claims in order of their first segment. The analyzer builds the neutral input of the
galant-schemas and scale-harmonization adapters in `src/pipeline/schema_pipeline.c`, ported from
`tests/integration/schema_util.h`, and copies the library's matches. A claim's `framework` is the library's own:
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

### Progressions

`progressions` lists the chord progressions that recur, counted over the library's Roman numerals (counting is not
theory, R-018). A progression is a run of 2 to 6 consecutive labelled chords inside one key region, each named by the
numeral's `base_numeral`. Consecutive chords with the same base numeral count once (a held chord), and a run does not
cross an unlabelled chord or a change of key region. Every run is counted, overlapping runs included, and a pattern
recurs when it occurs at least twice. A pattern that sits inside a longer pattern with the same number of occurrences
is dropped: it only ever occurs inside the longer one. Entries are ranked by `count` descending, then `length`
descending, then first occurrence. The section is empty when nothing recurs.

| Field | Type | Meaning |
| --- | --- | --- |
| `pattern` | array of strings | The base numerals, e.g. `["I", "IV", "V", "I"]`. |
| `label` | string | The pattern joined with `" > "`, the separator Ursatz's `corpus_stats_chord_progressions` uses: `"I > IV > V > I"`. |
| `length` | integer | 2 to 6. |
| `count` | integer | How often the pattern occurs: at least 2. |
| `occurrences` | array of occurrences | In time order; each is a claim. |

An occurrence has the shared claim fields and:

| Field | Type | Meaning |
| --- | --- | --- |
| `framework` | string | The chord claims' framework. |
| `span`, `location`, `note_ids` | | From the start of the run's first chord to the end of its last; the notes sounding in those chords. |
| `label` | string | The pattern's `label`. |
| `uncertainty` | number | The least certain Roman numeral among the run's chords (higher is less certain). |
| `region` | integer | Index into `keys.regions` of the key region all its chords lie in. |
| `chords` | array of integers | Indices into `chords` of every chord in the run, held repeats included. |

### Harmonic rhythm

`harmonic_rhythm` counts how often the harmony changes. A change is a chord segment whose base numeral or key region
differs from the segment before it (the first segment is a change; a segment with no numeral differs from one that has
one), counted in the measure the segment starts in. Measures are the library's (a pickup is 0).

| Field | Type | Meaning |
| --- | --- | --- |
| `per_measure` | array | `{ "measure", "changes" }` for every measure from the first chord's to the last chord's; a measure no chord starts in has 0. |
| `mean_per_measure` | rational | Changes per measure over those measures. |
| `per_window` | array | `{ "first_measure", "last_measure", "mean_per_measure" }` for each window of 4 measures from the first, the last window shorter when the measures run out. Optional. |
| `cadence_approaches` | array | One per cadence, in the order of `cadences`. See below. |

Each entry of `cadence_approaches` compares the harmonic rhythm in the two measures before a cadence arrives with the two
before those:

| Field | Type | Meaning |
| --- | --- | --- |
| `cadence` | integer | Index into `cadences`. |
| `arrival_measure` | integer | The measure the arrival begins in. Optional. |
| `approach` | rational or null | Mean changes per measure over the two measures before the arrival measure; `null` when the piece has none of them. |
| `before` | rational or null | The same over the two measures before those. |
| `accelerates` | boolean | `approach` is greater than `before`; `false` when either is `null`. |

### Markings

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

### Counterpoint check

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

## Versioning

- `schema_version` changes only for breaking changes: removing or renaming a field, changing a
  field's type or meaning, removing a section, or making a field required that an earlier version
  did not require.
- Additive changes keep the version: new optional fields, new fields inside a claim, new `method`
  values, or filling a placeholder section.
- Consumers should ignore fields they do not recognize.

### Version history

- **9**: Motive claims gain the optional `features` object and motive occurrences the optional `degrees` array, both
  additive. Adds the required placeholders `progressions` (`[]`), `harmonic_rhythm`, `markings` and
  `counterpoint_check` (`null`) after `schemas`, so the P3 sections fill them without another version. Required
  sections are a breaking change under the rule above. Chord claims gain the optional `context`, which did not
  need a version of its own. `cadences` is filled (its claims are new), and tonicization and modulation claims
  gain the optional `cadence_confirmed`; both are additive. Chord claims gain the optional `function` and
  `six_four`, and the optional top-level `prolongations` follows `chord_vocabulary`; all additive. `progressions` and `harmonic_rhythm` are filled (their
  entries are new), which is additive. `phrases` is filled (its claims are new, with the optional `subphrases`,
  `ideas` and `phrase_model`), and chord claims gain the optional `phrase_model`; all additive. The optional
  top-level `themes` follows `phrases`, and phrase claims gain the optional `expansions`; both are additive. `form` is
  filled (its object is new), which is additive. `motives` is filled (its claims are new), which is additive. `melody` is filled
  (its claims are new), which is additive. `rhythm` is filled (its claims are new, as one flat array labelled by kind),
  which is additive. `texture` is filled (its claims are new, as one flat array labelled by kind), which is additive.
  `voice_leading` is filled (its claims are new, as one flat array labelled by kind), which is additive. `schemas` is
  filled (its claims are new, as one flat array labelled by kind), which is additive. `markings` is filled (its object is
  new), which is additive.
- **8**: Adds the required `chord_vocabulary` section and the required `roman_numeral.base_numeral` field. A required
  section and a required field are breaking changes under the rule above.
- **7**: Chord claims gain the required `roman_numeral` object (the library's Roman numeral in the local key,
  the key region it is relative to, and the reason when there is none). A required field is a breaking change
  under the rule above. `roman_numeral` also gains the optional `applied` member for applied chords, which did not
  need a version of its own.
- **6**: `metadata` gains the required `input` object (format, source type, whether meter, key and spelling are notated, embedded or inferred, parts, tempo), and `meter.source` gains `"notated"`. A required field is a breaking change under the rule above.
- **5**: Adds the required `outer_voices` section (the soprano and bass lines) and the required `outer`
  field on every chord claim. A required section and a required field are breaking changes under the
  rule above.
- **4**: `keys` is an object of key regions with a derived home key (it was an array holding one
  whole-piece claim with a `method`), `metadata` gains the required `key_signatures`, and the top
  level gains the required `notes`. The `method` field is gone, with the key-signature, bass-fallback
  and pitch-class-scan steps it named. Changing a section's type and removing a field are breaking
  changes under the rule above.
- **3**: Adds the required `non_chord_tones` section. Chord claims gain required `root`, `quality`, `bass`, `inversion`, `duration`, `score` and
  `runner_up`, and are cut by the library's sonority segmentation, so `span` and `note_ids` cover the
  segment's window and every note sounding in it. A required field is a breaking change under the
  rule above.
- **2**: `meter` is filled (it was `null`). Every claim has a required `location`. v1 documents had
  `"meter": null` and no `location`.
- **1**: First version.
