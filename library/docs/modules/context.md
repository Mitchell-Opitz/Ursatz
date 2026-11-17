# context

[Docs index](../README.md): the Ursatz docs by topic.

Layer L6 (`music-context`). Path `theory/rules/`, tests `tests/context/`. One of four L6 siblings (with `semantics`, `relationship`, `interpretation`). Depends on `identity`, `core`, `time`, `pitch`, `events`, `containers`, and `structure` only, and does not depend on its three L6 siblings.

## Provides

The opaque type `Context` (`context/context.h`) and nothing else. The rule callbacks of `rules/` (`ConstraintEvaluateFn`, `PreferenceScoreFn`) take a `const Context *`, and nothing constructs one: `context_create` and `context_destroy` and the parent link were deleted (P55), with `context.c` and its test.

## Notes

See `docs/modules/l6-notes.md` for rules shared by all four L6 modules (no sideways imports, opaque-reference handling for not-yet-built dependencies).

## Dependencies

Depends on `identity` (L0), `core` (L1), `time`/`pitch` (L2), `events` (L3), `containers` (L4), and `structure` (L5) only. Never imports `semantics`, `relationship`, or `interpretation`.
