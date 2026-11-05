# Ursatz-Analyzer

A small C11 command-line tool that turns a MIDI or MusicXML file into a Markdown report and a JSON analysis
document: the piece's time signature, measure count and pickup, its local keys over time (key
regions, with the home key, tonicizations, modulations and pivot chords), and the most likely chord
for each group of notes that start together, each located by measure and beat. It is a
thin consumer of the [Ursatz](https://github.com/Mitchell-Opitz/Ursatz) library and its
`common-practice-minimal` framework.

## Build

Requires CMake 3.20+, a C11 compiler, and SQLite3 (for Ursatz). Ursatz is a git submodule at
`external/ursatz`, pinned to Ursatz main at `d2b85ce`. cJSON 1.7.18 is vendored at `external/cjson`; it is added only when no
parent project already provides a `cjson` target.

```sh
git submodule update --init --recursive
cmake -S . -B build
cmake --build build
ctest --test-dir build --output-on-failure
```

Build details, the test list and the CI checks are in [docs/building.md](docs/building.md).

## Usage

```sh
ursatz-analyzer [--json] [--markdown] [--input-format midi|musicxml] [--check-counterpoint <1-5> [--cantus upper|lower]] [--meter <n/d>] [--value-scale <n or n/d>] [--pickup <n/d>] <input-file-path>
```

Writes two files next to the input file:

- `<stem>.report.md`: the Markdown report.
- `<stem>.analysis.json`: the analysis document. See
  [docs/analysis-document.md](docs/analysis-document.md) and the
  [schema](schema/analysis-document.v9.schema.json).

`--markdown` writes only the report, `--json` writes only the analysis document. No flag, or both,
writes both. `--input-format` forces the input format (see "Input formats"). Flags can come before or
after the path. Exits 1 with a message on an unknown flag, a
missing path, bad input, allocation failure, or write failure.

`--check-counterpoint <1-5>` also checks two-voice input against that species of Ursatz's
standard-counterpoint (see `counterpoint_check` in
[docs/analysis-document.md](docs/analysis-document.md)). `--cantus upper|lower` says which voice is
the cantus firmus (default lower) and needs `--check-counterpoint`. The two voices are the input's
two streams (MIDI tracks or MusicXML parts), or the two voices the kernel finds in a single stream.
For any other input the rest of the analysis is written and the CLI exits 2 with "counterpoint
check needs two voices".

`--meter <n/d>`, `--value-scale <n or n/d>` and `--pickup <n/d>` read the notes as other notation
than the file's before they are analyzed (see "Notation readings"). They apply to one input file,
not to corpus, style or similar-material runs.

```sh
ursatz-analyzer --corpus <dir> [--out <dir>] [--resume] [--markdown]
```

Corpus mode analyzes every MIDI and MusicXML file under a folder and writes one analysis document each, plus a
corpus index. See [docs/corpus.md](docs/corpus.md) and the [index schema](schema/corpus-index.v1.schema.json).
It cannot be combined with an input file, `--json`, `--check-counterpoint`, `--cantus`, `--input-format`, `--meter`, `--value-scale` or `--pickup`.

```sh
ursatz-analyzer --style-profile <corpus output dir> --name <style name> [--out <file>]
ursatz-analyzer --compare-styles <a.json> <b.json> [--without <category>]... [--format text|json]
```

`--style-profile` counts the harmony, rhythm, melody and form labels of a corpus run's documents and writes them as a
style model file, with four categories for motive degrees, intervals, rhythms and non-chord-tone figures;
`--compare-styles` gives the distance between two such files, leaving out the categories named by `--without`. See
[docs/style-profile.md](docs/style-profile.md) and the [model schema](schema/style-model.v1.schema.json).

```sh
ursatz-analyzer --similar <corpus output dir> (--motive <file> | --progression "<numerals>") [--min <n/d>] [--max <n>] [--format text|json]
```

`--similar` finds passages of a corpus run's documents that are similar to a motive (the soprano of a MIDI or MusicXML
file) or a progression (Roman numerals). See [docs/similar-material.md](docs/similar-material.md) and the
[output schema](schema/similar-material.v1.schema.json).

```sh
ursatz-analyzer --link-motives <corpus output dir> [--cache <file>] [--depth <1-6>] [--candidates <n>] [--format text|json]
```

`--link-motives` finds the motive families of a corpus run's documents that are variants of one another (a chain of
motive relations from one to the other), groups them, and keeps what it compared in a cache so a later run compares only
what is new. See [docs/motive-links.md](docs/motive-links.md) and the [output schema](schema/motive-links.v1.schema.json).

## Chords and non-chord tones

The analyzer adds no music theory of its own here: it renders what Ursatz's segmentation
(`sonority_builder_build_adaptive`) and weighted identification
(`common_practice_identify_weighted_sequence`) establish.

- **Segments.** A segment is a stretch of the piece with the same sounding notes over the same bass:
  the piece is cut at every beat of the meter and where the bass changes, and neighbours that sound
  the same pitch classes over the same bass are merged (not across a barline), as is a melody or passing
  note over unchanged harmony (new attacks above the bass while the chord is still held). A segment holds every
  note sounding in it, attacked or held over. A chord's `note_ids` are those notes.
- **Each chord** has a root, quality, bass note and inversion (0 root position, 1 first, 2 second,
  3 third), a duration in beats and the measure and beat where it starts and ends, and the library's
  confidence. The chord's `label` is the library's (a Roman numeral for a triad, the seventh quality
  for a seventh chord). Roots are spelled with flats in keys that read in flats.
- **Context.** A segment too sparse to read alone (no match, or fewer than three of its chord's tones
  sounding) takes a neighbouring segment's chord when every pitch class it sounds belongs to it, and a run
  of incomplete segments inside one beat is read as one arpeggiated chord. The chord's `context` says
  where the reading came from: `none` (its own), `previous`, `next` or `arpeggio`.
