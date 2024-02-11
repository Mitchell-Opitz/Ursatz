# Analysis document

`ursatz-analyzer` writes `<stem>.analysis.json` next to each input file (see the README for the
`--json` and `--markdown` flags). It is the structured, versioned output that the Composer, the
GUI and corpus statistics read. The JSON Schema is
[`schema/analysis-document.v7.schema.json`](../schema/analysis-document.v7.schema.json)
(draft 2020-12). The library renders it with `analysis_document_render` from
`<ursatz_analyzer/analysis_document.h>`.

The output is pretty-printed with a 2-space indent, in the key order shown below, and ends with a
newline. The same input and build always give the same bytes.

## Top level

| Key | Type | v7 content |
| --- | --- | --- |
| `schema_version` | integer | Always `7`. |
| `metadata` | object | Filled. See below. |
| `meter` | meter claim | Filled. See below. |
| `keys` | object | Filled: the key regions, the home key and the opening and closing regions. See below. |
| `tonicizations` | array of tonicization claims | Filled. See below. |
| `modulations` | array of modulation claims | Filled. See below. |
| `pivot_chords` | array of pivot chord claims | Filled. See below. |
| `chords` | array of chord claims | Filled. |
| `non_chord_tones` | array of non-chord-tone claims | Filled. |
| `outer_voices` | object | Filled: the soprano and bass lines. See below. |
| `cadences` | array | Placeholder: `[]`. |
| `phrases` | array | Placeholder: `[]`. |
| `form` | object or null | Placeholder: `null`. |
| `motives` | array | Placeholder: `[]`. |
| `melody` | array | Placeholder: `[]`. |
| `rhythm` | array | Placeholder: `[]`. |
| `texture` | array | Placeholder: `[]`. |
| `voice_leading` | array | Placeholder: `[]`. |
| `schemas` | array | Placeholder: `[]`. |
| `notes` | array of notes | Filled: where the notated and detected keys disagree. Empty when they do not, or there is no notated key signature. |

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
| `tempo` | object | `declared`, `quarters_per_minute` (`null` when not declared) and `marking_count`. Carried, not analyzed. |
| `dynamics_count` | integer | Dynamics markings the input carries. Carried, not analyzed. |
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
| `framework` | string | `"midi-time-signature"` when the file declares a time signature; `"ursatz-default-meter"` when it declares none. |
| `span`, `note_ids` | | As for every claim: the extent of all the notes. |
| `label` | string | The time signature, e.g. `"3/4"`. |
| `uncertainty` | null | Always `null`: the library supplies no confidence for a declared or defaulted meter. Use `source` to tell them apart. |
| `source` | `"file"`, `"notated"` or `"inferred"` | `"file"` when a MIDI file declares at least one time signature; `"notated"` when a MusicXML score does. `"inferred"` when it declares none and the library's default 4/4 is used. An inferred meter is an assumption, not a fact about the piece. |
| `time_signature` | object | `{"numerator", "denominator"}` of the first full measure. |
| `measure_count` | integer | Measures that start before the last note ends, a pickup included (the library's count). `0` for a file with no notes. |
| `last_measure` | integer or null | The number of the last measure (see numbering below), or `null` when there are no measures. |
| `pickup` | object or null | `null` when the piece has no pickup. Otherwise `{"length", "beats"}`: `length` is a rational in whole notes; `beats` is the same length in beats of the signature the pickup is read in, a rational, or `null` when that signature has no beat grid. |
| `changes` | array | One entry per later time signature: `{"measure", "start", "time_signature", "inferred"}`. `measure` is the number of the first measure in the new signature; `start` is a rational time in whole notes. Empty when the signature never changes. |

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

No cadence is looked for, so "modulation" here means "a lasting new key region", not "a cadence
established it". The change back out of an excursion (`is_return`) carries the extent of the
excursion it closes. The library's extended tonicization (it needs a chord count) is not produced
today, and would be listed under `tonicizations`.

`tonicizations` holds one claim per tonicization, in time order. The return out of a tonicization is part
of it and is not listed again. Beyond the claim fields:

| Field | Type | Meaning |
| --- | --- | --- |
| `span`, `location`, `note_ids` | | The excursion: the stretch in the target key. |
| `label` | string | `"tonicization of G major"`. |
| `uncertainty` | null | The library supplies none. |
| `extent` | string | `"tonicization"` (or `"extended-tonicization"`). |
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
| `extent` | `"weak-modulation"` or `"modulation"` | The library's extent. |
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
| `quality` | string or null | `"major"`, `"minor"` or `"diminished"` for a triad; the library's seventh quality (`"dominant7"`, `"major7"`, `"minor7"`, `"half-diminished7"`) for a seventh chord. |
| `bass` | string or null | The lowest sounding note of the segment, as a pitch class name. |
| `inversion` | integer or null | Where the bass sits among the chord's tones: `0` root position, `1` first inversion (the third in the bass), `2` second, `3` third (sevenths only). `null` when the bass is not a chord tone. |
| `duration` | object | `{"whole_notes", "beats"}`: the span's length as a rational in whole notes, and in beats of the signature the segment starts in (`null` when that signature has no beat grid). Where the chord starts and ends in measures and beats is `location`. |
| `score` | number or null | The library's score for the chord. Scores run from below 0 to 1.05. |
| `outer` | object | `{"soprano", "bass"}`: the highest and lowest sounding note of the segment, each `{"status", "note_id", "pitch", "degree", "uncertainty"}`. `status` is `"present"` when a note sounds as the line, else `"absent"` (only other voices sound), `"bridged"` (a short gap inside the line) or `"guarded"` (dropped by the library's register guard). `note_id` and `pitch` (a name with octave, e.g. `"E5"`, spelled with flats in keys that read in flats) are `null` unless present. `degree` is the library's scale degree label in the local key of the chord's key region (`"1"` to `"7"`, with `b` or `#` for a chromatic alteration), `null` unless present and the segment lies in a key region. `uncertainty` is that key region's uncertainty, in [0, 1], higher LESS certain; `null` when `degree` is. The chord's `bass` field above is a different thing: it is the pitch class the chord is read over. |
| `roman_numeral` | object | The chord's Roman numeral in its local key. See below. |
| `runner_up` | object or null | The best-scoring candidate on a different root, with `label`, `root`, `quality`, `score` and `uncertainty` (the library's plausibility of it relative to the best chord, in [0, 1]); `null` when no other root sounds. |

`roman_numeral` is the numeral Ursatz's standard-tonality Roman numeral adapter reads for the chord in the key of
the region the segment starts in, so it is relative to the local key and not the home key. The label, its
inversion figure and its quality marks are the library's, unaltered; the analyzer only passes the library the
identified chord, the region's key and the segment's pitch-class weights.

| Field | Type | Meaning |
| --- | --- | --- |
| `label` | string or null | The library's label with its figure: case for quality (`"I"`, `"ii"`), the figures `6`, `6/4` for triads and `7`, `6/5`, `4/3`, `4/2` for sevenths (`"V6/5"`), and the library's quality marks (`"vii°"`, `"ii∅7"`). A chord the library reads as an applied chord carries its slash form (`"V7/V"`). `null` when the library gives no numeral. |
| `key_region` | integer or null | Index into `keys.regions` of the region the numeral is relative to; `null` when the segment lies in no key region. |
| `key` | string or null | That region's label, e.g. `"G major"`. |
| `degree` | integer or null | The chord's root as a degree (1 to 7) of the local key (for an applied chord, of the tonicized key); `null` when unlabelled. |
| `uncertainty` | number or null | The chord's and the key region's uncertainty combined, `1 - (1 - chord uncertainty) * (1 - key uncertainty)`, in [0, 1]; higher is LESS certain. `null` when unlabelled. |
| `unlabelled_reason` | string or null | Why the chord has no numeral; `null` when it has one. A chord is never dropped or guessed at: it stays in `chords` with `label` `null`. The reasons are: no key region; the region is minor (the library's weighted chord vocabulary is the major-key set, so it identifies no chord there); the library identified no chord; and the library gives no numeral for the chord in its key (a chord that is not a degree of the key, or a quality the key does not give that degree). |

The runner-up is always recorded when the library provides one. The Markdown report shows it only
when its score is within 0.10 of the best chord's.

### Outer voices

`outer_voices` holds the two outer lines of the piece, from Ursatz's outer-voice extraction:
`{"soprano": [...], "bass": [...]}`. Each line has one entry for every chord, in time order, so the
entry at index `i` of either line belongs to the chord at index `i` of `chords` (`chord_index` says so
too). A line is not a melody: it is the highest (soprano) or lowest (bass) sounding note of each
chord's span, theory-neutral pitch height with the scale degree read against the local key. It is the
shared input for voice-leading (A6.1), melody (A5.1, whose `melody` section stays reserved) and schema
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

## Versioning

- `schema_version` changes only for breaking changes: removing or renaming a field, changing a
  field's type or meaning, removing a section, or making a field required that an earlier version
  did not require.
- Additive changes keep the version: new optional fields, new fields inside a claim, new `method`
  values, or filling a placeholder section.
- Consumers should ignore fields they do not recognize.

### Version history

- **7**: Chord claims gain the required `roman_numeral` object (the library's Roman numeral in the local key,
  the key region it is relative to, and the reason when there is none). A required field is a breaking change
  under the rule above.
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
