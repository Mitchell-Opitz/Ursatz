# context

Layer L6 (`music-context`). Path `src/context/`, tests `tests/context/`. One of four L6 siblings (with `semantics`, `relationship`, `interpretation`). Depends on `identity`, `core`, `time`, `pitch`, `events`, `containers`, and `structure` only, and does not depend on its three L6 siblings.

## Provides

`Context`, composable via a parent reference.

## Notes

See `docs/modules/l6-notes.md` for rules shared by all four L6 modules (no sideways imports, opaque-reference handling for not-yet-built dependencies).

## Dependencies

Depends on `identity` (L0), `core` (L1), `time`/`pitch` (L2), `events` (L3), `containers` (L4), and `structure` (L5) only. Never imports `semantics`, `relationship`, or `interpretation`.
