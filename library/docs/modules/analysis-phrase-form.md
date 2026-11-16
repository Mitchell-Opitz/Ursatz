# Analysis: phrases and form

PhraseBoundaryDetection, PeriodDetection, FormStructureDetection and ThematicReturnDetection in the kernel `analysis` module. [Docs index](../README.md)

## PhraseBoundaryDetection

`phrase_boundary_detect` (`analysis/phrase_boundary_detection.h`) is a loop over points, not an analyzer. Given an array of `PhraseBoundaryDetectionPoint` (`has_cadence`, and `has_discontinuity` for a Contour or RhythmicPattern break such as a rest or a clear directional or rhythmic change, both supplied by the caller), it writes `is_boundary` per point and, when the caller passes one, the `PhraseBoundarySignal` that made each boundary:

- Primary signal: any detected cadence at a point is a boundary, whatever its type.
- Secondary signal: a contour or rhythm discontinuity only counts as a boundary once `max_length_without_cadence` points (0 selects `PHRASE_BOUNDARY_DETECTION_DEFAULT_MAX_LENGTH` = 8, a stated default, not Registry-settled) have elapsed since the last boundary with no cadence among them.

The chain's form bridge (`chain/ursatz_chain/form_bridge.h`) fills the points from the form input and calls `phrase_boundary_detect` for the boundary detector `standard-form` takes by injection (`StandardFormBoundaryDetector`). The loop builds no `Interpretation` and no `PhraseOccurrence`; the phrases come from `standard_form_segment_phrases`.

Out of scope: pitch or interval math beyond what Contour and RhythmicPattern provide, and L9 query infrastructure (`PatternMatching`, `SimilarityModel`, `SearchResult`).

## PeriodDetection

A Period structure has no named type in the System Registry. `PeriodDetection` (`analysis/period_detection.h`) adds no new type: it emits a generic `Interpretation` whose claim describes the antecedent and consequent relationship.

Given an ordered sequence of `PhraseOccurrence`s, each annotated with the `CadenceType` that triggered its boundary, it identifies a period pair between two consecutive phrases when the first's cadence ranks strictly weaker than the second's. The kernel does not own that order: the caller injects a ranking function (`PeriodDetectionRankFn`, `int (*)(CadenceType)`, larger is stronger, `<= 0` is not rankable) in `PeriodDetectionInput.rank` and in `period_detection_build_relationships`. `frameworks/standard-harmony` owns the order (`harmony_cadence_strength`, weakest to strongest: half < deceptive < plagal < authentic (unspecified) < imperfect authentic < perfect authentic; a subverted cadence ranks 0 and `CADENCE_TYPE_UNKNOWN` is unranked), so a caller passes it in:

```c
PeriodDetectionInput input = {entries, entry_count, harmony_cadence_strength};
```

Analysis code never includes a framework header. A pair where either phrase's cadence is unranked (`CADENCE_TYPE_UNKNOWN`, subverted, or invalid) is skipped entirely, with no Interpretation and no guess.

An `Interpretation` has a single `EntityID` target, but a period pair is two `PhraseOccurrence`s, so the link is a `Relationship` (L6, `relationship/relationship.h`): `source_id` is the antecedent `PhraseOccurrence` id, `predicate_name` is `"precedes"`, `target_id` is the consequent id. `period_detection_analyze` emits one `Interpretation` per pair targeting that `Relationship`'s id, derived deterministically from the phrase-id pair. It carries a `PeriodDetectionClaim` (both phrase ids and cadence types) and a 1/1 scalar-confidence `Uncertainty`. The kernel builds no `Relationship` object for it.

## FormStructureDetection

`Section` (`containers/section.h`, L4) is a real, non-transient container: `id` plus `phrase_ids`. `FormStructureDetection` (`analysis/form_structure_detection.h`) mints a `Section` per identified group directly with `section_create`; nothing is transient.

