# rules

[Docs index](../README.md): the Ursatz docs by topic.

Layer L7 (`music-rules`). Path `theory/rules/`, tests `tests/rules/`. Sibling of `theory`, and the one L7/L8 sibling pair that is not mutually restricted. `Rule`, `Constraint`, and `Preference` each hold a direct, unowned `Framework` reference (the same reference-only pattern `Context` uses for its own parent field).

## Provides

`Rule`, `Constraint`, `Preference`.

## Notes

- Each is created, given its callback where it has one, and destroyed; nothing reads its fields back. The accessors, `constraint_evaluate`, `preference_rank`, the rule evaluation, the weighted ranking and the rule pair builder were deleted (P55): no application and no framework called them. The frameworks evaluate their rules through the callbacks of their own rule tables (standard-harmony and standard-counterpoint build their own rule, constraint and preference triples). The tests read a `Preference` through `tests/support/preference_test_access.h` (`rules/preference_internal.h` holds the struct).
- `Constraint`'s type is a tagged type, not a boolean. `ConstraintType` has five variants (`_HARD`, `_SOFT`, `_WEIGHTED`, `_CONDITIONAL`, `_CONTEXTUAL`, TDD §96), not a single `is_hard` flag. Only `CONSTRAINT_TYPE_WEIGHTED` uses the weight numerator/denominator fields (denominator above 0, numerator not above it); every other type ignores them. A framework registers an evaluation callback (`ConstraintEvaluateFn`) on the Constraint with `constraint_set_evaluate_fn`. A `Constraint` holds only that pointer: the framework owns the callback and any data behind it, and nothing owned is embedded in `Constraint`. standard-harmony and standard-counterpoint register callbacks for every rule in their rule tables (a candidate `Value` carries a `standard-<framework>/<table prefix>` type tag and the rule's subject struct, borrowed for the call); no other framework registers one. Wiring evaluation into a generation pipeline is `generation` (L8) work.
- A callback takes an optional `Context` (`context/context.h`) reference: `CONDITIONAL` and `CONTEXTUAL` constraints need ambient context to decide whether their condition applies. The condition `Value` is opaque, so the callback decides. `ConstraintEvalResult` has `CONSTRAINT_EVAL_NOT_APPLICABLE` for a `CONDITIONAL` or `CONTEXTUAL` constraint whose condition does not hold, kept distinct from `CONSTRAINT_EVAL_FAIL` so a caller aggregating scores never conflates "did not apply" with "failed". `CONSTRAINT_TYPE_WEIGHTED` stays on the same PASS/FAIL/NOT_APPLICABLE contract as every other variant: no variant gets its own return shape.
- `Preference` cannot reject a candidate. Its scoring callback (`PreferenceScoreFn`), registered with `preference_set_score_fn`, returns a score and has no pass/fail/invalid vocabulary, so no code path in `preference.h` lets a `Preference` invalidate a candidate the way a `Constraint` can. The `Preference` holds only the pointer; the framework owns it and the data behind it. A score is an exact-rational `PreferenceScore` in [0, 1]: 1/1 is the best, 0/1 the worst, and a denominator of 0 means the callback could not score the candidate. Hard rejection belongs to `Constraint` only.

## Deferred

`Procedure` belongs to `analysis`/`generation` (L8) despite appearing near Phase 9 material in the TDD; it is not built here.

## Dependencies

Depends on `identity` (L0), `core` (L1), `time`/`pitch` (L2), `events` (L3), `containers` (L4), `structure` (L5), all four L6 modules, and `theory` (L7) only.