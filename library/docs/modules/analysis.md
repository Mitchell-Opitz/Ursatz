# analysis

Layer L8 (`music-analysis`). Path `src/analysis/`, tests `tests/analysis/`. One of three mutually-restricted L8 siblings (with `transform` and `generation`); see `docs/modules/l8-notes.md` for the sibling rule and shared L8 notes.

## Provides

`Analyzer`, `AnalysisResult`, `FeatureExtraction`, and eight concrete analyzers (`IntervalAnalysis`, `ScaleAnalysis`, `ChordIdentification`, `KeyEstimation`, `VoiceLeadingAnalysis`, `CadenceDetection`, `MotifDetection`, `PhraseSegmentation`).

## Notes

An `Analyzer` never returns a bare conclusion; it derives owned `Interpretation`/`Evidence` objects (L6) into an `AnalysisResult`, which owns them (there is nowhere else for them to live once `analyze()` returns). Every concrete analyzer follows the same division. The caller supplies the already-measured dimension; the analyzer packages it. `PitchIdentity` carries no tuning-system semantics, so none of the eight analyzers can derive their own measurements (semitone displacement, scale/chord/key fit, motion classification, boundary confidence) from raw pitch tokens; each takes that dimension as an already-computed input field and reports it through the Interpretation/Evidence/Uncertainty architecture.

## Dependencies

Depends on `identity` (L0), `core` (L1), `time`/`pitch` (L2), `events` (L3), `containers` (L4), `structure` (L5), all four L6 modules, and both L7 modules only. Never imports `transform` or `generation`.
