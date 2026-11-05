# Rhythm and texture claims

Claims about rhythm, meter and texture in the analysis document. Back to the [docs index](../README.md) and the [analysis document](../analysis-document.md).

## Rhythm and meter

`rhythm` is one flat array of claims, each labelled by the kind of result it is: the meter fit first, then the densities
(phrase by phrase), then the syncopations, hemiolas, hypermeters and tuplets, each kind in time order. A flat list keeps
the placeholder's type (an array) from version 9; `label` says which of the six a claim is. The analyzer builds the neutral input of
Ursatz's standard-meter adapter (`analysis/meter_analysis_input.h`) in `src/pipeline/rhythm_pipeline.c`, and copies the library's results; every count, level, share and verdict is the
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

## Texture

`texture` is one flat array of claims: the passages in time order (`label` `"passage"`), then the changes between
consecutive passages (`label` `"change"`). The analyzer builds the neutral input of Ursatz's standard-texture adapter
(`analysis/texture_analysis_input.h`) in `src/pipeline/texture_pipeline.c`, and copies the library's results; every texture, category, feature and confidence is
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
