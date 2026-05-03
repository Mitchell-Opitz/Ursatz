# Framework v1 verification

Framework v1's finite endpoint is the five-piece reference set in `tests/fixtures/framework-v1-corpus/`: Bach, Minuet in G, BWV Anh. 114; Clementi, Sonatina
Op. 36 No. 1, first movement; Field, Nocturne No. 1; Field, Nocturne No. 5; Mozart, Sonata K. 545, first movement. Until this suite existed, `CorpusRegression`
checked only that the five files register, and a manual pass in the GUI had looked at Bach alone. This page records the verification: what is asserted, what
the full chain gives, where it does not give the expected value and why, and whether Phase 2.5's exit criterion is met.

**Status: not met.** The assertions all pass, every deviation is documented with its exact actual value, and every deviation has a cause and a named roadmap
item. But the expected values are drafts that were never checked against the editions, ten roadmap items have no number yet, and four deviations have a difference
of convention, not a method limit, among their causes. The reasons are listed at the end.

## What is asserted

The chain is the full chain as `framework_v1_chain.h` runs it: import, the piece's meter, adaptive segments, local key regions (with the file's key
signature events), chords identified in each region's key, Roman numerals, outer voices, the cadence adapter, and the L3.6 adapter (phrases, ideas, themes,
expansions). No stage is given an answer, and every default is the library's. (Bach 114's file declares 4/4 and the piece is 3/4, so a 3/4 meter replaces it, as in
every other reference-piece test. Chord identification reads each key region in its own mode: the major set in a major region, the diatonic triads and sevenths of natural, harmonic and melodic minor in a minor one.)

The expected values are `tests/fixtures/annotations/<piece>_v1_expected.txt`, one per piece, each citing its edition and marked unverified. Where the older
annotation files (key regions, cadences, phrase form) cover the same ground their values are copied, and `framework_v1_verification` fails if the two disagree,
or if a phrase end has no cadence of its type within a measure. Every cadence list is complete for its scope; the phrase-form lists are what could be read with
confidence.

| Area | One check per | Tolerance |
|---|---|---|
| Keys | the home key (the whole scope scored as one window by the chain's own estimator); each key region; each key change | region edges within 1 measure; a change within 1 measure with its `KeyChangeExtent` |
| Cadences | each annotated cadence; each cadence the chain reports in the scope that is not annotated | arrival within 1 beat, `CadenceType` exactly (PAC and IAC differ) |
| Phrase form | each annotated phrase end; each annotated theme (its form and its two parts); each annotated expansion | within 1 measure |
| Thematic returns (Mozart 545, Clementi 36) | each annotated return of the main and secondary themes | starts within 1 measure, and "resembles" the original (which starts within 1 measure) |

Thematic returns use the kernel's `ThematicReturnDetection` on the L3.6 themes: each theme becomes a `ThemeOccurrence` and a `MotifWindow` (the contour, the
rhythm and the event ids of its soprano notes), and the detector's "resembles" relationships are read. No library change was needed to feed it.

## Results

Counts are checks that give the expected value / deviations (checks whose actual value is asserted in `expected_deviations.md`). Every check either passes or is a
deviation; none fails. As of the commit that wrote this page; `framework_v1_verification` prints them.

| Piece | Keys | Cadences | Phrase form | Thematic returns |
|---|---|---|---|---|
| Bach 114 | 4 / 6 | 0 / 10 | 4 / 6 | not asserted |
| Mozart 545 | 4 / 10 | 2 / 5 | 1 / 1 | 1 / 5 |
| Clementi 36 | 3 / 5 | 2 / 17 | 4 / 5 | 0 / 2 |
| Field 1 | 2 / 0 | 1 / 6 | 3 / 3 | not asserted |
| Field 5 | 1 / 1 | 1 / 0 | 0 / 0 (nothing annotated) | not asserted |
| **All five** | **14 / 22** | **6 / 38** | **12 / 15** | **1 / 7** |

