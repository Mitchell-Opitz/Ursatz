# rules

Layer L7 (`music-rules`). Path `src/rules/`, tests `tests/rules/`. Sibling of `theory`, and the one L7/L8 sibling pair that is not mutually restricted. `Rule`, `Constraint`, and `Preference` each hold a direct, unowned `Framework` reference (the same reference-only pattern `Context` uses for its own parent field).

## Provides

`Rule`, `Constraint`, `Preference`.

## Notes

- `Constraint`'s type is a tagged type, not a boolean. `ConstraintType` has five variants (`_HARD`, `_SOFT`, `_WEIGHTED`, `_CONDITIONAL`, `_CONTEXTUAL`, TDD §96), not a single `is_hard` flag. Only `CONSTRAINT_TYPE_WEIGHTED` uses the weight numerator/denominator fields; every other type ignores them. `constraint_evaluate` is a stub. It validates its arguments and reports `CONSTRAINT_EVAL_NOT_IMPLEMENTED`, never actually deciding whether a candidate satisfies the constraint. Wiring real evaluation logic into a generation pipeline is `generation` (L8) work.
- `constraint_evaluate` takes an optional `Context` (`context/context.h`) reference, widened now rather than left for a later breaking change: `CONDITIONAL`/`CONTEXTUAL` constraints need ambient context to decide whether their condition applies, and this is the cheapest point to add it since no real callers exist yet. `ConstraintEvalResult` also gained `CONSTRAINT_EVAL_NOT_APPLICABLE`, reserved for a `CONDITIONAL`/`CONTEXTUAL` constraint whose condition does not hold -- kept distinct from `CONSTRAINT_EVAL_FAIL` so a caller aggregating scores downstream never conflates "did not apply" with "failed." `CONSTRAINT_TYPE_WEIGHTED` deliberately stays on the same PASS/FAIL/NOT_APPLICABLE contract as every other variant (weight is read separately via the existing accessors) -- no variant gets its own return shape.
- `Preference` cannot reject a candidate. `preference_rank` is structurally distinct from `constraint_evaluate`. Its result type has no pass/fail/invalid variant, only "computed" or "not implemented." No code path in `preference.h` lets a `Preference` invalidate a candidate the way a `Constraint` can.

## Deferred

`RuleEvaluation`: project-owner confirmation received on the shape (struct lives here in `rules/`, populated by `generation`'s `CandidateEvaluator`, L8) -- not yet built, tracked as a follow-up commit in this same pass.

`Procedure` belongs to `analysis`/`generation` (L8) despite appearing near Phase 9 material in the TDD; it is not built here.

## Dependencies

Depends on `identity` (L0), `core` (L1), `time`/`pitch` (L2), `events` (L3), `containers` (L4), `structure` (L5), all four L6 modules, and `theory` (L7) only.