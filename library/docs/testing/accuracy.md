# Accuracy harness

[Docs index](../README.md): the Ursatz docs by topic.

`tests/accuracy/` measures the whole chain (import, segments, local keys, chords, Roman numerals, and the L3
adapters) against published annotations and the repository's hand-annotated fixtures, reports the accuracy per piece,
and fails CI when it drops below a recorded baseline. It is test infrastructure, not library code. Nothing in it
changes how the analysis runs: every library default is used as it is.

The analysis it measures is `analysis_chain_run` (`chain/`), the production chain, run on the score's notes in two passes: the first for the key regions from the pitch classes alone, then the harness finds the phrase ends (the ends of the cadences that pass reads, on outer degrees taken from the highest and lowest member of each segment), and the second pass reads the keys again with the first pass's chords as evidence and those ends as boundaries (`refine_with_given_boundaries`). What stays in `tests/accuracy/chorale_chain.c` is the fixture loading, the RomanText alignment, those phrase ends, the numerals in the annotation's keys and the scoring.

The CTest target is `accuracy_harness`. Run it directly with `build/tests/accuracy/accuracy_harness`; it takes about
a second.

## What it measures

**Chorales.** Each piece under `tests/accuracy/corpus/chorale-*` has a MusicXML score and a RomanText analysis. The
harness runs the chain on the score and compares it with the analysis over time.

**Reference fixtures.** The existing accuracy checks under `tests/integration/` (cadences, phrase form, texture,
rhythm, melody, motives, the phrase model, schemas) are compiled into the harness with `main` renamed, run as they
are, and their whole-chain summary lines ("N of M") are read. The tests are not edited and keep their own assertions.
The voice leading report is not read: it lists findings and does not score against an annotation.

## Metrics

All values are fractions in `report.json` and `baseline.json`, percentages in `report.md`. A piece with nothing
annotated for a metric shows **n/a** and does not enter the overall. The overall value of a metric pools the
numerators and denominators of all its pieces, so a long piece weighs more than a short one.

Chorale metrics are time-weighted. Time is cut at every segment boundary and every annotated chord or key change, and
each stretch counts for its length in whole notes, from the first annotated chord to the end of the score.

| Metric | Right when | Denominator |
|---|---|---|
| `chord_root` | the root pitch class of the chain's chord equals the annotated chord's | time under supported annotated chords |
| `chord_quality` | the chain's chord has the annotated chord's root-relative interval set (a triad or a seventh quality) | same |
| `roman_numeral` | the numeral the adapter reads in the **annotation's** local key has the annotated degree (for an applied chord, the degree of the applied chord and the degree of its target), inversion, seventh-or-triad, quality and applied-or-not | same |
| `roman_numeral_chain_key` | as `roman_numeral`, with the adapter reading in the **chain's own** local key | same |
| `key` | the chain's local key at the stretch has the annotation's tonic and mode (a relative major or minor is wrong, there is no partial credit) | all annotated time |

A stretch with no segment (silence), a segment with no chord match and a segment before the first key count as wrong.
`roman_numeral` isolates the chord reading from the key estimate; `roman_numeral_chain_key` is the end-to-end figure.
An annotated chord with a lowered or raised degree (`bVII`, `#iv`) never matches a numeral: the adapter reads
diatonic degrees only. The report also shows each piece's **unsupported chord time**, the share of annotated time
under a chord the loader does not read (see below); it is left out of the chord denominators and is not gated.

Fixture metrics are counts, as the integration tests define them:

| Metric | From | Counts |
|---|---|---|
| `cadence_recall`, `cadence_type`, `cadence_precision` | `test_cadences_reference_pieces` (whole chain, five pieces) | annotated cadences found within one beat; found cadences with the right type; reported cadences that match an annotation |
| `phrase_boundaries`, `theme_types`, `expansions` | `test_form_reference_pieces` (whole chain, pooled) | annotated phrase ends found; themes of the right form; expansions of two or more measures found |
| `phrase_model_labels` | `test_mozart545_phrase_model` | annotated segments with the right phrase-model label |
| `melody_climaxes`, `melody_ranges`, `melody_leaps` | `test_melody_bach114` (extracted soprano) | annotated climaxes, ranges and leaps found right |
| `motive_occurrences` | `test_motive_mozart545` | annotated occurrences found with the right kind |
| `rhythm_syncopations`, `rhythm_hyperdownbeats` | `test_rhythm_field5` (whole chain) | annotated syncopations and hyperdownbeats found |
| `texture_measures`, `texture_changes` | `test_texture_field1` | annotated measures with the right category; change points found |
| `schemas_sequences` | `test_schema_sequence` (whole chain, galant fixture) | planted schemas and sequences found |

