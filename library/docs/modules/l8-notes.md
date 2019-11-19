# L8 shared notes

`transform`, `analysis`, and `generation` are built together against Dependency & Layer Spec v2 §2-§3. Like L7's `theory`/`rules` split, but stricter. All three are mutually sideways-restricted the same way L6's four siblings are, with no `transform`-`analysis`-`generation` import in any direction. Each depends on L0-L7 only, never on a sibling. Each module's architecture test enforces this by allowlisting only L0-L7 directories plus its own.

This is why `generation/candidate_evaluator.h`'s `CandidateEvaluation` cannot reuse `analysis/analyzer.h`'s `AnalysisResult`, even though both hold the identical Interpretation/Evidence shape. See `docs/modules/generation.md`.

## L7 rules decisions relevant to CandidateEvaluator (L8)

`rules/rule.h` depended on a `RuleEvaluation` shape that had no confirmed Registry row; this was flagged, reviewed, and resolved in the same L7 pass rather than left for L8 to invent later. Four decisions, recorded here since a future `CandidateEvaluator` implementation is the actual consumer:

- `constraint_evaluate` (`rules/constraint.h`) now takes an optional `Context` (`context/context.h`) reference, so a `CandidateEvaluator` evaluating a `CONDITIONAL`/`CONTEXTUAL` `Constraint` passes context through this call rather than resolving the condition itself. This was widened at L7 while no real callers existed, rather than deferred into a breaking change once `CandidateEvaluator` had real callers.
- `ConstraintEvalResult` gained `CONSTRAINT_EVAL_NOT_APPLICABLE`, distinct from `CONSTRAINT_EVAL_FAIL`. A `CandidateEvaluator` folding many per-Constraint results into one `CandidateEvaluation.score` MUST treat NOT_APPLICABLE as excluded from the score, never as a failure.
- `RuleEvaluation` (`rules/rule_evaluation.h`, L7) is the per-Rule unit a `CandidateEvaluator` builds while judging a `Candidate` against one `Rule`/`Constraint`/`Preference` -- the shape `CandidateEvaluation`'s own evaluations[]/violations[] arrays are assembled from. Its struct/shape lives in `rules/` (L7, alongside what it evaluates); populating real `RuleEvaluation` values from `constraint_evaluate`/`preference_rank` output against an actual `Candidate` is `CandidateEvaluator`'s job (L8) -- not yet built.
- `CONSTRAINT_TYPE_WEIGHTED` stays on the same PASS/FAIL/NOT_APPLICABLE evaluation contract as every other `ConstraintType` -- it does not get its own return shape. A `CandidateEvaluator` reads the weight separately via `constraint_weight_numerator`/`constraint_weight_denominator` and applies it when aggregating, rather than expecting `constraint_evaluate` itself to return a scaled magnitude.

## Deferred, flagged rather than built

Per the Registry, `Chord` and `Key` belong to `interpretation` (L6), not L8. `ChordIdentification` and `KeyEstimation` identify a chord/key and report it as a framework-relative `Interpretation`, but neither constructs a `Chord` or `Key` object; there is nothing at L6 yet to construct. This mirrors `CadenceDetection` not constructing a `Cadence` and `MotifDetection` not persisting the `MotifOccurrence` it validates. Both report by `Interpretation` and let a downstream caller build the real L5/L6 object from the claim. Building `Chord`/`Key` themselves is L6 follow-up work.

## Out of scope for L8 entirely, not missing

Actual I/O format adapters (MIDI/MusicXML/MEI/ABC, L9); corpus ingestion/statistics itself (L9, and separately blocked from being imported here regardless, per the L7 open issue in `docs/modules/theory.md`; `CandidateEvaluator` consuming a `StatisticalModel` is fine, that is L8 to L7, legal; nothing in L8 reaches past L7 into a corpus module directly); and species counterpoint / common-practice harmony frameworks (Phase 10/11, consume `Framework`/`Rule`/`Constraint` as extension modules, not part of L8's own build).