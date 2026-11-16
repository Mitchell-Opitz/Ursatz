# L8 shared notes

[Docs index](../README.md): the Ursatz docs by topic.

`transform` was retired in P28 B4 (R-287) and `generation` left the library in P55 (the unreached code was deleted, and the Composer took the three types it uses); this page keeps their history. `transform`, `analysis`, and `generation` were built together against Dependency & Layer Spec v2 §2-§3. Like L7's `theory`/`rules` split, but stricter. All three were mutually sideways-restricted the same way L6's four siblings are, with no `transform`-`analysis`-`generation` import in any direction. Each depends on L0-L7 only, never on a sibling. The layer table enforces this by allowing only L0-L7 directories plus its own.

This is why the deleted `generation/candidate_evaluator.h`'s `CandidateEvaluation` could not reuse the deleted `analysis/analyzer.h`'s `AnalysisResult`, even though both held the identical Interpretation/Evidence shape.

## L7 rules decisions made for CandidateEvaluator (deleted in P55)

`rules/rule.h` had a `RuleEvaluation` shape with no confirmed Registry row; it was flagged, reviewed and resolved in the same L7 pass. The consumer, `CandidateEvaluator`, and then `RuleEvaluation`, `constraint_evaluate`, `preference_rank`, `preference_rank_candidates` and the weight arithmetic were all deleted in P55, since nothing called them. What the decisions leave in `rules/` (see `docs/modules/rules.md`):

- A framework registers its `Constraint` callback, which takes an optional `Context`. A framework candidate is a `Value` with type tag `standard-<framework>/<table prefix>` holding the rule's subject struct; an unrecognised tag is `NOT_APPLICABLE`.
- `ConstraintEvalResult` has `CONSTRAINT_EVAL_NOT_APPLICABLE`, distinct from `CONSTRAINT_EVAL_FAIL`. A caller folding many results into one score must treat NOT_APPLICABLE as excluded from the score, never as a failure.
- A `Preference` callback never rejects a candidate: it returns a score.
- `CONSTRAINT_TYPE_WEIGHTED` stays on the same PASS/FAIL/NOT_APPLICABLE contract as every other `ConstraintType`: it does not get its own return shape.

## Chord and Key

Per the Registry, `Chord` and `Key` belong to `interpretation` (L6), not L8. Both are built there, and `ChordIdentification`/`KeyEstimation` produce populated `Chord`/`Key` objects (see `docs/modules/analysis.md`). The kernel analyzers do not construct a `Cadence` or persist a `MotifOccurrence`; they report by `Interpretation` and let a downstream caller build the L5 object from the claim.

## Out of scope for L8 entirely, not missing

Actual I/O format adapters (MIDI/MusicXML/MEI/ABC, L9); corpus ingestion/statistics itself (L9, and separately blocked from being imported here regardless, per the L7 open issue in `docs/modules/theory.md`; nothing in L8 reaches past L7 into a corpus module directly); and the theory frameworks, including species counterpoint and common-practice harmony (`frameworks/`, extension modules on `Framework`/`Rule`/`Constraint`, not part of L8's own build; see `frameworks/README.md`).