Given the same ordered, cadence-annotated `PhraseOccurrence` sequence `PeriodDetection` consumes, it answers a coarser question: which cadences close a Section rather than a phrase. Only `CADENCE_TYPE_PERFECT_AUTHENTIC` and `CADENCE_TYPE_IMPERFECT_AUTHENTIC` count as structural, because they are the only two cadence types that achieve tonic resolution. This is a stated default, narrower than `PeriodDetection`'s ranking, and not Registry-settled. `CADENCE_TYPE_AUTHENTIC_UNSPECIFIED`, plagal, deceptive and half cadences work as internal phrase punctuation but never as section-ending closure, and an ambiguous PAC or IAC call is never promoted to structural. A structural cadence at entry `i` (other than the last entry) closes a Section spanning the entries since the previous boundary through `i`. A trailing run of entries after the last boundary that never reaches another structural cadence does not become a Section. With no structural cadence anywhere there is no boundary, no Section and no output.

Consecutive Sections are linked as `PeriodDetection` links phrases, by one `Relationship` each (`source_id` is the earlier Section id, `predicate_name` is `"precedes"`, `target_id` is the later Section id). `form_structure_detection_build_sections` and `form_structure_detection_build_relationships` mint the `Section`s and the `Relationship`s from the same boundary computation, so the ids match; there is no `Interpretation` and no `AnalysisResult` for it.

## ThematicReturnDetection

`ThematicReturnDetection` (`analysis/thematic_return_detection.h`) adds no new named type: a theme occurrence is an id and a window of event ids (`ThematicReturnDetectionEntry`) the caller holds.

It applies `MotifWindowMatching`'s comparison rule at Theme granularity. The rule is granularity-agnostic: it compares whatever `Contour` and `RhythmicPattern` the caller built for a window. A theme occurrence's region (possibly several phrases) is such a window, and the caller builds one `MotifWindow` per theme occurrence as for any other `MotifWindow`. `ThematicReturnDetection` implements the comparison itself rather than calling `motif_window_matching_find`, because that function reports only a per-window "matched something" flag, and a return needs the matching pair (which later theme occurrence recalls which earlier one). `MotifWindowMatching`'s public surface is untouched.

The predicate is `"resembles"`, not `"transformed_from"`: a return asserts similarity with no claimed direction of derivation. For every ordered pair of theme occurrence entries (earlier, later) whose windows match, it builds one `Relationship` (`source_id` is the later theme occurrence id, `predicate_name` is `"resembles"`, `target_id` is the earlier id; the claim is about the Theme material, not the Section it sits in) and nothing else: there is no `Interpretation` and no `AnalysisResult`. No matching windows means no output (`thematic_return_detection_build_relationships_configured` yields no `Relationship`).

Minimum window length: any window below `THEMATIC_RETURN_DETECTION_MIN_WINDOW_LENGTH` (4 events) is excluded from matching, because short generic windows (for example a 3-event stepwise run) match across unrelated occurrences. It is a fixed constant, local to `ThematicReturnDetection`, like `PhraseBoundaryDetection`'s default of 8. 4 is a stated default, not Registry-settled, and may move as more of the corpus is checked. `motif_window_matching_find` has no such floor; its callers hand it short, curated candidate windows.

Windowed and varied matching: a return is not a whole theme repeating another note for note. For each pair of theme occurrence windows the detector slides a stretch of `window_length` events (default 6, or the shorter theme's length when that is less, never below 4) across both and compares every pair of stretches with `motif_window_matching_compare` (`MotifWindowMatching`'s rule, returning scores). The contour score is the share of equal up, down and same directions, the rhythm score the share of equal durations, each 0 to 1. The similarity of a stretch pair is the lower of the two, and the best stretch pair is the match. A pair is a return when its similarity reaches `threshold` (default 0.8). A later theme that restates only the opening idea of an earlier one is found wherever the idea sits, a transposed statement matches (contour direction is transposition-invariant), and one changed duration or direction in six still matches while a different rhythm or shape does not. Both settings live in `ThematicReturnDetectionConfig` (`thematic_return_detection_default_config`, the `config` argument of `thematic_return_detection_build_relationships_configured`); a NULL config means the defaults. The default threshold is a stated default, not tuned: the Framework v1 returns are blocked upstream (no L3.6 theme exists where the annotated returns are, C8), so the harness shows no threshold separation (identical results from 0.5 to 1.0). The evidence is a `thematic-return-similarity` `Value` in the `qualifiers` of the "resembles" `Relationship` (`ThematicReturnSimilarity`: contour score, rhythm score, similarity, the two stretch offsets and the stretch length).

