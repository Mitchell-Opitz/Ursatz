# Framework v1 verification

[Docs index](../README.md): the Ursatz docs by topic.

Framework v1's finite endpoint is the five-piece reference set in `tests/fixtures/framework-v1-corpus/`: Bach, Minuet in G, BWV Anh. 114; Clementi, Sonatina
Op. 36 No. 1, first movement; Field, Nocturne No. 1; Field, Nocturne No. 5; Mozart, Sonata K. 545, first movement. Until this suite existed, `CorpusRegression`
checked only that the five files register, and a manual pass in the GUI had looked at Bach alone. This page records the verification: what is asserted, what
the full chain gives, where it does not give the expected value and why, and whether Phase 2.5's exit criterion is met.

**Status: met (2026-10-17).** The assertions all pass, every deviation is documented with its exact actual value, and every deviation is a method limit with a roadmap
ID. The expected values are verified, and the chain reports both readings of a V to I6 and of a dominant before a section start. See "Exit criterion" at the end.

## What is asserted

The chain is the production chain, `analysis_chain_run` to the form stage on the piece's notes (`framework_v1_chain.h` loads the expected values, runs it and keeps the results for the checks): import, the piece's meter, adaptive segments, local key regions (with the file's key
signature events), chords identified in each region's key, Roman numerals, outer voices, the cadence adapter, and the L3.6 adapter (phrases, ideas, themes,
expansions). No stage is given an answer, and every default is the library's. (Bach 114's file declares 4/4 and the piece is 3/4, so a 3/4 meter replaces it, as in
every other reference-piece test. Chord identification reads each key region in its own mode: the major set in a major region, the diatonic triads and sevenths of natural, harmonic and melodic minor in a minor one.)

The expected values are `tests/fixtures/annotations/<piece>_v1_expected.txt`, one per piece, each citing its edition. They are **verified** (Michelle Opitz,
2026-10-17, R-092): read from the MIDI first, then checked against standard theory and a printed edition. Where the older annotation files (key regions, cadences,
phrase form) cover the same ground their values are copied, and `framework_v1_verification` fails if the two disagree, or if a phrase end has no required cadence
of its type within a measure. Every cadence list is complete for its scope; the phrase-form lists are what could be read with confidence.

A cadence line may end with `optional` (`cadence <m> <beat> <type> optional`): the music may or may not have it, so missing is no deviation, and found with its
type within one beat it is not an extra. The consistency check compares required cadences only. Mozart 545 m26 and m27 are optional (codettas before the closing
cadence at m28, R-095).

A `readings` line (`readings <m> <beat> <reading> <m> <beat> <reading> [prefer 1|2]`) states a cadence point with two competing readings (R-090, R-091), each a cadence
type word or `prolongation`. One cadence the chain reports must have these two as its primary and alternative, in either order, each arrival within 1 beat; with
`prefer`, that reading must also have the higher share. The cadence a `readings` line takes is not an extra and matches no `cadence` line. A `phrase-end` at the
measure of either reading passes when the chain's phrase end is at either reading's measure (within 1 measure).

Notes on the verified values:

- **Bach 114.** The file declares 4/4 and its note values are halved. Correctly notated, each strain is 16 measures of 3/4 and every cadence lands on beat 1. The
  verification keeps the file's numbering (R-092), so the measures and beats in the expected file and the deviation rows are the file's.
- **Mozart 545.** The recapitulation keys (C major for m79-101 and m124-146, half cadence on G at m85 and m130) are accepted and flagged for Michelle to
  re-verify. M79-85 repeats m6-12 note for note.

| Area | One check per | Tolerance |
|---|---|---|
| Keys | the home key (the whole scope scored as one window by the chain's own estimator); each key region; each key change | region edges within 1 measure; a change within 1 measure with its `KeyChangeExtent` |
| Cadences | each annotated cadence; each `readings` pair; each cadence the chain reports in the scope that is not annotated | arrival within 1 beat, `CadenceType` exactly (PAC and IAC differ) |
| Phrase form | each annotated phrase end; each annotated theme (its form and its two parts); each annotated expansion | within 1 measure |
| Thematic returns (Mozart 545, Clementi 36) | each annotated return of the main and secondary themes | starts within 1 measure, and "resembles" the original (which starts within 1 measure) |

Thematic returns use the kernel's `ThematicReturnDetection` on the L3.6 themes: each theme becomes a `ThemeOccurrence` and a `MotifWindow` (the contour, the
rhythm and the event ids of its soprano notes), and the detector's "resembles" relationships are read. No library change was needed to feed it.

## Results

Counts are checks that give the expected value / deviations (checks whose actual value is asserted in `expected_deviations.md`). Every check either passes or is a
deviation; none fails. As of the commit that wrote this page; `framework_v1_verification` prints them.

