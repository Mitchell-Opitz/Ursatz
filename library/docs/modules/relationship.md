# relationship

[Docs index](../README.md): the Ursatz docs by topic.

Layer L6 (`music-relationship`). Path `model/relationship/`, tests `tests/relationship/`. One of four L6 siblings; see `docs/modules/l6-notes.md` for rules shared across all four.

## Provides

`Relationship`.

## Notes

The predicate vocabulary is open, not a fixed enum. The System Registry marks the initial predicate vocabulary (TDD §77, `contains`, `precedes`, `derived_from`, `realizes`, `resembles`, `interpreted_as`, and others) "explicitly extensible, not fixed." `Relationship` takes the predicate as an open `predicate_name` string, not a closed set of predicate constants.

The tests read a `Relationship`'s predicate name and scope through `tests/support/relationship_test_access.h`; no application reads them, so `relationship.h` has no accessor for them.

Analysis-layer uses of `Relationship`: `ThematicReturnDetection` and `FormStructureDetection` (`music-analysis`) build `Relationships` between theme occurrences (`"resembles"`) and between sections (`"precedes"`, TDD §77's vocabulary). This is the mechanism the codebase uses wherever an Interpretation's claim needs to reference two objects rather than one: `Interpretation.target` stays a single `EntityID` (referencing the `Relationship`'s own id), and the `Relationship` itself carries the two-object link.

## Dependencies

Depends on `identity` (L0), `core` (L1), `time`/`pitch` (L2), `events` (L3), `containers` (L4), and `structure` (L5) only. Never imports `context`, `semantics`, or `interpretation`.
