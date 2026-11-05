# Harmony analyses

What the Analyzer reads for chords, Roman numerals, outer voices, keys, cadences, functions and progressions, and how. The fields of these claims are in [chords](../analysis-document/chords.md), [keys](../analysis-document/keys.md) and [harmonic structure](../analysis-document/harmony-structure.md). Back to the [docs index](../README.md).

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
  confirms each change with the cadences found (see [Cadences](#cadences)): a change is a modulation only when a
  cadence arrives in the new key, otherwise a tonicization however long. It reports the library's
  classification and applies no rule of its own.
- **Pivot chords.** For a modulation, the library names the last segment before the new key whose
  chord is diatonic in both keys, within two measures. The analysis document gives the chord by root,
  quality, bass and inversion (no Roman numeral) with the two keys. A modulation with none is direct.

A pivot chord is named by root, quality, bass and inversion in the old key, not by Roman numeral.

## Cadences

The analyzer adds no music theory here (`src/pipeline/harmony_pipeline.c`). It builds the library's neutral inputs for each chord segment from the
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
and a `## Prolongations` section follows the chord vocabulary. The phrase model labels the phrases' chords (see [Phrases](form-and-motive.md#phrases)).

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
