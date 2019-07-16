# rules

Layer L7 (`music-rules`). Path `src/rules/`, tests `tests/rules/`. Sibling of `theory`, and the one L7/L8 sibling pair that is not mutually restricted. `Rule`, `Constraint`, and `Preference` each hold a direct, unowned `Framework` reference (the same reference-only pattern `Context` uses for its own parent field).

## Provides

`Rule`, `Constraint`, `Preference`.

## Notes

- `Constraint`'s type is a tagged type, not a boolean. `ConstraintType` has five variants (`_HARD`, `_SOFT`, `_WEIGHTED`, `_CONDITIONAL`, `_CONTEXTUAL`, TDD §96), not a single `is_hard` flag. Only `CONSTRAINT_TYPE_WEIGHTED` uses the weight numerator/denominator fields; every other type ignores them. `constraint_evaluate` is a stub. It validates its arguments and reports `CONSTRAINT_EVAL_NOT_IMPLEMENTED`, never actually deciding whether a candidate satisfies the constraint. Wiring real evaluation logic into a generation pipeline is `generation` (L8) work.
- `Preference` cannot reject a candidate. `preference_rank` is structurally distinct from `constraint_evaluate`. Its result type has no pass/fail/invalid variant, only "computed" or "not implemented." No code path in `preference.h` lets a `Preference` invalidate a candidate the way a `Constraint` can.

## Deferred

`RuleEvaluation`, flagged for project-owner confirmation before building (Registry-First Dependency Rule, Dependency Spec §6). `Rule` depends on it, but no distinct Registry row or shape was found for it in this pass. Do not invent its field shape from TDD §102 alone without that confirmation.

`Procedure` belongs to `analysis`/`generation` (L8) despite appearing near Phase 9 material in the TDD; it is not built here.

## Dependencies

Depends on `identity` (L0), `core` (L1), `time`/`pitch` (L2), `events` (L3), `containers` (L4), `structure` (L5), all four L6 modules, and `theory` (L7) only.
