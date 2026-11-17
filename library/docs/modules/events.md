# events

[Docs index](../README.md): the Ursatz docs by topic.

Layer L3 (`music-events`). Path `model/events/`, tests `tests/events/`. Depends on `identity`, `core`, `time`, and `pitch` only. Must not depend on `containers` (L4).

## Provides

`NoteEvent`, `Dynamics` (a marking attached to a `TimeSpan` by value), and `TempoMarking` (a text and/or `MetronomeMarking` tempo indication at a position).

## Notes

- `NoteEvent`'s category (Entity vs Event vs pure Occurrence) is unresolved upstream (Domain Spec §9, §16.1), carried forward, not resolved here. `NoteEvent` is implemented under the current provisional "Event" treatment and is deliberately not built on top of `core/entity.h`'s `Entity` type, since doing so would itself assume a resolution.
- `NoteEvent`'s `optional_voice` field is typed as `VoiceReference*` (L1). No C type representing a resolved `Voice` (L4) exists anywhere in this codebase, so the L3/L4 non-dependency cannot be violated through that constructor; the layer table independently confirms no `containers/` include exists anywhere under `model/events/`.
- `NoteEvent` has no mutator. A revision is a new `NoteEvent` with a new id and/or provenance; constructing that from an old one needs an Activity/Transformation, which does not exist yet at this layer.
- `NoteEvent`'s `provenance` field must always be an explicit `ProvenanceReference` (present or absent), never `NULL`. Hand-authored `NoteEvent`s may use `state=absent`; nothing yet enforces `state=present` for Activity-produced `NoteEvent`s, since no Activity/Transformation exists at this layer to produce that case.
- `Dynamics` attaches its `marking` (an opaque, non-exhaustive string, e.g. "mf", "cresc.") to a `TimeSpan` by value, via `dynamics_create_for_time_span` (`TimeSpan` is immutable and has no identity to reference by, so this constructor takes ownership of it directly rather than embedding a live/mutable link).
- `TempoMarking` is positioned by a `TemporalAnchor`, and needs a text, a `MetronomeMarking`, or both ("Allegro (quarter = 120)"). Its text is an opaque string; what tempo words mean (grouping, order, rit./accel./a tempo) is `theory/markings/`.

## Deferred

Nothing in this module's own scope remains deferred as of this pass. `NoteEvent`, `Dynamics` and `TempoMarking` are implemented above.

Out of scope entirely, not missing. `Voice` itself (L4, `containers`; only `VoiceReference` is used here), `Interval`, `Sonority`, `PerformanceEvent` (later layers), and any Transformation/Activity-driven provenance enforcement.

## Dependencies

Depends on `identity` (L0), `core` (L1), `time` and `pitch` (L2) only.