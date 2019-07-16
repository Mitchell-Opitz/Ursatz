# structure

Layer L5 (`music-structure`). Path `src/structure/`, tests `tests/structure/`. Depends on `identity`, `core`, `time`, `pitch`, `events`, and `containers` only. Must not depend on anything L6+.

## Provides

`Structure` (base category composing an `Entity` with non-duplicating event refs), `Motif` (Structure), `MotifOccurrence` (built on `Occurrence`, L1), and `Cadence` (Structure).

## Notes

- `Structure` itself is a real, constructible base type, not a pure marker like `Container`. The Registry requires an invariant test at this layer confirming referenced events are never duplicated, so `Structure` needs actual event-ref state to test against.
- `Motif`, `MotifOccurrence`, and `Cadence` do not embed `Structure`'s or `Entity`'s wrapper structs directly. Each flattens to its own fields, the same approach `NoteEvent` takes relative to `Event` and `Voice` takes relative to `Container` elsewhere in this codebase.
- `MotifOccurrence` is the one exception that embeds its L1 base type directly. It composes `Occurrence` (own id plus the target Motif's id) the same way `Event` composes `Occurrence` with a `TemporalAnchor`.
- No hierarchy is assumed among `Structure` subtypes (TDD §57). A Structure graph is not required to follow Motif to Phrase to Theme to Section as a universal shape, and nothing here hard-codes that shape.
- `Cadence` is a Phase-5 structural shell only. Its `optional_harmonic_context` field is an opaque, nullable `Value`, the same opaque-token treatment `PitchIdentity` and `ControlEvent`'s payload use. Full harmonic-context semantics are an L6+/L7 concern and are not built here. Cadence detection, recognizing that a region of events forms a cadence, is Phase 8 (`analysis`) work and out of scope for this module.

## Deferred

`Figure`; `Theme`, `ThemeOccurrence`; `PhraseOccurrence` (`Phrase` itself is L4 container-side, deferred there too, not built here either).

Out of scope entirely, not missing. `SimilarityResult`, Motif Detection, Phrase Segmentation, Cadence Detection (Phase 8 `analysis` procedures that produce `Motif`/`MotifOccurrence`/`Cadence` instances, not data shapes this layer defines). Also out of scope, `Section`, `Movement`, `Work` (L4 containers, deferred there too), and any harmonic-context resolution (L6/L7).

## Dependencies

Depends on `identity` (L0), `core` (L1), `time`/`pitch` (L2), `events` (L3), and `containers` (L4) only.
