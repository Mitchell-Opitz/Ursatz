# Harmonic structure claims

Claims about outer voices, cadences, prolongations, progressions and harmonic rhythm in the analysis document. Back to the [docs index](../README.md) and the [analysis document](../analysis-document.md).

## Outer voices

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

## Cadences

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

## Prolongations

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

## Progressions

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

## Harmonic rhythm

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