## The soprano of the form input

The phrase, idea, theme and expansion analysis (`frameworks/standard-form/analysis/`) reads a soprano line, the cadences and the segments that `chain_build_form_input` (`chain/`) builds. The soprano is the outer line on the voice segments (`analysis_chain_voice_segments`, `analysis_chain_outer_voices`), one note per soprano note, consecutive voice segments on one note id being one note (R-377). It is not the outer lines sampled at each harmonic span's start, which keep one note per span and lose the rest of the melody. The harmonic segments and the cadences stay on the chord segments.

## The phrase grid

Phrase first (R-389, R-391): a piece is cut into units of 4, 8 or 16 measures before any cadence is trusted, and the cadences then confirm the units' ends. `standard_form_find_phrase_grid` (`frameworks/standard-form/analysis/analysis_phrase_grid.h`) finds the grid from the melody (one note per attack) and the candidate cadences (the found ones, both readings of a two-reading cadence, and the half readings). The chain calls it in its final pass (below).

Evidence of a measure m: the cadence arriving in m or held into it (1.0 for a perfect authentic, 0.8 for an imperfect authentic, an unspecified authentic or a half cadence), 0.3 when the last melody note starting in m lasts at least half the measure, 0.3 when no note sounds in m's last half beat, and 0.3 when the notes of m + 1 and m + 2 (at least 6) repeat the onsets and semitone steps of an earlier pair starting at least 5 measures before.

A unit [f, e] scores evidence(e) - 0.3, minus 0.6 when its length is not 4, 8 or 16, minus 0.1 when it is elided, plus the cadence evidence at its half measure when it is 8 or 16 long (an antecedent ends there). A unit is 2 to 16 measures long, a pickup counts in no length, and a unit starts on the previous unit's last measure (an elision) only when an authentic cadence arrives there. The grid is the tiling of the highest total, found by dynamic programming over (start measure, elided); of equal totals the one reached first wins. The constants are measured, never fitted to a piece.

`standard_form_segment_phrases_on_grid` (`analysis_phrase.h`) ends a phrase at a unit's last or half measure that holds a goal cadence (at the point's measure, not the arrival chord's end), and at a unit's last measure without one only when the phrase would pass 16 measures or no unit follows (the discontinuity signal, which on the grid is the 16-measure cap). `standard_form_identify_themes_on_grid` (`analysis_theme.h`) pairs two phrases into one theme only when the first ends on a unit's half measure and the second on that unit's last. One-phrase themes are unchanged. A last note held over the barline into a measure where no note starts belongs to the last phrase, which ends with it (the grid itself ends at the last note start); `tests/accuracy/test_chorale_form.c` runs every corpus chorale through the form stage (R-396).

### The chain's use

`analysis_chain_run` (`chain/`) always uses the grid in the form stage of its final pass, the second pass of the key refinement (the only pass without one). The grid is read from `chain_build_grid_input`: the note-level soprano (the highest sounding note at each attack, as `chain_build_motive_input` takes it) and every cadence of the pass and every half reading, each arriving over the window of its first arrival segment only, not the run. Reading the grid from the form input's beat-level line misplaces Clementi 36 (1-8, 8-15, 16-19, 20-24, 25-28, 29-38). The cadences claimed at the grid's points replace the pass's cadences, and the phrases (`standard_form_segment_phrases_on_grid`) and themes (`standard_form_identify_themes_on_grid`) follow. The first pass of the refinement keeps the cadence-ended phrases, so the key regions do not move. `tests/analysis/test_reference_phrases.c` asserts the readings of Clementi 36 (grid 1-8, 8-15, 16-23, 24-31, 32-38; the cadences HC m4, IAC m8, HC m23 with the PAC m24 as its alternative, HC m27; the period 1-8) and Mozart 545 (phrases 1-12 and 13-28), and reports the cadences the chain misses with their causes (R-393).
