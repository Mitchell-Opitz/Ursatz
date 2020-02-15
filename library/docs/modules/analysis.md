# analysis

Layer L8 (`music-analysis`). Path `src/analysis/`, tests `tests/analysis/`. One of three mutually-restricted L8 siblings (with `transform` and `generation`); see `docs/modules/l8-notes.md` for the sibling rule and shared L8 notes.

## Provides

`Analyzer`, `AnalysisResult`, `FeatureExtraction`, and nine concrete analyzers (`IntervalAnalysis`, `ScaleAnalysis`, `ChordIdentification`, `KeyEstimation`, `VoiceLeadingAnalysis`, `CadenceDetection`, `CadenceClassification`, `MotifDetection`, `PhraseSegmentation`).

## Notes

An `Analyzer` never returns a bare conclusion; it derives owned `Interpretation`/`Evidence` objects (L6) into an `AnalysisResult`, which owns them (there is nowhere else for them to live once `analyze()` returns). `PitchIdentity` carries no tuning-system semantics, so no analyzer can derive its own measurements (semitone displacement, scale/chord/key fit, motion classification, boundary confidence) from raw pitch tokens; each takes that dimension as an already-computed input field (scale degrees, tonic-relative pitch classes, and so on).

**Real (wired to `frameworks/common-practice-minimal/`):**

- `ChordIdentification` -- given a Sonority's scale degrees, produces a real, populated `Chord` (L6) naming one of common-practice-minimal-v1's seven diatonic triads, or reports a failed analysis on no match. Single-answer/deterministic, matching the framework's own deterministic behavior.
- `KeyEstimation` -- given a region's tonic name and tonic-relative pitch-class collection, produces a real, populated `Key` (L6) naming major or natural minor under common-practice-minimal-v1, or reports a failed analysis on no match. Single-answer/deterministic, same as `ChordIdentification`.

Both build their real `Chord`/`Key` via the framework, then unwrap it with `chord_take_interpretation`/`key_take_interpretation` (L6) into the bare `Interpretation` `AnalysisResult` carries -- `AnalysisResult` only ever holds `Interpretation`/`Evidence` objects, never a `Chord`/`Key` wrapper.

**`CadenceDetection` / `CadenceClassification` -- real root-motion rules, explicit caller-supplied refinements, no false-confidence guesses:**

`CadenceDetection` (`analysis/cadence_detection.h`) is the generic half: given a region and an already-weighted `UncertaintyCandidate` set, it picks the top candidate and reports it as a single `Interpretation` targeting the region (an existing L5 `Cadence` shell, `structure/cadence.h`) -- the same generic-Interpretation pattern `ChordIdentification`/`Motif`/`Theme`/`PhraseOccurrence` use; `CadenceDetection` does not itself get a new named output type. It now optionally accepts a `Framework`, threading a real `FrameworkReference` (e.g. to common-practice-minimal) into the Interpretation when supplied.

`CadenceClassification` (`analysis/cadence_classification.h`) is the real classifier: it builds that candidate set from chord ROOT MOTION -- each chord's scale-degree root (1-7), the same real, already-computed dimension the caller supplies to `ChordIdentification` itself, not a re-parse of chord label strings (a seventh chord's label, e.g. `"dominant7"`, carries no root information at all). Its rules:

- V(7) -> I root motion: authentic-cadence candidate.
- IV -> I: plagal-cadence candidate.
- ends on V: half-cadence (HC) candidate.
- V -> vi: deceptive-cadence candidate.
- anything else: a single `"unclassified"` candidate at full weight -- no guess.

What's REAL vs. CALLER-SUPPLIED-REFINEMENT vs. UNCLASSIFIED, the same transparency standard `VoiceLeadingAnalysis`'s docs use:

- Real: the from/to root-degree motion driving which rule fires.
- Optional caller-supplied refinement (never required): `from_is_dominant_seventh` and Voice Leading's bass-motion classification (`analysis/voice_leading_analysis.h`) each only adjust the winning candidate's confidence tier within the authentic-cadence rule -- a caller with no Voice Leading data still gets a classification, just a lower-confidence one.
- Optional caller-supplied refinement that changes the LABEL, not just its weight: `soprano_resolves_to_tonic`, distinguishing PAC from IAC. This genuinely cannot be derived today -- no `PitchClass`/`TuningSystem` (`docs/modules/l8-notes.md`) means no real soprano-resolution computation exists anywhere in this codebase. When absent, the authentic-cadence candidate is reported as `"authentic (unclassified: PAC/IAC)"` rather than guessing PAC or IAC.

**Still placeholder/deferred (not touched by the common-practice-minimal wiring):**

- `PhraseSegmentation`, `VoiceLeadingAnalysis`, `ScaleAnalysis` -- unchanged; still report caller-supplied candidate sets rather than a framework-derived answer. `VoiceLeadingAnalysis` specifically has AnalysisRegression coverage pinning this packaging for parallel-fifth/parallel-octave motion and step/leap/same bass-motion shapes (`tests/analysis/test_voice_leading_analysis.c`) -- the classification itself is still caller-supplied, not derived.
- `MotifDetection` itself still just packages an already-determined `MotifDetectionOccurrence` set into `Interpretation`s (`analysis/motif_detection.h`), but its match-finding step (`analysis/motif_window_matching.h`) is now real, not caller-supplied: given caller-supplied `MotifWindow`s (each a `Contour` + `RhythmicPattern` + event-id list -- window construction and segmentation stay the caller's job, same boundary Contour/RhythmicPattern themselves already draw), it finds matches by exact equality of Contour direction sequence AND RhythmicPattern duration sequence. Matching on direction alone is transposition-invariant by construction, so this one rule covers both exact repetition and transposition without any pitch-interval math -- still blocked on `PitchClass`/`TuningSystem` (`docs/modules/l8-notes.md`) for anything beyond that. Two judgment calls flagged, not Registry-settled: a match is reported only when a window matches at least one other (a motif needs >=2 occurrences by definition), and every match is reported at a fixed 1/1 confidence (an exact match has no partial-credit dimension to score in v1). Inversion, retrograde, and fragmentation matching remain deferred entirely.
- Multi-candidate/confidence-weighted output for `ChordIdentification`/`KeyEstimation` -- deliberately out of scope for this pass; both analyzers are single-answer/deterministic today, matching the framework's own deterministic behavior. Planned as a later, separate refinement.
- Phrase Segmentation consuming `CadenceClassification`'s output to find phrase boundaries -- a separate, later branch.

## Dependencies

Depends on `identity` (L0), `core` (L1), `time`/`pitch` (L2), `events` (L3), `containers` (L4), `structure` (L5), all four L6 modules, both L7 modules, and (for `ChordIdentification`/`KeyEstimation` specifically) `frameworks/common-practice-minimal/` -- the first Framework extension module music-analysis depends on; see the architecture-gate note in `tests/analysis/test_architecture_analysis.c` for why that dependency is scoped to an exact header allowlist rather than a directory prefix. Never imports `transform` or `generation`. `CadenceDetection`/`CadenceClassification` take an optional `Framework` (L7) reference but never depend on `frameworks/common-practice-minimal/` vocabulary directly -- root motion is caller-supplied as plain scale-degree integers, not framework-specific chord labels.
