# analysis

[Docs index](../README.md): the Ursatz docs by topic.

Layer L8 (`music-analysis`). Path `analysis/motive/`, tests `tests/analysis/`. The only L8 module (`generation` left the library in P55); see `docs/modules/l8-notes.md` for the sibling rule and shared L8 notes.

## Provides

The phrase boundary loop (`PhraseBoundaryDetection`), `FormStructureDetection` and `ThematicReturnDetection`, plus the motive, outer voice and voice separation modules listed below.

## Notes

The kernel has no `Analyzer` type, no `AnalysisResult` and no `Evidence` (`analysis/analyzer.h`, `period_detection.h` and `interpretation/evidence.h` were deleted, P55, since nothing called them): the modules below return plain results and `Relationship`s. `PitchIdentity` carries no tuning-system semantics, so no module here can derive its own measurements (semitone displacement, scale/chord/key fit, motion classification, boundary confidence) from raw pitch tokens; each takes that dimension as an already-computed input field (scale degrees, tonic-relative pitch classes, and so on).


The analyzers are documented by topic:

- [Common-practice-minimal analysis](common-practice-minimal-analysis.md): weighted chord identification, outer scale degrees, local key regions.
- [Analysis: voices](analysis-voices.md): outer-voice extraction, soprano at every attack, voice separation.
- [Analysis: phrases and form](analysis-phrase-form.md): phrase boundaries, period, form structure, thematic return.
- [Motive link](motive-link.md): choosing and grouping linked motives across pieces.

**Real (wired to `theory/tonality/`):**

- **Progression patterns** (`analysis/progression_patterns.h`) -- the chord progressions a piece repeats: held chords merged, runs of 2 to 6 chords inside blocks (a NULL label or group -1 ends a block, a change of group ends one), patterns that occur at least twice, a pattern dropped when a longer one with as many occurrences contains it, ordered by occurrences, length, then earliest. Labels are compared as text; no theory. Port of Analyzer `progression_pipeline.c` without file-scope state.

**Still placeholder/deferred (not touched by the common-practice-minimal wiring):**

- Owners ([analytic-job-owners.md](../analytic-job-owners.md)): the phrase boundary loop is an internal step of the form bridge, not the phrase segmentation owner (`standard_form_segment_phrases`); `ChordIdentification` and `KeyEstimation` are not the owners of chord identification (`common_practice_identify_weighted_sequence`) or key identification (`key_regions_estimate`) and live in the chain (`chain/README.md`); `MotifWindowMatching` is the exact contour and rhythm primitive under the motive matching owner (`motive_variant_recognize`).
- `MotifDetection` (the analyzer) was retired in P28 B4 (R-287): nothing called it. Its match-finding step (`analysis/motif_window_matching.h`) stays, with its own `MotifWindowOccurrence` result: given caller-supplied `MotifWindow`s (each a `Contour` + `RhythmicPattern` + event-id list -- window construction and segmentation stay the caller's job, same boundary Contour/RhythmicPattern themselves already draw), it finds matches by exact equality of Contour direction sequence AND RhythmicPattern duration sequence. Matching on direction alone is transposition-invariant by construction, so this one rule covers both exact repetition and transposition without any pitch-interval math. A match is reported only when a window matches at least one other (a motif needs >=2 occurrences by definition), and every match is reported at a fixed 1/1 confidence. Motive recognition of every other kind is `motive_variant_recognize` (`docs/modules/motive-variant-recognition.md`).
- Multi-candidate/confidence-weighted output for `ChordIdentification`/`KeyEstimation` -- deliberately out of scope for this pass; both analyzers are single-answer/deterministic today, matching the framework's own deterministic behavior. Planned as a later, separate refinement.

## Dependencies

Depends on `identity` (L0), `core` (L1), `time`/`pitch` (L2), `events` (L3), `containers` (L4), `structure` (L5), all four L6 modules, both L7 modules, and a scoped allowlist of common-practice-minimal vocabulary headers (see the `analysis` row of `cmake/layer-table.txt`); `ChordIdentification` and `KeyEstimation` no longer live here and depend on the framework from the chain. Never imports `transform`.
