# L8 shared notes

`transform`, `analysis`, and `generation` are built together against Dependency & Layer Spec v2 §2-§3. Like L7's `theory`/`rules` split, but stricter. All three are mutually sideways-restricted the same way L6's four siblings are, with no `transform`-`analysis`-`generation` import in any direction. Each depends on L0-L7 only, never on a sibling. Each module's architecture test enforces this by allowlisting only L0-L7 directories plus its own.

This is why `generation/candidate_evaluator.h`'s `CandidateEvaluation` cannot reuse `analysis/analyzer.h`'s `AnalysisResult`, even though both hold the identical Interpretation/Evidence shape. See `docs/modules/generation.md`.

## Deferred, flagged rather than built

Per the Registry, `Chord` and `Key` belong to `interpretation` (L6), not L8. `ChordIdentification` and `KeyEstimation` identify a chord/key and report it as a framework-relative `Interpretation`, but neither constructs a `Chord` or `Key` object; there is nothing at L6 yet to construct. This mirrors `CadenceDetection` not constructing a `Cadence` and `MotifDetection` not persisting the `MotifOccurrence` it validates. Both report by `Interpretation` and let a downstream caller build the real L5/L6 object from the claim. Building `Chord`/`Key` themselves is L6 follow-up work.

## Out of scope for L8 entirely, not missing

Actual I/O format adapters (MIDI/MusicXML/MEI/ABC, L9); corpus ingestion/statistics itself (L9, and separately blocked from being imported here regardless, per the L7 open issue in `docs/modules/theory.md`; `CandidateEvaluator` consuming a `StatisticalModel` is fine, that is L8 to L7, legal; nothing in L8 reaches past L7 into a corpus module directly); and species counterpoint / common-practice harmony frameworks (Phase 10/11, consume `Framework`/`Rule`/`Constraint` as extension modules, not part of L8's own build).