- **Runner-up.** The library also reports the best-scoring chord on a different root, with its
  score. The analysis document always carries it. The report's Chords table shows it only when its
  score is within 0.10 of the best chord's (`RUNNER_UP_MARGIN_THRESHOLD` in
  `src/pipeline/markdown_report.c`): a close call is worth seeing, a clear win is not.
- **Non-chord tones.** Every note in a matched segment that is not a tone of the chord is listed in
  `non_chord_tones` and in the report's Non-chord tones section, with the library's name for the
  figure (`passing`, `neighbor`, `suspension`, `anticipation`, `appoggiatura`, `escape-tone`,
  `pedal`, ...), the note, where it sounds, and the chord it is non-chordal against. A tone the
  library cannot place in one of its figures is listed as `unclassified`, not dropped.
- **Chord coverage** is the fraction of the notes' sounding time, in all segments, that is a chord
  tone or a classified non-chord tone of an identified chord. `test_chord_coverage` measures it.

See [docs/analysis-document.md](docs/analysis-document.md) for the fields.

## Roman numerals and the chord vocabulary

The analyzer adds no music theory here either. `roman_numeral_pipeline` hands Ursatz's
standard-tonality Roman numeral adapter (`standard_tonality_build_roman_numerals`) each segment's
identified chord, its key region's kernel Key, its pitch-class weights and its sounding pitches, in time
order (`standard_tonality_build_chord_inputs`), and copies what comes back. The inversion figure is read
from the library's chord bass: the lowest sounding chord tone, so a non-chord tone in the bass does not
make a chord read as root position.

- **Numerals are relative to the local key region**, not the home key: the same chord is V in the
  key it dominates and I in the key it opens. The label, its case for quality, its inversion figure
  (`6`, `6/4`; `7`, `6/5`, `4/3`, `4/2`) and its quality marks are the library's, exactly. A chord's
  `roman_numeral` in the analysis document names the region it is relative to. In the Chords
  table the Numeral column names the region on the first numeral and wherever it changes
  ("V (in G major)").
- **A chord with no numeral is reported, not dropped or guessed**: `unlabelled` with the reason. The
  reasons are no key region, no chord identified, and a chord the library gives no numeral in its key.
- **Applied chords** are the library's own applied-chord re-check; the analyzer adds no rule for
  what counts as one. They read `V/V`, `V7/V`, `V6/5/V`, `viio/V` (the library's `vii°`), with the
  chord, its target numeral and the target's tonic, what it resolves to, and whether the resolution
  is deceptive. Each is cross-referenced to the tonicization claims: the claim is the one whose span
  overlaps the chord's or begins where it ends, and it agrees when its key is the one the target
  names. A tonicization of another key, or none at all, is flagged as a disagreement in the document
  (`tonicization_agreement`) and in the report's Applied chords section, and neither side is changed.
  A single applied chord is usually not a tonicization region, so "no tonicization" is common.
- **Chord vocabulary.** Each distinct numeral of the chords that have one, overall and per key
  region: as the library names it (inversion included) and grouped by base numeral (the numeral in
  root position, so `V6/5` and `V4/3` count as `V7`), with `count`, `runs`, total length in beats,
  its share of the numbered chords' length and where it first appears, sorted by count, then
  length, then first appearance. Diatonic, applied and borrowed chords are separate groups. The
  library names no borrowed (modal mixture) chord yet, so that group is empty today.
- **Counting rule.** `count` is the number of chord rows with the numeral: every sonority segment
  counts once, whatever its length, so the counts of a scope add up to its chords with a numeral.
  A chord held across a beat is several rows; `runs` counts adjacent rows with the identical
  numeral in one key region once (an unlabelled row or a key change starts a new run).

See [docs/analysis-document.md](docs/analysis-document.md) for the fields.

## Outer voices

The analyzer adds no music theory of its own here: it renders what Ursatz's outer-voice extraction
(`outer_voice_extract`, `src/analysis/outer_voice.h`) and scale-degree labelling
(`common_practice_scale_degree_label`, `frameworks/common-practice-minimal/outer_scale_degrees.h`)
establish, in `src/pipeline/outer_voice_pipeline.c`.

- **Soprano and bass.** The lines are extracted over the voice segments (one per melody note). Each chord's
  soprano and bass are the lines' notes at the chord's start: the highest and the lowest sounding note,
  attacked or held. Doubled pitches resolve to the note attacked most recently. The library
  smooths each line across short gaps and register leaps; a line it leaves empty (a rest, a bridged gap,
  or a note dropped by the register guard) is reported as a rest, never filled from another voice.
- **Scale degrees** are taken relative to the local key: the key of the region the chord lies in, so a
  note in a tonicized or modulated region is read against that region's key, not the home key. Degrees
  are the library's labels: `1` to `7`, with `b` or `#` for a chromatic alteration (`b7`, `#4`). In a
  minor region the diatonic degrees are the natural minor's, so the raised sixth and seventh read as
  `#6` and `#7`. A chord outside every key region has pitches but no degrees. Pitches are spelled with
  flats in keys that read in flats.
- **Uncertainty.** A degree is only as sure as the key it is read against, so each degree's
  `uncertainty` in the analysis document is the library's uncertainty for that key region (0 clearly
  this key, 1 another key fits as well or better).
- **Report layout.** The soprano and bass go in their own Outer voices table, one row per chord, and not
  as columns of the Chords table, which already has ten. A chord's span is one segment, so each line has
  one note per chord; the same note appears in consecutive rows when it is held across them.
- **Crossing voices and texture.** The soprano is whatever is highest, whatever voice plays it: an inner
  voice that rises above the melody becomes the soprano for as long as it is highest. Where the melody
  repeats a note, the soprano is the note attacked most recently. When one note sounds, soprano and bass
  are that note.

See [docs/analysis-document.md](docs/analysis-document.md) for the fields.

## Key regions, tonicizations and modulations

The analyzer adds no music theory of its own here: it renders what Ursatz's key-region detection
(`key_regions_estimate`, `frameworks/common-practice-minimal/key_regions.h`) establishes, in
`src/pipeline/key_region_pipeline.c`.