The fixtures are drafts: hand readings, not yet checked against printed scores (see the headers in
`tests/fixtures/annotations/`). Their numbers measure the chain against those drafts.

## Reference harmony

Michelle's hand harmony of the Clementi and Mozart reference pieces (`tests/fixtures/annotations/clementi36_harmony.txt`,
`mozart545_harmony.txt`, verified 2026-11-07) is read by `tests/analysis/test_reference_harmony.c`. For each annotated chord it checks whether
the chain's chord sounding at the chord's onset has the same root and triad quality (the seventh is not compared; an applied numeral is
compared by the root and quality of the applied chord until P40), and it counts the chord changes the chain makes inside the annotated
stretches against the changes the annotation has. Since P39 the test asserts the target bars (R-371): in Clementi measures 1 to 5, 8 and 9
and Mozart measures 1 to 6, 11 and 12 every annotated chord matches, and the chain makes no chord change inside Clementi measure 8 or
Mozart measures 1 and 8. Mozart measure 8 is one chord; which chord is reported, not asserted (R-375). Every other bar is reported. The
pieces are not in the harness metrics above.

| Piece | Chords matched | Chain changes | Annotated changes | Extra changes |
|---|---|---|---|---|
| Clementi 36 | 8 / 10 | 8 | 9 | -1 |
| Mozart 545 | 15 / 17 | 16 | 15 | 1 |

Measured 2026-11-08 (P39 B3, chords on harmonic spans; P38 B4 before: Clementi 5 / 10 with 24 extra changes, Mozart 13 / 17 with 17). The
misses, none in a target bar: Clementi m6 b4 (viio7/V, chain C major) and m7 b2 (C major, chain A minor); Mozart m8 b1 (A minor, the chain
reads the bar as one C major chord) and m10 b1 (F major, no chord).

## Harmonic spans

