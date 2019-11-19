# rules

Layer L7 (`music-rules`). Path `src/rules/`, tests `tests/rules/`. Sibling of `theory`, and the one L7/L8 sibling pair that is not mutually restricted. `Rule`, `Constraint`, and `Preference` each hold a direct, unowned `Framework` reference (the same reference-only pattern `Context` uses for its own parent field).

## Provides

`Rule`, `Constraint`, `Preference`, `RuleEvaluation`.

## Notes

- `Constraint`'s type is a tagged type, not a boolean. `ConstraintType` has five variants (`_HARD`, `_SOFT`, `_WEIGHTED`, `_CONDITIONAL`, `_CONTEXTUAL`, TDD §96), not a single `is_hard` flag. Only `CONSTRAINT_TYPE_WEIGHTED` uses the weight numerator/denominator fields; every other type ignores them. `constraint_evaluate` is a stub. It validates its arguments and reports `CONSTRAINT_EVAL_NOT_IMPLEMENTED`, never actually deciding whether a candidate satisfies the constraint. Wiring real evaluation logic into a generation pipeline is `generation` (L8) work.
- `constraint_evaluate` takes an optional `Context` (`context/context.h`) reference, widened now rather than left for a later breaking change: `CONDITIONAL`/`CONTEXTUAL` constraints need ambient context to decide whether their condition applies, and this is the cheapest point to add it since no real callers exist yet. `ConstraintEvalResult` also gained `CONSTRAINT_EVAL_NOT_APPLICABLE`, reserved for a `CONDITIONAL`/`CONTEXTUAL` constraint whose condition does not hold -- kept distinct from `CONSTRAINT_EVAL_FAIL` so a caller aggregating scores downstream never conflates "did not apply" with "failed." `CONSTRAINT_TYPE_WEIGHTED` deliberately stays on the same PASS/FAIL/NOT_APPLICABLE contract as every other variant (weight is read separately via the existing accessors) -- no variant gets its own return shape.
- `Preference` cannot reject a candidate. `preference_rank` is structurally distinct from `constraint_evaluate`. Its result type has no pass/fail/invalid variant, only "computed" or "not implemented." No code path in `preference.h` lets a `Preference` invalidate a candidate the way a `Constraint` can.
- `RuleEvaluation` is the per-Rule evaluation outcome a `CandidateEvaluator` (`generation/candidate_evaluator.h`, L8) builds while judging a Candidate, and the shape that `CandidateEvaluation`'s own evaluations[]/violations[] would be assembled from -- confirmed with the project owner after this type was originally flagged as needing a decision (Registry-First Dependency Rule, Dependency Spec §6): no distinct Registry row exists for it, so this is a considered fit against `Rule`/`Constraint`/`Preference` and `CandidateEvaluator`'s consumption pattern, not a literal TDD field list. It holds a required `rule_id` reference (by id only, never embedded), a tagged `RuleEvalOutcome` (`_SATISFIED`/`_VIOLATED`/`_NOT_APPLICABLE`/`_NOT_IMPLEMENTED` -- `_NOT_APPLICABLE` mirrors `constraint_evaluate`'s own `CONSTRAINT_EVAL_NOT_APPLICABLE`), an optional owned `finding` (`Interpretation`, kept explainable per TDD §159), and an optional weight fraction (denominator 0 means "no weight recorded"). It has no identity of its own -- no `id` field, no `*_identity_equals` -- the same treatment `CandidateEvaluation` gets. This type only defines the shape and its own construction invariants; populating a real one from `constraint_evaluate`/`preference_rank` output against an actual candidate is `CandidateEvaluator`'s job (L8), the same L7/L8 split those two stubs already follow.

## Deferred

`Procedure` belongs to `analysis`/`generation` (L8) despite appearing near Phase 9 material in the TDD; it is not built here.

## Dependencies

Depends on `identity` (L0), `core` (L1), `time`/`pitch` (L2), `events` (L3), `containers` (L4), `structure` (L5), all four L6 modules, and `theory` (L7) only.