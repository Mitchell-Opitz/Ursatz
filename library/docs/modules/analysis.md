# analysis

Layer L8 (`music-analysis`). Path `src/analysis/`, tests `tests/analysis/`. One of three mutually-restricted L8 siblings (with `transform` and `generation`); see `docs/modules/l8-notes.md` for the sibling rule and shared L8 notes.

## Provides

`Analyzer`, `AnalysisResult`, `FeatureExtraction`, and eight concrete analyzers (`IntervalAnalysis`, `ScaleAnalysis`, `ChordIdentification`, `KeyEstimation`, `VoiceLeadingAnalysis`, `CadenceDetection`, `MotifDetection`, `PhraseSegmentation`).

## Notes

An `Analyzer` never returns a bare conclusion; it derives owned `Interpretation`/`Evidence` objects (L6) into an `AnalysisResult`, which owns them (there is nowhere else for them to live once `analyze()` returns). `PitchIdentity` carries no tuning-system semantics, so no analyzer can derive its own measurements (semitone displacement, scale/chord/key fit, motion classification, boundary confidence) from raw pitch tokens; each takes that dimension as an already-computed input field (scale degrees, tonic-relative pitch classes, and so on).

**Real (wired to `frameworks/common-practice-minimal/`):**

- `ChordIdentification` -- given a Sonority's scale degrees, produces a real, populated `Chord` (L6) naming one of common-practice-minimal-v1's seven diatonic triads, or reports a failed analysis on no match. Single-answer/deterministic, matching the framework's own deterministic behavior.
- `KeyEstimation` -- given a region's tonic name and tonic-relative pitch-class collection, produces a real, populated `Key` (L6) naming major or natural minor under common-practice-minimal-v1, or reports a failed analysis on no match. Single-answer/deterministic, same as `ChordIdentification`.

Both build their real `Chord`/`Key` via the framework, then unwrap it with `chord_take_interpretation`/`key_take_interpretation` (L6) into the bare `Interpretation` `AnalysisResult` carries -- `AnalysisResult` only ever holds `Interpretation`/`Evidence` objects, never a `Chord`/`Key` wrapper.

**Still placeholder/deferred (not touched by the common-practice-minimal wiring):**

- `MotifDetection`, `PhraseSegmentation`, `VoiceLeadingAnalysis`, `ScaleAnalysis` -- unchanged; still report caller-supplied candidate sets rather than a framework-derived answer.
- `CadenceDetection` -- deferred. Producing a real `Cadence` (L5 structural shell, `structure/cadence.h`) from `ChordIdentification`/`KeyEstimation` output needs harmonic-motion pattern logic (e.g. recognizing a V-I motion as an authentic cadence, IV-I as plagal, V-vi as deceptive) that common-practice-minimal-v1 does not yet define -- meaningfully more than a direct consequence of the Chord/Key wiring above, so it stays out of scope for this pass.
- Multi-candidate/confidence-weighted output for `ChordIdentification`/`KeyEstimation` -- deliberately out of scope for this pass; both analyzers are single-answer/deterministic today, matching the framework's own deterministic behavior. Planned as a later, separate refinement.

## Dependencies

Depends on `identity` (L0), `core` (L1), `time`/`pitch` (L2), `events` (L3), `containers` (L4), `structure` (L5), all four L6 modules, both L7 modules, and (for `ChordIdentification`/`KeyEstimation` specifically) `frameworks/common-practice-minimal/` -- the first Framework extension module music-analysis depends on; see the architecture-gate note in `tests/analysis/test_architecture_analysis.c` for why that dependency is scoped to an exact header allowlist rather than a directory prefix. Never imports `transform` or `generation`.
