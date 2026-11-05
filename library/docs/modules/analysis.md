# analysis

[Docs index](../README.md): the Ursatz docs by topic.

Layer L8 (`music-analysis`). Path `src/analysis/`, tests `tests/analysis/`. One of two mutually-restricted L8 siblings (with `generation`); see `docs/modules/l8-notes.md` for the sibling rule and shared L8 notes.

## Provides

`Analyzer`, `AnalysisResult`, `FeatureExtraction`, and twelve concrete analyzers (`IntervalAnalysis`, `ScaleAnalysis`, `VoiceLeadingAnalysis`, `CadenceDetection`, `CadenceClassification`, `PhraseSegmentation`, `PhraseBoundaryDetection`, `PeriodDetection`, `SentenceDetection`, `FormStructureDetection`, `ThematicReturnDetection`, plus the `SentenceContinuationMatching` procedure it consumes).

## Notes

An `Analyzer` never returns a bare conclusion; it derives owned `Interpretation`/`Evidence` objects (L6) into an `AnalysisResult`, which owns them (there is nowhere else for them to live once `analyze()` returns). `PitchIdentity` carries no tuning-system semantics, so no analyzer can derive its own measurements (semitone displacement, scale/chord/key fit, motion classification, boundary confidence) from raw pitch tokens; each takes that dimension as an already-computed input field (scale degrees, tonic-relative pitch classes, and so on).


The analyzers are documented by topic:

- [Common-practice-minimal analysis](common-practice-minimal-analysis.md): weighted chord identification, outer scale degrees, local key regions.
- [Analysis: voices](analysis-voices.md): outer-voice extraction, soprano at every attack, voice separation.
- [Analysis: cadences](analysis-cadence.md): CadenceDetection and CadenceClassification.
- [Analysis: phrases and form](analysis-phrase-form.md): phrase boundaries, period, sentence, form structure, thematic return.
- [Motive link](motive-link.md): choosing and grouping linked motives across pieces.

**Real (wired to `frameworks/common-practice-minimal/`):**

- **Progression patterns** (`analysis/progression_patterns.h`) -- the chord progressions a piece repeats: held chords merged, runs of 2 to 6 chords inside blocks (a NULL label or group -1 ends a block, a change of group ends one), patterns that occur at least twice, a pattern dropped when a longer one with as many occurrences contains it, ordered by occurrences, length, then earliest. Labels are compared as text; no theory. Port of Analyzer `progression_pipeline.c` without file-scope state.

## KeyEstimation

Both build their real `Chord`/`Key` via the framework, then unwrap it with `chord_take_interpretation`/`key_take_interpretation` (L6) into the bare `Interpretation` `AnalysisResult` carries -- `AnalysisResult` only ever holds `Interpretation`/`Evidence` objects, never a `Chord`/`Key` wrapper.

**Still placeholder/deferred (not touched by the common-practice-minimal wiring):**

- Owners ([analytic-job-owners.md](../analytic-job-owners.md)): `CadenceDetection` and `CadenceClassification` package caller candidates and are not the cadence detection owner (`harmony_analysis_find_cadences`); `VoiceLeadingAnalysis` packages caller-supplied motions and is not the motion classification owner (`standard_counterpoint_classify_motions`); `PhraseSegmentation` is an internal step of the kernel boundary detector, not the phrase segmentation owner (`standard_form_segment_phrases`); `ChordIdentification` and `KeyEstimation` are not the owners of chord identification (`common_practice_identify_weighted_sequence`) or key identification (`key_regions_estimate`) and live in the chain (`chain/README.md`); `MotifWindowMatching` is the exact contour and rhythm primitive under the motive matching owner (`motive_variant_recognize`).
- `VoiceLeadingAnalysis`, `ScaleAnalysis` -- unchanged; still report caller-supplied candidate sets rather than a framework-derived answer. `VoiceLeadingAnalysis` specifically has AnalysisRegression coverage pinning this packaging for parallel-fifth/parallel-octave motion and step/leap/same bass-motion shapes (`tests/analysis/test_voice_leading_analysis.c`) -- the classification itself is still caller-supplied, not derived.
- `MotifDetection` (the analyzer) was retired in P28 B4 (R-287): nothing called it. Its match-finding step (`analysis/motif_window_matching.h`) stays, with its own `MotifWindowOccurrence` result: given caller-supplied `MotifWindow`s (each a `Contour` + `RhythmicPattern` + event-id list -- window construction and segmentation stay the caller's job, same boundary Contour/RhythmicPattern themselves already draw), it finds matches by exact equality of Contour direction sequence AND RhythmicPattern duration sequence. Matching on direction alone is transposition-invariant by construction, so this one rule covers both exact repetition and transposition without any pitch-interval math. A match is reported only when a window matches at least one other (a motif needs >=2 occurrences by definition), and every match is reported at a fixed 1/1 confidence. Motive recognition of every other kind is `motive_variant_recognize` (`docs/modules/motive-variant-recognition.md`).
- Multi-candidate/confidence-weighted output for `ChordIdentification`/`KeyEstimation` -- deliberately out of scope for this pass; both analyzers are single-answer/deterministic today, matching the framework's own deterministic behavior. Planned as a later, separate refinement.

## Dependencies

Depends on `identity` (L0), `core` (L1), `time`/`pitch` (L2), `events` (L3), `containers` (L4), `structure` (L5), all four L6 modules, both L7 modules, and a scoped allowlist of common-practice-minimal vocabulary headers (see `tests/analysis/test_architecture_analysis.c`); `ChordIdentification` and `KeyEstimation` no longer live here and depend on the framework from the chain. Never imports `transform` or `generation`. `CadenceDetection`/`CadenceClassification` take an optional `Framework` (L7) reference but never depend on `frameworks/common-practice-minimal/` vocabulary directly -- root motion is caller-supplied as plain scale-degree integers, not framework-specific chord labels.