33 of 115 checks give the expected value; 82 are deviations. Of the 38 cadence deviations, 21 are cadences the chain reports that are not annotated.

## Deviations

`tests/fixtures/framework-v1-corpus/expected_deviations.md` lists each of the 82 deviations (piece, area, location, expected, actual, cause) and the ten causes.
The tests read that file, so a row is an exact statement of what the chain does now: it breaks when the chain changes, either way.

| Cause | Deviations | What it is | Roadmap item (no number yet) |
|---|---|---|---|
| C1 | 23 | key region edges set by 4-measure windows | key regions placed on harmonic evidence |
| C2 | 11 | closely related key read for a passage | harmony-aware key estimation |
| C3 | 1 | `KeyChangeExtent` from region lengths, no cadence consulted | extents confirmed by cadences |
| C4 | 8 | cadence arrival on a weak beat is not looked for (all of Bach 114's) | cadence arrival on weak beats |
| C5 | 26 | chords read from the sounding pitches: sparse, alternating or ornamented segments | chord identification with context |
| C6 | 5 | a non-chord bass tone reads the chord as root position | inversion from the chord's bass tone |
| C7 | 3 | convention: V to I6 is SUBVERTED in the adapter, a prolongation in the drafts | check the drafts against the editions |
| C8 | 22 | L3.6 phrases, themes and expansions follow the cadences it is given | re-verify L3.6 after the cadence items |
| C9 | 1 | convention: a dominant resolving into the recapitulation | cadences across a formal boundary |
| C10 | 7 | return detection compares whole themes for exact contour and rhythm | matching inside themes and with variation |

(A deviation can have more than one cause, so the counts add to more than 82.)

## Bugs found and fixed

None in the library. The assertions found no output that is wrong for a reason that no documented limit covers, so no `fix` commit changes library code.
One defect in the new test was found and fixed: the thematic return glue leaked the arrays that `contour_create`, `rhythmic_pattern_create` and
`theme_occurrence_create` copy.

One observation, outside the Framework v1 path: the kernel's `chord_identification_analyze` anchors a Chord to the first event id of its sonority, and on
Bach 114's adaptive segments two segments share a first event id (a held note), so it would give two Chords one id. The chain does not use it; it identifies chords
with the weighted identifier, whose Chord ids come from the segment ids. Nothing here depends on it, and it was not changed.

## PR #36 and #37, re-verified

- **#36, Chord EntityID uniqueness.** On Bach 114 the chain's 149 Chord ids are all distinct (asserted in `framework_v1_verification`).
- **#37, thematic return minimum window.** On Bach 114, of the 138 sliding three-note windows of the soprano, 592 pairs share contour and rhythm, and the detector
  reports none of them as returns. Two identical windows of 70 notes (the first strain and its written-out repeat) are matched by one "resembles" link, so the
  floor is not a blanket block (asserted in `framework_v1_thematic_returns`). The floor of 4 events is the library's default and was not changed.

## Is the exit criterion met?

The criterion is: all assertions pass, and every deviation is a documented method limit with a roadmap item attached.

- All 115 checks pass or are asserted deviations. Met.
- Every deviation is documented, with its exact actual value and a cause. Met.
- Every cause has a named roadmap item, but **none has a roadmap number yet** (the Registry assigns them). Not met.
- Two causes (C7, C9; four deviations name one) are differences of convention between the drafts and the library, not method limits. Not met until the drafts are checked.
- The expected values are **unverified drafts**. A check against the editions may change them, and so move deviations in either direction. Not met.

So the status is **not met**. The step that would settle it is a check of the five `_v1_expected.txt` files against the editions, followed by numbering the ten
roadmap items. To rerun the suite: `ctest -R framework_v1`. When a stage improves, a deviation row that now passes fails the test and is removed; one whose
actual value changes fails and is rewritten (`FRAMEWORK_V1_PRINT_DEVIATIONS=1` prints the rows).
