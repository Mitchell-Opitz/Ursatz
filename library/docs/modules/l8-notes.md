# L8 shared notes

[Docs index](../README.md): the Ursatz docs by topic.

`transform` was retired in P28 B4 (R-287) and `generation` left the library in P55 (the unreached code was deleted, and the Composer took the three types it uses); this page keeps their history. `transform`, `analysis`, and `generation` were built together against Dependency & Layer Spec v2 §2-§3. Like L7's `theory`/`rules` split, but stricter. All three were mutually sideways-restricted the same way L6's four siblings are, with no `transform`-`analysis`-`generation` import in any direction. Each depends on L0-L7 only, never on a sibling. The layer table enforces this by allowing only L0-L7 directories plus its own.

This is why the deleted `generation/candidate_evaluator.h`'s `CandidateEvaluation` could not reuse the deleted `analysis/analyzer.h`'s `AnalysisResult`, even though both held the identical Interpretation/Evidence shape.

## L7 rules decisions made for CandidateEvaluator (deleted in P55 and P66b)

`CandidateEvaluator`, `RuleEvaluation`, `constraint_evaluate`, `preference_rank`, `preference_rank_candidates` and the weight arithmetic were deleted in P55, since nothing called them. P66b B2 deleted the rest of the objects: Rule, Constraint, Preference, their callbacks and `Context` (R-685). What stays in `rules/` is the rule table (`docs/modules/rules.md`): a rule area answers `*_rule_holds` on its subject directly, and says whether a rule is soft with its own `*_rule_soft` query.

## Chord and Key

Per the Registry, `Chord` and `Key` belong to `interpretation` (L6), not L8. Both are built there, and `ChordIdentification`/`KeyEstimation` produce populated `Chord`/`Key` objects (see `docs/modules/analysis.md`). The kernel analyzers do not construct a `Cadence` or persist a `MotifOccurrence`; they report by `Interpretation` and let a downstream caller build the L5 object from the claim.

## Out of scope for L8 entirely, not missing

Actual I/O format adapters (MIDI/MusicXML/MEI/ABC, L9); corpus ingestion/statistics itself (L9, and separately blocked from being imported here regardless, per the L7 open issue in `docs/modules/theory.md`; nothing in L8 reaches past L7 into a corpus module directly); and the theory frameworks, including species counterpoint and common-practice harmony (`theory/`, areas on `Framework`/`Rule`/`Constraint`, not part of the analysis build; see `theory/README.md`).