- **Key regions.** The piece is cut into windows of four measures, each scored against the 24 major and
  minor keys, and a path through the windows that charges for each key change gives the local key over
  time. Regions run from barline to barline. Each has a key, a span located by measure and beat, the
  measures it covers, and the library's uncertainty (0 clearly this key, 1 another key fits as well or
  better). Key labels use flat spelling (`Eb`, `Db`) when the tonic needs fewer flats than sharps,
  for every region. Chord roots and basses are spelled the same way, following the region.
- **Home key.** The library gives no determination of its own, so the home key is the key with the
  greatest total time over all the regions in it, a tie going to the key that appears first. The
  opening and closing regions are reported as well. The home key is not necessarily either of them.
- **Notated key signatures.** The file's key signature events are input metadata
  (`metadata.key_signatures`) and a boundary for the library: the region that starts at one is
  restricted to the signature's major key and its relative minor. They never decide a key on their
  own. Where the detected keys do not fit a notated signature, the document's `notes` and the report
  say so; neither side is preferred.
- **Tonicizations and modulations.** The library classes each change between neighbouring regions: a
  region shorter than four measures that returns to the previous key is a tonicization; a longer one
  that returns is a weak modulation; one that does not return is a modulation. The analyzer then
  confirms each change with the cadences found (see "Cadences"): a change is a modulation only when a
  cadence arrives in the new key, otherwise a tonicization however long. It reports the library's
  classification and applies no rule of its own.
- **Pivot chords.** For a modulation, the library names the last segment before the new key whose
  chord is diatonic in both keys, within two measures. The analysis document gives the chord by root,
  quality, bass and inversion (no Roman numeral) with the two keys. A modulation with none is direct.

A pivot chord is named by root, quality, bass and inversion in the old key, not by Roman numeral.

See [docs/analysis-document.md](docs/analysis-document.md) for the fields.

## Cadences

The analyzer adds no music theory here (`src/pipeline/harmony_pipeline.c`, ported from Ursatz's test glue
as a rewrite with error returns). It builds the library's neutral inputs for each chord segment from the
Roman numerals (degree, quality, inversion, applied target, mode, uncertainty), the soprano and bass scale
degrees at the segment (the library's labels, a chromatic degree counted as none), the segment's place on the
meter and its beat strength, and hands them to `harmony_analysis_prepare` and `harmony_analysis_find_cadences`
with the default config. Each cadence has the library's type, its arrival and dominant, the arrival's beat
strength, the soprano degree at the arrival, the key region, the uncertainty and the share of its reading.

- **Boundary cadences.** A dominant before a section start is a half cadence of the earlier section, or the
  authentic cadence into the tonic that starts the next one
  (`harmony_analysis_add_boundary_cadences`). A section starts at each key region that returns to the
  home key after leaving it, as in Ursatz's Framework v1 chain; sections from the form structure do not
  feed back.
- **Competing readings.** A V to I6 is an evaded cadence or a prolongation of the dominant, and a dominant
  before a section start is a half cadence or an authentic one (R-090, R-091). The library reports both
  with shares in thousandths that sum to 1000. The claim carries `share` and, when it has a competing
  reading, `alternative`. The report prints both in a "Competing readings" list, the likelier first, for
  example `m23 b1 half cadence (60%), or perfect authentic cadence at m24 b1 (40%)` and
  `m4 b1 evaded cadence (55%), or tonic prolongation (45%)`. The report calls a `subverted` cadence "evaded";
  the document keeps the library's word.
- **Key change confirmation.** Each change into a new key is checked with `harmony_key_change_confirm`
  against the cadence arrivals: no cadence in the new region makes it a tonicization however long. The
  report's key changes, tonicizations and modulations follow the confirmed extents, and `cadence_confirmed`
  marks the modulations.
- **Cadence plan.** The cadences in time order with the count of each type, rendered from the analysis
  document's `cadences`.

## Harmonic function, six-fours and prolongations

Over the same prepared segments as the cadences (`src/pipeline/harmony_pipeline.c`, no music theory of its own),
the analyzer asks the library for each chord's function (`harmony_analysis_assign_functions`; an applied chord is
a dominant toward its target), marks the cadential six-fours (`harmony_analysis_mark_cadential`), classifies the
six-fours (`harmony_analysis_classify_six_fours`) and finds the prolongations
(`harmony_analysis_find_prolongations`). A diatonic predominant is strong (IV, ii) or weak (iii, vi) by the
library's `harmony_predominant_strength`. The chord claims carry `function` and `six_four`, and the new
`prolongations` section lists the spans. The Markdown report's Chords table gains Function and Six-four columns,
and a `## Prolongations` section follows the chord vocabulary. The phrase model labels the phrases' chords (see "Phrases").

## Progressions and harmonic rhythm

Counted by the analyzer over the library's Roman numerals, key regions and cadences, with no music theory of its own
(`src/pipeline/progression_pipeline.c`, `harmonic_rhythm_pipeline.c`).

- **Progressions.** A progression is a run of 2 to 6 consecutive labelled chords in one key region, named by the
  numerals' base numerals. A held chord (the same base numeral again) counts once, and a run does not cross an
  unlabelled chord or a change of key region. Every run is counted; a pattern that occurs at least twice is kept,
  unless a longer pattern with the same number of occurrences contains it. They are ranked by count, then length, then
  first occurrence. The document's `progressions` holds them all; the Markdown report's `## Progression patterns`
  shows the top 10 with the measures of each occurrence.
- **Harmonic rhythm.** A change is a chord whose base numeral or key region differs from the chord before it. The
  document's `harmonic_rhythm` has the changes in each measure, their mean, the mean per window of four measures, and
  for every cadence the mean changes per measure in the two measures before its arrival against the two before those
  (`accelerates` when the approach is faster). The Markdown report's `## Harmonic rhythm` prints the per-measure
  changes four measures to a row, the mean, and the approach to each cadence.

## Phrases

