# events

Layer L3 (`music-events`). Path `src/events/`, tests `tests/events/`. Depends on `identity`, `core`, `time`, and `pitch` only. Must not depend on `containers` (L4).

## Provides

`Event` (base type composing `Occurrence` with `TemporalAnchor`), `NoteEvent`, `RestEvent`, `ControlEvent` (the three Phase-1 event kinds), and `Tie` (connects two `NoteEvent`s by id).

## Notes

- `NoteEvent`'s category (Entity vs Event vs pure Occurrence) is unresolved upstream (Domain Spec §9, §16.1), carried forward, not resolved here. `NoteEvent` is implemented under the current provisional "Event" treatment and is deliberately not built on top of `core/entity.h`'s `Entity` type, since doing so would itself assume a resolution.
- `NoteEvent`'s `optional_voice` field is typed as `VoiceReference*` (L1). No C type representing a resolved `Voice` (L4) exists anywhere in this codebase, so the L3/L4 non-dependency cannot be violated through that constructor; `tests/events/test_architecture_events.c` independently confirms no `containers/` include exists anywhere under `src/events/`.
- `NoteEvent` has two separate equality functions, `note_event_equals_by_id` (same id regardless of other fields) and `note_event_equals_by_value` (same content independent of id). Value-equal does not imply the same event.
- `NoteEvent` has no mutator. A revision is a new `NoteEvent` with a new id and/or provenance; constructing that from an old one needs an Activity/Transformation, which does not exist yet at this layer.
- `NoteEvent`'s `provenance` field must always be an explicit `ProvenanceReference` (present or absent), never `NULL`. Hand-authored `NoteEvent`s may use `state=absent`; nothing yet enforces `state=present` for Activity-produced `NoteEvent`s, since no Activity/Transformation exists at this layer to produce that case.
- `Tie` references its two `NoteEvent`s by `EntityID` only, never by embedded instances. Constructing a `Tie` does not alter either referenced `NoteEvent`'s own fields.
- `ControlEvent`'s payload is generic and opaque at this layer, the same treatment `PitchIdentity` gives its token. Protocol-specific meaning (MIDI, OSC, MusicXML) is an adapter concern for L9+, not this module.

## Deferred

`Slur`, `Articulation`, `Dynamics` (Registry Phase 2). `Instrument`, `InstrumentCapability` (Registry Phase "1+", provisional placement, property list explicitly flagged illustrative, non-normative).

Out of scope entirely, not missing. `Voice` itself (L4, `containers`; only `VoiceReference` is used here), `Interval`, `Sonority`, `PerformanceEvent` (later layers), and any Transformation/Activity-driven provenance enforcement.

## Dependencies

Depends on `identity` (L0), `core` (L1), `time` and `pitch` (L2) only.