P39 reads chords on harmonic spans (`AnalysisChainConfig.harmonic_spans`, default on) with a seventh penalty of 0.15
(`ANALYSIS_CHAIN_SPAN_SEVENTH_PENALTY`, R-376). Chorale harness, spans off then on (chord root, chord quality, Roman numeral, numeral in the
chain's key, key): 84.5%, 67.2%, 67.4%, 52.0%, 69.1% before; 86.4%, 75.9%, 70.0%, 54.7%, 69.6% after. Without the penalty the same spans read
82.7% root and 60.1% quality (a span reads the union of a beat or more, so a passing tone completes a seventh over a triad), under the guards of
84.5% and 67.2%. The other gated metrics did not move. The baseline was re-recorded in P39 B3.

## The chain on a chorale

1. The MusicXML score is imported. `score_layout.h` reads where each measure of the first part starts from the
   MusicXML, because the importer lays barlines from the time signature alone and a pickup or a measure split across
   a repeat ("7" and "7a") would otherwise misplace every later barline. A measure numbered with a letter continues the
   one before it. The pickup (measure 0) and the signature become the `MeterMap` the chain uses.
2. Adaptive segments are built on that meter and grouped into harmonic spans (one segment per span, with its bass, R-369 to R-373),
   weighted, and the key regions are estimated (the score's key signature events are passed in, as the importer reads them).
3. Chords are identified per key region, in that region's own key and mode: the major set in a major region and the
   minor vocabulary (natural, harmonic and melodic minor triads and sevenths, with the raised V and vii-dim winning an
   exact tie) in a minor one.
4. The standard-tonality adapter reads the Roman numerals twice: in the annotation's key regions and in the chain's.

## RomanText subset

The loader (`romantext.h`) reads the format of the "When in Rome" corpus:

- metadata `Composer:`, `Title:`, `Time Signature:`;
- measure lines `m<N> ...`, with `m0` for a pickup, and `b<beat>` tokens (`b2`, `b2.5`; the beat is the denominator
  note, or the dotted beat of a compound signature);
- key tokens (`G:`, `a:`, `Bb:`, `f#:`; a capital is major);
- chords: degree `I`..`VII` with case, a lowering or raising prefix (`b`, `-`, `#`), `o`, `%`, `+`, figures
  (`6`, `64`, `7`, `65`, `43`, `42`, `2`, and `6/4`, `6/5`, `4/3`, `4/2`), and one applied target (`V7/V`, `viio6/ii`);
- ignored: barline and repeat marks, `var...` and `Phrase...` tags, and `Note:`, `Phrase`, `Pedal` lines.

Anything else is kept as written and counted as **unsupported**, never an error: augmented sixth chords (`Ger65`,
`Fr43`, `It6`), the Neapolitan `N`, `Cad64`, bracketed additions (`[no5]`), figures beyond sevenths, nested applied
targets, measure repeats (`m5-6 = m1-2`), and variant lines (`m11var1`, skipped so the main reading stands). A
position outside the score is an error that names the line.

## Corpus and licenses

`tests/accuracy/corpus/chorale-<number>-bwv<number>/{score.musicxml, analysis.rntxt}`: ten Bach chorales, scores from
the music21 corpus and analyses from When in Rome, each copied unmodified. `corpus/LICENSES.md` records the source
URL, commit, license text and status of every file. The music is public domain; the encodings and analyses are not
all stated to be. **The corpus is for private use. Review or replace it before publishing the repository, sharing it
or using it commercially.** Material with a known non-commercial license is not included. The corpus can be swapped
for other pieces with `--corpus-dir`; a different corpus needs its own baseline.

A chorale is admitted only if its analysis and score agree: `test_corpus_load` requires that the roots of the
annotated chords sound in the score where the chords start (`alignment_root_share`, at least 85%). Two sources of one
chorale can be in different transpositions; three of the first candidates were, and are left out.

To add a chorale, add its directory, add a row to `LICENSES.md`, check that `accuracy_corpus_load` passes, and raise
the baseline (below).

## Reading `report.md`

`report.md` has a table of the chorales (one row per piece, the overall row last), a table of the fixture
metrics per fixture, and the fixture metrics pooled. The copy under `tests/accuracy/` is the snapshot recorded with
the baseline; a run writes its own `report.md` and `report.json` to the build directory
(`build/tests/accuracy/`), and CI uploads those as the `accuracy-report` artifact.

## The baseline and the gate

`tests/accuracy/baseline.json` holds the tolerance (`tolerance_points`, in percentage points, default 0.5), the
overall value of every metric, and the per-piece values for reading and diffing. Only the overall values are gated.

The harness compares each overall metric with the baseline and exits 1, printing the table, when:

- a metric is more than the tolerance below its baseline (a drop of exactly the tolerance passes);
- a metric in the baseline was not produced (a piece, a fixture or a summary line went missing);
- a metric was produced that the baseline does not have.

An improvement never fails. The tolerance is read from the file, so changing it is a change to the baseline.
`--no-gate` reports without comparing.

## Raising the baseline

Raise it when the analysis has deliberately changed what it measures: a stage was improved (the numbers go up), the
corpus changed, or a loss is accepted and understood. Never to make a failing run pass without knowing why.

1. Run `build/tests/accuracy/accuracy_harness --update-baseline`. It rewrites `baseline.json` and the
   `report.md` / `report.json` snapshot in `tests/accuracy/`.
2. Look at the diff. Per-piece values show where a change came from.
3. Commit those three files **on their own**, titled `test - Record the accuracy baseline` (or `... Raise ...`), with
   the before and after overall values in the commit message body.

## Options

`--corpus-dir DIR`, `--report-dir DIR`, `--baseline FILE`, `--tolerance POINTS` (written by `--update-baseline`),
`--update-baseline`, `--no-gate`.

The five reference pieces are also verified against exact expected values (keys, cadences, phrase form, thematic returns): `docs/testing/framework_v1_verification.md`.
