# relationship

Layer L6 (`music-relationship`). Path `src/relationship/`, tests `tests/relationship/`. One of four L6 siblings; see `docs/modules/l6-notes.md` for rules shared across all four.

## Provides

`Relationship`, `PredicateDefinition`.

## Notes

`PredicateDefinition`'s vocabulary is open, not a fixed enum. The System Registry marks the initial predicate vocabulary (TDD §77, `contains`, `precedes`, `derived_from`, `realizes`, `resembles`, `interpreted_as`, and others) "explicitly extensible, not fixed." `predicate_definition.h` is a registrable metadata type with an open `name` string, not a closed set of predicate constants.

## Dependencies

Depends on `identity` (L0), `core` (L1), `time`/`pitch` (L2), `events` (L3), `containers` (L4), and `structure` (L5) only. Never imports `context`, `semantics`, or `interpretation`.
