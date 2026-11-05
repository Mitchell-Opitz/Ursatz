# Chord claims

Claims about notes, chords, the chord vocabulary and non-chord tones in the analysis document. Back to the [docs index](../README.md) and the [analysis document](../analysis-document.md).

## Notes

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

## Chord claims

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

## Chord vocabulary

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

## Non-chord-tone claims

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