Over the cadences (`src/pipeline/form_pipeline.c` and `form_boundary.c`, ported from Ursatz's test glue as a
rewrite with error returns; the analyzer adds no music theory), the analyzer builds the library's standard-form input
from the outer-voice soprano (consecutive segments of one note are one note), the chord segments, the meter and the
cadences with their arrival spans, injects the kernel's Phrase Boundary Detection, and reads back the phrases, then
the ideas and subphrases inside each. The library's phrase model labels (`Tb`, `PD`, `D`, `Te`) go on the chords of
each phrase that ends in a cadence. The ends of all but the last phrase are then the boundaries of the second key
pass (see "Pipeline"). The Markdown report has a `## Phrases` table (measures, length, cadence, what ended it, role)
and a line per phrase with its ideas, subphrases and phrase model; the document's `phrases` has the claims. A
phrase's role is its position in the piece (the first is a beginning, the last an ending), since the library names
none.

Measured on the reference pieces (MIDI; `test_phrases`, which asserts these counts so a change in the chain shows):

| Piece | Phrases | Ideas |
| --- | --- | --- |
| Bach 114 | 2 | 10 |
| Clementi 36 | 10 | 18 |
| Mozart 545 | 52 | 166 |
| Field 1 | 10 | 30 |
| Field 5 | 9 | 22 |

These are the library's phrases, not hand readings: a phrase without a cadence ends on a break in the melody or the
end of the piece, and the cadences still miss much of the harmony (Mozart K. 545 has 52 phrases, 15 of them without a
cadence, and Field 1 has one).

## Themes and expansions

On the phrases, ideas and subphrases above the library identifies themes (`standard_form_identify_themes`) and
detects expansions and contractions (`standard_form_detect_expansions`), both with the default config. The document's
`themes` has a claim per theme with the form, the names of its two parts, their measures and cadences, the commonality
of the pairing and the traits of its continuation; each phrase claim lists the expansions and contractions that concern
it. The Markdown report has a `## Themes` table and an Expansion column in `## Phrases`. A constructed sentence,
period and compound period (`tests/musicxml/sentence.musicxml`, `period.musicxml`, `compound-period.musicxml`) are
heard as such by `test_themes`.

Measured on the reference pieces (MIDI; `test_themes` asserts structure, not these counts):

| Piece | Themes | Forms | Expansions | Contractions |
| --- | --- | --- | --- | --- |
| Bach 114 | 2 | 2 hybrid | 2 (1 internal, 1 external) | 1 |
| Clementi 36 | 3 | 3 hybrid | 0 | 3 |
| Mozart 545 | 18 | 17 hybrid, 1 repeated phrase | 6 (internal) | 9 |
| Field 1 | 5 | 5 hybrid | 3 (2 internal, 1 external) | 2 |
| Field 5 | 4 | 4 hybrid | 3 (2 internal, 1 external) | 1 |

The constructed pieces give a sentence, a period and a compound period; the reference pieces give hybrids and one
repeated phrase, and no sentence, period or compound period. Continuation traits are the claim's fragmentation only
(Field 5 has one), and the cadential ending is never identified (see "Known limits").

## Form plan

Over the phrases and themes (`src/pipeline/form_plan_pipeline.c`, ported from Ursatz's
`tests/integration/framework_v1_thematic_returns.c` as a rewrite with error returns), the analyzer gives the
kernel's form structure detection one phrase occurrence per phrase with its cadence type, and reads back the sections:
a section closes at each phrase that ends on a perfect or imperfect authentic cadence. The kernel leaves out the
phrases after the last such cadence, so the analyzer takes them as a last section (`closed: false`) and the sections
tile the phrases; a piece with no such cadence is one section. Thematic returns come from the kernel's thematic return
detection (default config) over each theme's soprano notes: a later theme recalls an earlier one when a stretch of
six notes of both has the same contour and rhythm to a similarity of at least 0.8. Themes that share a phrase (a
compound period and its sentences) are not returns of one another. The sections are reported only (R-054): they do not
feed back into the cadences, which use the key-return section starts.