| Piece | Keys | Cadences | Phrase form | Thematic returns |
|---|---|---|---|---|
| Bach 114 | 7 / 3 | 2 / 7 | 3 / 7 | not asserted |
| Mozart 545 | 2 / 16 | 2 / 2 | 2 / 0 | 0 / 6 |
| Clementi 36 | 6 / 2 | 4 / 15 | 5 / 4 | 0 / 2 |
| Field 1 | 2 / 0 | 1 / 5 | 3 / 3 | not asserted |
| Field 5 | 1 / 1 | 1 / 0 | 0 / 0 (nothing annotated) | not asserted |
| **All five** | **18 / 22** | **10 / 29** | **13 / 14** | **0 / 8** |

41 of 114 checks give the expected value; 73 are deviations. Of the 29 cadence deviations, 13 are cadences the chain reports that are not annotated.

Two `readings` pairs pass (Clementi m12 and m35, V to I6, evaded and prolongation at 500 each). Two are deviations: Mozart m4 (the chain reads the arrival as a
root-position I over the Alberti bass, so no V to I6 is found; C5) and Clementi m23 (the chain finds the pair, but the half cadence's dominant arrives at b4.5, not b1;
C1, C2).

## Deviations

`tests/fixtures/framework-v1-corpus/expected_deviations.md` lists each of the 73 deviations (piece, area, location, expected, actual, cause) and the seven causes.
The tests read that file, so a row is an exact statement of what the chain does now: it breaks when the chain changes, either way.

| Cause | Deviations | What it is | Roadmap ID |
|---|---|---|---|
| C1 | 22 | key region edges set by 4-measure windows | L8.1 |
| C2 | 14 | closely related key read for a passage | L8.2 |
| C3 | 1 | `KeyChangeExtent` from region lengths, no cadence consulted | L8.3 |
| C4 | 6 | cadence arrival on a weak beat is not looked for (all of Bach 114's) | L8.4 |
| C5 | 48 | chords read from the sounding pitches: sparse, alternating or ornamented segments | L8.5 |
| C6 | 6 | a non-chord bass tone reads the chord as root position | L8.6 |
| C8 | 14 | L3.6 builds phrases, themes, expansions and returns only from the cadences it is given; re-verified in P1 B8, no L3.6 rule was found wrong, every row also names the upstream cadence cause | L8.7 |

(A deviation can have more than one cause, so the counts add to more than 73.) The chain reports both readings of a V to I6 and of a dominant before a section start,
and the expected files state both. C10 was removed in P1 B7.

## Bugs found and fixed

None in the library. The assertions found no output that is wrong for a reason that no documented limit covers, so no `fix` commit changes library code.
One defect in the new test was found and fixed: the thematic return glue leaked the arrays that `contour_create`, `rhythmic_pattern_create` and
`theme_occurrence_create` copy.

One observation, outside the Framework v1 path: the chain's `chain_subset_chord_analyze` anchors a Chord to the first event id of its sonority, and on
Bach 114's adaptive segments two segments share a first event id (a held note), so it would give two Chords one id. The chain does not use it; it identifies chords
with the weighted identifier, whose Chord ids come from the segment ids. Nothing here depends on it, and it was not changed.

## PR #36 and #37, re-verified

- **#36, Chord EntityID uniqueness.** On Bach 114 the chain's 149 Chord ids are all distinct (asserted in `framework_v1_verification`).
- **#37, thematic return minimum window.** On Bach 114, of the 138 sliding three-note windows of the soprano, 592 pairs share contour and rhythm, and the detector
  reports none of them as returns. Two identical windows of 70 notes (the first strain and its written-out repeat) are matched by one "resembles" link, so the
  floor is not a blanket block (asserted in `framework_v1_thematic_returns`). The floor of 4 events is the library's default and was not changed.

## Exit criterion

Date: 2026-10-17. The criterion is: all assertions pass, and every deviation is a documented method limit with a roadmap item attached (R-020).

- Passing checks: 41 of 114 (re-counted 2026-11-08 with the Clementi and Mozart key readings of Michelle's review of 2026-11-07, R-357). The other 73 are asserted deviations; none fails. Met.
- Deviations by cause: C1 22, C2 11, C3 1, C4 6, C5 48, C6 6, C8 14 (a deviation can name several causes). Every one is documented with its exact actual value. Met.
- Every cause is a method limit with a roadmap ID (L8.1 to L8.7, R-095). Met.
- The expected values are verified (2026-10-17, R-092). Met. The Mozart recapitulation keys are flagged for a re-check.

**Verdict: met.** Every remaining deviation is a method limit with a roadmap ID; not every check passes. To rerun the suite: `ctest -R framework_v1`. When a stage improves, a
deviation row that now passes fails the test and is removed; one whose actual value changes fails and is rewritten (`FRAMEWORK_V1_PRINT_DEVIATIONS=1` prints the rows).
