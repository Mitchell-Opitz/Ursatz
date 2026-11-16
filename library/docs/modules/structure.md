# structure

[Docs index](../README.md): the Ursatz docs by topic.

Layer L5 (`music-structure`). Path `src/structure/`, tests `tests/structure/`. Depends on `identity`, `core`, `time`, `pitch`, `events`, and `containers` only. Must not depend on anything L6+.

## Provides

`Structure` (base category composing an `Entity` with non-duplicating event refs), `Motif` (Structure), `MotifOccurrence` (built on `Occurrence`, L1), `Cadence` (Structure), and `PhraseOccurrence` (built on `Occurrence`, L1).

## Notes

- `Structure` itself is a real, constructible base type, not a pure marker like `Container`. The Registry requires an invariant test at this layer confirming referenced events are never duplicated, so `Structure` needs actual event-ref state to test against.
- `Motif`, `MotifOccurrence`, `Cadence`, and `PhraseOccurrence` do not embed `Structure`'s or `Entity`'s wrapper structs directly. Each flattens to its own fields.
- `MotifOccurrence` and `PhraseOccurrence` are the exceptions that embed their L1 base type directly. Each composes `Occurrence` (own id plus the target's id).
- No hierarchy is assumed among `Structure` subtypes (TDD §57). A Structure graph is not required to follow Motif to Phrase to Section as a universal shape, and nothing here hard-codes that shape.
- `Cadence` is a Phase-5 structural shell only. Its `optional_harmonic_context` field is an opaque, nullable `Value`, the same opaque-token treatment `PitchIdentity` uses. Full harmonic-context semantics are an L6+/L7 concern and are not built here. Cadence detection, recognizing that a region of events forms a cadence, is Phase 8 (`analysis`) work and out of scope for this module.
- The tests read a `Cadence`'s id and event ids, a `Motif`'s event id count, a `MotifOccurrence`'s motif id and region event id count and a `PhraseOccurrence`'s region event id count through `tests/support/structure_test_access.h`. No application reads them, so the structure headers have no accessor for them.
- `PhraseOccurrence` carries the Registry's non-normative, low-priority status, built to close out the Registry's known-gaps list rather than as settled design. It is an occurrence of a phrase named by id; the kernel has no phrase container.

- Motif/Cadence/PhraseOccurrence stay L5 structural shells; they are not reimplemented as L6 `Interpretation`-shaped types. When Cadence Detection, Motif Detection, or Phrase Segmentation (Phase 8 `analysis`, later branches) produce a framework-relative claim about one of these L5 instances, that claim is expressed as an existing generic `Interpretation` (L6, the same pattern `Chord`/`Key` already use -- see `docs/modules/interpretation.md`) with `target_id` set to the L5 instance's `EntityID`. No new named L6 type is introduced for Motif/Cadence/PhraseOccurrence claims.

## Deferred

Out of scope entirely, not missing. `SimilarityResult`, Motif Detection, Phrase Segmentation, Cadence Detection (Phase 8 `analysis` procedures that produce `Motif`/`MotifOccurrence`/`Cadence` instances, and generic `Interpretation` claims about them, not data shapes this layer defines). Also out of scope, `Section` (an L4 container, built in `containers`), and any harmonic-context resolution (L6/L7).

## Dependencies

Depends on `identity` (L0), `core` (L1), `time`/`pitch` (L2), `events` (L3), and `containers` (L4) only.