# structure

[Docs index](../README.md): the Ursatz docs by topic.

Layer L5 (`music-structure`). Path `src/structure/`, tests `tests/structure/`. Depends on `identity`, `core`, `time`, `pitch`, `events`, and `containers` only. Must not depend on anything L6+.

## Provides

`Structure` (base category composing an `Entity` with non-duplicating event refs), `Motif` (Structure), `MotifOccurrence` (built on `Occurrence`, L1), `Cadence` (Structure), `Figure` (Structure), `Theme` (Structure), `ThemeOccurrence` (built on `Occurrence`, L1), and `PhraseOccurrence` (built on `Occurrence`, L1).

## Notes

- `Structure` itself is a real, constructible base type, not a pure marker like `Container`. The Registry requires an invariant test at this layer confirming referenced events are never duplicated, so `Structure` needs actual event-ref state to test against.
- `Motif`, `MotifOccurrence`, `Cadence`, `Figure`, `Theme`, `ThemeOccurrence`, and `PhraseOccurrence` do not embed `Structure`'s or `Entity`'s wrapper structs directly. Each flattens to its own fields, the same approach `NoteEvent` takes relative to `Event` and `Voice` takes relative to `Container` elsewhere in this codebase.
- `MotifOccurrence`, `ThemeOccurrence`, and `PhraseOccurrence` are the exceptions that embed their L1 base type directly. Each composes `Occurrence` (own id plus the target's id) the same way `Event` composes `Occurrence` with a `TemporalAnchor`.
- No hierarchy is assumed among `Structure` subtypes (TDD §57). A Structure graph is not required to follow Motif to Phrase to Theme to Section as a universal shape, and nothing here hard-codes that shape. `Theme` referencing `Phrase` ids does not imply Theme must be built from Phrases, only that it may reference them.
- `Cadence` is a Phase-5 structural shell only. Its `optional_harmonic_context` field is an opaque, nullable `Value`, the same opaque-token treatment `PitchIdentity` and `ControlEvent`'s payload use. Full harmonic-context semantics are an L6+/L7 concern and are not built here. Cadence detection, recognizing that a region of events forms a cadence, is Phase 8 (`analysis`) work and out of scope for this module.
- `Figure` and `Theme` are Registry-flagged "No (example only)": minimal, non-normative scaffolding consistent with `Motif`'s existing pattern, not a claim of a finished, authoritative design. `ThemeOccurrence` and `PhraseOccurrence` carry the same non-normative, low-priority status, built to close out the Registry's known-gaps list rather than as settled design.
- `Theme` references `containers/phrase.h`'s L4 container-layer `Phrase` by id, not this module's own occurrence-based sense of the word (`PhraseOccurrence`). `PhraseOccurrence` is likewise an occurrence of that same L4 container `Phrase` -- see `containers/phrase.h`'s header comment for the same disambiguation from its side.

- Motif/Cadence/Theme/PhraseOccurrence stay L5 structural shells; they are not reimplemented as L6 `Interpretation`-shaped types. When Cadence Detection, Motif Detection, or Phrase Segmentation (Phase 8 `analysis`, later branches) produce a framework-relative claim about one of these L5 instances, that claim is expressed as an existing generic `Interpretation` (L6, the same pattern `Chord`/`Key` already use -- see `docs/modules/interpretation.md`) with `target_id` set to the L5 instance's `EntityID`. No new named L6 type is introduced for Motif/Cadence/Theme/PhraseOccurrence claims.

## Deferred

Out of scope entirely, not missing. `SimilarityResult`, Motif Detection, Phrase Segmentation, Cadence Detection (Phase 8 `analysis` procedures that produce `Motif`/`MotifOccurrence`/`Cadence` instances, and generic `Interpretation` claims about them, not data shapes this layer defines). Also out of scope, `Section`, `Movement`, `Work` (L4 containers, built in `containers`), and any harmonic-context resolution (L6/L7).

## Dependencies

Depends on `identity` (L0), `core` (L1), `time`/`pitch` (L2), `events` (L3), and `containers` (L4) only.