The letters are the analyzer's bookkeeping over the returns. Sections are lettered by first appearance. A section
whose opening theme (a theme that starts at the section's first phrase) returns a theme of an earlier section takes
that section's letter with a prime, one more prime for each section of that letter already (`M`, `M'`, `M''`);
where several returns qualify, the most similar one decides, then the earliest section. The document's `form` is the
tree (`form`, `section`, `phrase`, `subphrase`, `idea`) and the returns; the Markdown report has a `## Form plan`
table, a `### Thematic returns` table and an indented `### Outline`.

Measured on the reference pieces (MIDI; `test_form_plan` asserts these counts and letters, and the structure):

| Piece | Sections | Letters | Returns |
| --- | --- | --- | --- |
| Bach 114 | 2 | A A' | 1 |
| Clementi 36 | 5 | A B C D E | 3 |
| Mozart 545 | 24 | A to L, M M' N O M'' P M''' Q to U | 77 |
| Field 1 | 5 | A B C D E | 0 |
| Field 5 | 4 | A B A' C | 4 |

The constructed sentence is one section, the period two (the antecedent's imperfect authentic cadence closes the
first), and the compound period two (A A'). The last section of every reference piece is open: the final phrase is
never a section boundary in the kernel's rule. Mozart 545's 77 returns are almost all between hybrids and repeated
phrases whose six-note stretches share a contour and rhythm (similarity 0.80 to 1.00), so they say that stock figures
recur, not that a theme returns; only the opening themes decide the letters.

## Motives

Over a note-level soprano (`src/pipeline/motive_pipeline.c`, ported from Ursatz's `tests/integration/motive_util.h`,
`form_kernel_bridge.h` and `test_motive_mozart545.c` as a rewrite with error returns), the analyzer runs the library's
motive analysis with the default config: motives of 3 to 8 notes that recur, in the soprano only. The soprano is the
highest sounding note at each attack, because the outer voices keep one note per voice segment and would thin a run of
sixteenths; each note carries its scale degree in its key region, so a diatonic sequence matches. The motive analysis
needs phrases, ideas and themes on the same notes, so the stage reruns them on its own input; its occurrences are then
placed in the report's phrases and ideas by span (an occurrence that crosses a phrase boundary lies in none).

A family has a prototype and occurrences, each with the library's kind against the prototype (exact, real-transposed,
tonal-transposed, inverted, retrograde, augmented, diminished, fragmented, varied) and its score. The development of a
family is its occurrences in time order. The prototype is not always the earliest occurrence, because the library
searches continuations for fragments first. The document's `motives` holds all of it; the Markdown report has a
`## Motives` table (with the prototype's degrees) and a `### Development` list.

Each family also carries `features`, read from its prototype: the notes with exact onsets and durations, the key
region's tonic and mode, the meter, the degrees against the tonic's major scale (`1 b2 2 b3 3 4 #4 5 b6 6 b7 7`, so a
minor-key `1 b6 5` reads the same as one in major), the intervals in semitones and scale steps, the rhythm, the
non-chord-tone type of each note's segment, and a fingerprint of the prototype as a library `MotiveSketch`
(`src/pipeline/motive_sketch_map.c` is the one mapping from features to the sketch, shared by the report and the
motive links). Each occurrence carries its notes' degrees. See
[docs/analysis-document.md](docs/analysis-document/motive-and-melody.md#motives) and [docs/style-profile.md](docs/style-profile.md).

Measured on the reference pieces (MIDI):

| Piece | Families | Occurrences | Kinds |
| --- | --- | --- | --- |
| Bach 114 | 32 | 92 | exact 78, fragmented 14 |
| Clementi 36 | 0 | 0 | |
| Mozart 545 | 214 | 1610 | exact 556, fragmented 695, tonal-transposed 163, real-transposed 101, varied 38, inverted 35, retrograde 20, augmented 2 |
| Field 1 | 1 | 4 | exact 1, real-transposed 2, fragmented 1 |
| Field 5 | 10 | 48 | exact 32, inverted 8, fragmented 6, tonal-transposed 2 |

`test_motives` checks Mozart 545 against Ursatz's hand annotation of the exposition
(`tests/fixtures/annotations/mozart545_motives.txt`, a draft reading): all 7 annotated families are found, and 20 of
the 21 annotated occurrences are found with the right kind (95.2%; the miss is the tonal-transposed occurrence at
measure 21 beat 2), against the library test's 75% target. The Analyzer finds the motives over the whole piece with
its own cadences, not over the exposition with annotated phrase ends.

## Melody

For each phrase, the analyzer builds the soprano as the neutral line of the melody adapter of Ursatz's
standard-counterpoint (`src/pipeline/melody_pipeline.c`, ported from `tests/integration/melody_util.h` and
`test_melody_bach114.c`) and reports the library's results: the range, the climax (the first note at the highest pitch),
the contour (the library's direction of each interval, counted, no shape named), every leap with its recovery status
(the cantus firmus rule LEAP_RECOVERY), the pairs of leaps in a row the library's rule faults, the non-chord tones by
type (the chord identification's result attached to each note), and the tendencies. The line is the outer voice soprano
of the phrase, one note per melody note (consecutive voice segments with the same note are one note, a note held in from
the previous phrase is left out, a gap is a rest); each note carries its scale degree in its key region and its beat
strength. The library has no table of scale-degree tendencies, so the report gives the five line tendencies of
`tendency_rules.h` (pitch proximity, step declination, step inertia, melodic regression, melodic arch) as held or not, and
the seventh degree judged by the cantus firmus rule LEADING_TONE: how many sevenths a note follows that follow the rule,
and where the broken ones stand. The document's `melody` holds all of it; the Markdown report has a `## Melody` table and
the subsections `### Leaps`, `### Non-chord tones` and `### Tendencies`.

Measured on the reference pieces (MIDI), summed over the phrases:

| Piece | Phrases | Notes | Leaps (recovered / unrecovered / third / not judged) | Faulted pairs | Non-chord tones | Seventh followed / broken |
| --- | --- | --- | --- | --- | --- | --- |
| Bach 114 | 2 | 149 | 9 / 27 / 18 / 0 | 2 | 11 | 16 / 4 |
| Clementi 36 | 10 | 151 | 0 / 19 / 37 / 6 | 21 | 3 | 8 / 5 |
| Mozart 545 | 52 | 910 | 30 / 217 / 232 / 27 | 189 | 94 | 30 / 45 |
| Field 1 | 10 | 164 | 11 / 26 / 24 / 4 | 22 | 15 | 4 / 8 |
| Field 5 | 9 | 113 | 6 / 20 / 11 / 1 | 4 | 4 | 5 / 4 |

`test_melody` checks the structure of the section on Bach 114 (the range, climax, contour, leaps and non-chord tones agree
with the notes), and its floors against Ursatz's hand annotation of the first eight measures
(`tests/fixtures/annotations/bach114_melody.txt`, a draft reading with placeholder phrase spans): both climaxes are right,
none of the ranges and 4 of the 11 annotated leaps have the annotated status, the same as Ursatz's own whole-chain run.
`test_melody_bach_114_target` asserts the library test's target (every climax and range, at least 90% of the leaps) and is
marked `WILL_FAIL` (see "Known limits").

## Rhythm and meter

The analyzer builds the neutral input of the rhythm adapter of Ursatz's standard-meter
(`src/pipeline/rhythm_pipeline.c`, ported from `tests/integration/rhythm_util.h`) and reports the library's results.
The input holds every note (its line: the outer voice soprano, the bass or any other), a chord change wherever the base
numeral or key region differs from the chord before, and the phrases with the arrival of their cadence and their length
change. The document's `rhythm` is one flat list of claims labelled by kind:

- **Meter fit** (first in the list): the meter the analysis used (`declared`: meter, scale, pickup and a score from 0 to 1)
  against the best other reading (`alternative`, `null` unless it scores at least 0.15 higher). The score is how well the
  reading's barlines carry the harmony changes (on a downbeat, weight 2; on any beat, 1) and the cadence arrivals (on a
  downbeat, 3). It exists for a file whose meter does not fit its music, such as a 3/4 piece written in 4/4; the
  alternative is the `--meter`, `--value-scale` and `--pickup` that would read it. It is offered and never applied. A reading
  applied with those options is the one checked, so a corrected piece offers nothing. The Markdown report prints one line
  of it under `## Rhythm and meter`.
- **Density** per phrase, for all its notes and for each line: notes, attacks, attacks per beat, the mean distance
  between attacks, the share of attacks on the beat and a histogram of the notes' lengths. The recurring rhythm patterns
  are the motive families' rhythms (see "Motives"), not a second detector.
- **Syncopation**: a note attacked at a weaker metric level and held through a stronger one that no note of its line
  attacks, with the level it is attacked at, the one it is held through and the strength of the difference.
- **Hemiola**: two measures of a simple triple meter, or one of 6/8, whose six pulses read as three duple units, with the
  three kinds of evidence (the durations, the chord changes, the accents) and how many agree.
- **Hypermeter**: for each phrase the hypermeasure length (2 or 4 measures) that fills it and holds the cadence in the
  last hypermeasure, the hyperdownbeats and the hypermeasures (regular, short, expanded, extension or contracted).
- **Tuplets**: runs of triplets (sextuplets included) and duplets, named borrowed or not and placed at a level by the
  library's rules. The importers carry no notated tuplets, so they are read from the notes' times alone, rounded to a grid
  of 96 to the whole note.

The Markdown report has a `## Rhythm and meter` section with a subsection for each.

Measured on the reference pieces (MIDI):

| Piece | Meter read | Phrases | Syncopations (soprano / bass / other) | Hemiolas | Hypermeasures (regular / short / expanded / extension / contracted) | Tuplet runs |
| --- | --- | --- | --- | --- | --- | --- |
| Bach 114 | 4/4 | 2 | 21 (4 / 11 / 6) | 0 | 8 (6 / 1 / 1 / 0 / 0) | 0 |
| Clementi 36 | 4/4 | 10 | 0 | 0 | 14 (9 / 2 / 0 / 0 / 3) | 0 |
| Mozart 545 | 4/4 | 52 | 31 (29 / 0 / 2) | 0 | 113 (79 / 19 / 6 / 0 / 9) | 4 (triplets, borrowed) |
| Field 1 | 12/8 | 10 | 146 (114 / 1 / 31) | 0 | 19 (11 / 3 / 2 / 1 / 2) | 83 (triplets, not borrowed) |
| Field 5 | 12/8 | 9 | 11 (5 / 0 / 6) | 0 | 15 (6 / 6 / 2 / 1 / 0) | 2 (triplets, not borrowed) |

`test_rhythm` checks the structure of the section on Field 5 and its floors against Ursatz's hand annotation
(`tests/fixtures/annotations/field5_rhythm.txt`, a draft reading of measures 1 to 30 of the 12/8 score): the annotated
syncopation (measure 14, beat 2) is found, no hemiola is annotated (12/8 has none) and 7 of the 8 annotated hyperdownbeats
are the first measure of a hypermeasure, against the phrases the form adapter detects (Ursatz scores the annotated
phrases). `test_rhythm_constructed` runs small pieces whose answer is known: eighth-note triplets in 4/4 (one run of six
attacks, a borrowed division at the beat), a duplet in 6/8 and a hemiola in 3/4. `test_rhythm_field5_target` asserts
Ursatz's target (80% of the syncopations and hyperdownbeats, every hemiola, every reinterpretation) and is marked
`WILL_FAIL` (see "Known limits").

## Texture

The analyzer builds the neutral input of the texture adapter of Ursatz's standard-texture
(`src/pipeline/texture_pipeline.c`, ported from `tests/integration/texture_util.h`) and reports the library's results.
Every imported note carries a stream (`ImportedNotes.streams`: the part, that is the MIDI track and channel read back
from the file, or the MusicXML part), the kernel's voice separation runs per stream (a part is usually a hand) and gives each note a voice and
a confidence, and the library measures each measure, classifies it with its defaults and merges equal consecutive
measures into passages. The document's `texture` has a claim per passage (texture, category, measures, confidence and
the measured features) and one per change. The library names no accompaniment pattern, so the passage reports the
repeated-pattern score only. The Markdown report's `## Texture` prints the passages and the changes.

Measured on the reference pieces (MIDI), passages by texture:

| Piece | Passages | Changes (category changes) | Monophony | Polyphony | Homorhythm | Melody and accompaniment |
| --- | --- | --- | --- | --- | --- | --- |
| Bach 114 | 9 | 8 (4) | 0 | 2 | 2 | 5 |
| Clementi 36 | 7 | 6 (4) | 0 | 2 | 1 | 4 |
| Mozart 545 | 137 | 136 (60) | 15 | 16 | 47 | 59 |
| Field 1 | 10 | 9 (9) | 1 | 4 | 0 | 5 |
| Field 5 | 19 | 18 (14) | 0 | 7 | 2 | 10 |

`test_texture` checks the structure on Field 1 and scores it against Ursatz's hand annotation
(`tests/fixtures/annotations/field1_texture.txt`, a draft reading): 59 of 65 annotated measures have the right category
(90.8%, target 80%; measures 8, 40, 42, 43, 63 and 64 are read as polyphony, the annotation says homophony) and the one
annotated change point (measure 2) is found. A MusicXML file's streams are its parts, so its passages can differ from the
MIDI file's.

## Voice leading

The analyzer builds the voice-pitch grid of the adapters of Ursatz's standard-counterpoint and standard-harmony
(`src/pipeline/voice_leading_pipeline.c`, ported from `tests/integration/voice_leading_util.h` and
`test_voice_leading_report.c`) and reports the library's results. The kernel's voice separation runs over every note of
the piece as one stream (all tracks and parts together, voice 0 the top); the grid has one row per chord segment (a
report entry) and one column per voice, up to six (lower voices are left out), and each cell holds the note of that voice
sounding at the start of the segment. The cell's spelling is the input's own when it is notated and matches the sounding
pitch, otherwise the library derives it from the semitone. The document's `voice_leading` has these claims:

- `motions`, one claim: the library's classification (contrary, similar, parallel, oblique, static) of the motion between
  consecutive chord segments for every pair of voices that sound in both, in total and per pair.
- `violation`: a parallel fifth or octave between any pair of voices, or a direct fifth or octave in the outer pair
  (`check_perfects`), with the voices, the two chords and the four notes.
- `resolution`: a seventh or leading tone left unresolved when its chord changes (`check_resolutions`, against the chords
  the harmony stage prepared), with the voice, the two chords and the two notes.
- `doubling`: for each chord three or more voices sound, the chord member doubled, the voices on notes the chord does not
  have, the semitone gaps between adjacent voices and, when exactly four voices sound, the verdict of standard-voicing's
  spacing rule (`spacing_rules.h`: an octave between soprano and alto and between alto and tenor, a twelfth between tenor
  and bass).

Doubling is counted, not judged: the chord members are found from the chord's interval set and the bass, and
standard-voicing's doubling rules need chord spellings the analyzer does not build (R-018). The Markdown report's
`## Voice leading` prints the motion table, the faults and a summary of the doubling.

Measured on the reference pieces (MIDI):

| Piece | Voices | Chord segments | Motions | Parallel fifths | Parallel octaves | Direct | Unresolved sevenths | Unresolved leading tones | Chords with doubling | Four-voice chords, too wide |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| Bach 114 | 4 | 155 | 74 | 0 | 0 | 0 | 6 | 1 | 6 | 0 of 6 |
| Clementi 36 | 3 | 205 | 55 | 0 | 0 | 0 | 9 | 0 | 3 | none |
| Mozart 545 | 4 | 1095 | 1025 | 2 | 11 | 0 | 57 | 12 | 289 | 22 of 88 |
| Field 1 | 4 | 268 | 128 | 0 | 1 | 1 | 19 | 2 | 43 | 2 of 4 |
| Field 5 | 4 | 169 | 531 | 0 | 0 | 2 | 24 | 2 | 127 | 39 of 83 |

The counts follow the voice separation: a piece whose voices rest often has fewer motions than segments. `test_voice_leading`
runs a constructed chorale (`tests/musicxml/chorale-voice-leading.musicxml`) with one parallel fifth and one unresolved
leading tone and asserts that both are found at their measures and nothing else.

## Schemas and sequences

The analyzer builds the neutral input of Ursatz's galant-schemas and scale-harmonization adapters
(`src/pipeline/schema_pipeline.c`, ported from `tests/integration/schema_util.h`) and reports the library's matches. Each
chord segment becomes one input segment: the outer voices' pitch classes read as degrees of its local key (the Roman
numeral input's tonic and mode) by the offset above the tonic, that key as a degree of the home key, the library's
numeral (an applied chord's root from the tonicized degree), and the strength and length of its beat. Consecutive chords
with the same key, outer voice degrees and numeral are one segment, with the uncertainty of the least certain; a chord
outside every key region is left out, and a segment's uncertainty is its numeral's (0.5 with none). The library's defaults
run unchanged. The document's `schemas` is one flat array with two kinds of claim:

- `schema`: a galant schema with its function, the stages (each with the measure it starts in and the soprano and bass
  degrees matched; a stage may match on the bass alone, and a two-note stage has a second soprano degree), the frame key
  it was found in, and the library's confidence parts.
- `sequence`: a model of one or more chords repeated, transposed by a constant diatonic interval, at least three times,
  with its kind, units, direction, transposition in diatonic steps and stages (bass, soprano and root degrees in the home
  key). The cycle of fifths has no rule in scale-harmonization yet, so the library reports it from the Roman numerals
  alone (`rule_backed` false).

The Markdown report's `## Schemas and sequences` prints a table of each.

Measured on the reference pieces (MIDI, the library's defaults): no piece has more than one schema.

| Piece | Schemas | Sequences |
| --- | --- | --- |
| Bach 114 | 0 | 2 (parallel chords) |
| Clementi 36 | 0 | 7 (parallel chords) |
| Mozart 545 | 1 (a Jupiter) | 5 (4 cycles of fifths, 1 parallel chords) |
| Field 1 | 0 | 0 |
| Field 5 | 0 | 0 |

`test_schemas` writes the hand-written galant fixture of Ursatz's `test_schema_sequence.c` as a MIDI file and runs the whole
chain on it, as Ursatz does for its chain: 5 of the 9 planted items are found (the Romanesca twice, the Prinner, the
Do-Re-Mi and the ascending 5-6; the Meyer, Fonte, Monte and the cycle of fifths are missed), above the floor of 4 that
Ursatz asserts. Ursatz also scores the adapters on the fixture's own segments (all 9 found); the analyzer has no such
entry point. The reference pieces' counts above are pinned.

## Known limits

- Key regions use fixed four-measure windows, so a boundary can land a measure or two from where a
  phrase would put it and a modulation shorter than about half a window is smoothed away. A stretch of
  four or more measures built on the subdominant, dominant or supertonic can read as its own key.
  The chord evidence of the first pass lets the library hold the home key through a passage on IV, V or
  ii when no tone outside the key is heard.
  Measured here (`test_key_regions`, still `WILL_FAIL`): Clementi Op. 36 No. 1 reads as annotated except
  measure 20 (the dominant of C minor reads as G major). Mozart K. 545 (the file holds all three
  movements) comes out as 43 regions, and 38 measures inside the cores of the annotated first-movement
  regions read in a neighbouring key (F major, G major, A minor, D minor or E minor); no modulation is
  found near measures 13 (to G), 29 (to C), 41 (to G), 79 or 124 (to C): the first three regions have no cadence
  arriving in them, so they are tonicizations, and the returns at 79 and 124 are not read. Bach 114 is read as G major, D major from measure 14, G major from measure 16 and D major from
  measure 20 to the end. Its file declares 4/4 for a 3/4 piece and the analyzer reads the file's meter, so its
  measures and beat strengths are those of a 4/4 reading: the two perfect authentic cadences of the 3/4 score (m. 20
  and m. 28) arrive at m. 15 and m. 21, beat 3.5. A region read in a key the music is not in also reads its chords in that key.
  The profile fits chromatic music less well. A key signature is trusted as a boundary: many exporters
  write a default C major at tick 0 whatever the key.
- Themes: the reference pieces yield no sentence, period or compound period (17 of Mozart 545's 18 themes and all
  the others are hybrids, one is a repeated phrase), so the sentence, period and compound period cases are tested on
  constructed pieces only. The library's ideas are the nominal two-measure cut, its cadential ending needs the bass
  (`bass_is_cadential_pattern`) and is never identified, fragmentation is judged on those nominal ideas and is rarely
  found, and its claim gives no acceleration or sequence trait. A constructed period cannot end its antecedent on a
  half cadence: a V followed by I reads as an authentic cadence, so the constructed antecedent ends on an imperfect
  authentic one.
- Motives: only the soprano is read (the library's `include_bass` is off), so there are no bass motives. The soprano is
  the highest sounding note at each attack, so a left-hand note is the line where the right hand rests. Clementi 36
  yields no family. Mozart 545 yields 214 families and 1610 occurrences, 168 of which cross a phrase boundary and lie in
  no phrase. A family's prototype can come after fragments of itself.
- Melody: only the soprano is read (the bass line is not asked for), and the outer voice extraction keeps one soprano
  note per voice segment, which drops the quick notes some leaps arrive on, and adopts a left-hand note where the right
  hand rests (Bach 114 measure 4). Bach 114 gets none of the annotated ranges and 4 of the 11 annotated leaps with the
  annotated status (36.4%, target 90%): `test_melody_bach_114_target` is `WILL_FAIL`. A MIDI file has no spellings, so the
  library reads a leap's size from the scale degrees, and two notes a semitone apart whose degrees are a fifth apart read
  as a leap of a fifth (Bach 114, measures 13 and 19). A phrase is one line, so a leap at its end has nothing after it (not judged). The line
  tendencies treat a rest in the line as no gap.
- Rhythm: the form adapter does not report elided cadences, so the hypermeter reads no phrase as elided and reports no
  hypermetric reinterpretation. Field 5's annotation has one (measure 9, where the phrase of measures 5 to 8 is elided with
  the next): `test_rhythm_field5_target` is `WILL_FAIL`, with the syncopation, the hemiolas (none) and 7 of 8
  hyperdownbeats (87.5%, target 80%; measure 18 is missed) meeting its target. The hypermeter reads the phrases as the
  form adapter cuts them, so a phrase of one or three measures takes the better of the two lengths and ends in a short
  hypermeasure. Tuplets are read from times alone: only triplets, sextuplets and duplets are looked for, a MIDI file
  whose times are not on the grid of 96 to the whole note (an expressive performance) yields few or none, and a duplet
  in a compound meter is two notes that halve a beat, which sound the same as two dotted values. Field 1, a performed
  12/8 file, yields 83 runs of triplets within the beat that the library does not call borrowed.
- Form plan: the kernel's sections close only on perfect and imperfect authentic cadences, so a piece of half
  cadences and deceptive endings is one section, and the final phrase never closes one. Returns compare six-note
  stretches by contour and rhythm, so a figure that recurs inside a hybrid reads as a return (Mozart 545: 77). A
  section's key is the key region its first chord lies in, not the key it ends in.
- Pivot chords are read in the old key and its mode; a minor old key is read in its minor vocabulary.
- Chord identification is Ursatz's weighted identification, read in each segment's key region and its
  mode: the diatonic triads and sevenths of a major key, or of natural, harmonic and melodic minor in a
  minor region. When no key is found, no chord is identified. Chord coverage on the reference pieces:
  Bach 114 96.2%, Clementi 36 95.2%, Field 1 92.7%, Field 5 87.9%, Mozart 545 92.4%. Chromatic, applied
  and fully diminished chords outside minor are not candidates, so a segment built on one is matched to
  the nearest diatonic chord or not at all.
- Roman numerals follow chord identification, so a chord has a numeral only where the weighted
  identification found one, and a chromatic chord is matched to the nearest
  diatonic chord or not at all. The library's adapter reads a chord that is not a degree of its key,
  or has a quality the key does not give that degree, as having no numeral; it names no borrowed
  (modal mixture) chord, Neapolitan or augmented sixth, so the borrowed group of the vocabulary is
  empty. It reads an applied chord only when the chord was identified as a diatonic neighbour and
  resolves as the library requires.
- The outer voices in the document are per chord: a chord's soprano and bass are the lines' notes at its
  start, so a melody note attacked later inside a merged chord segment is not listed.
- Non-chord tones are classified only where the library can: its passing, neighbor, suspension,
  anticipation, appoggiatura, escape-tone and pedal rules. Many tones come back unclassified.
- Progressions are the library's base numerals counted as they come: Bach 114's most common progression is
  I > IV7 (6 occurrences, tied with ii > iii7), which ends on IV7, so `test_progressions_bach_114_top` (its top
  progression ends on I) is `WILL_FAIL`. The 4/4 reading of Bach 114's 3/4 meter (see below) also shapes its
  measures and so its per-measure harmonic rhythm.
- SMPTE-division MIDI files are rejected.
- Compressed `.mxl` MusicXML is not read: the Ursatz importer takes uncompressed documents only.
- MusicXML repeats and endings are not unfolded, and the pickup is found only when the first measure
  is marked `implicit="yes"`.
- Measure and beat follow the file's time signature events. A file that declares the wrong meter
  (for example, a minuet marked 4/4) is reported as declared.
