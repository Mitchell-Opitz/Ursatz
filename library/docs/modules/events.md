# events

Layer L3 (`music-events`). Path `src/events/`, tests `tests/events/`. Depends on `identity`, `core`, `time`, and `pitch` only. Must not depend on `containers` (L4).

## Provides

`Event` (base type composing `Occurrence` with `TemporalAnchor`), `NoteEvent`, `RestEvent`, `ControlEvent` (the three Phase-1 event kinds), `Tie` (connects two `NoteEvent`s by id), `Slur` (a reference-only span over a range of `NoteEvent`s), `Articulation` (a marking attached to a single `Event` by reference), `Dynamics` (a marking attached to an `Event` by reference or a `TimeSpan` by value), `TempoMarking` (a text and/or `MetronomeMarking` tempo indication at a position), `StructuralMarker` (a fermata, repeat, ending, segno, and so on at a position), `Instrument` (identity, name, range, transposition), and `InstrumentCapability` (range/transposition derived from an `Instrument`, separable from its identity).

## Notes

- `NoteEvent`'s category (Entity vs Event vs pure Occurrence) is unresolved upstream (Domain Spec §9, §16.1), carried forward, not resolved here. `NoteEvent` is implemented under the current provisional "Event" treatment and is deliberately not built on top of `core/entity.h`'s `Entity` type, since doing so would itself assume a resolution.
- `NoteEvent`'s `optional_voice` field is typed as `VoiceReference*` (L1). No C type representing a resolved `Voice` (L4) exists anywhere in this codebase, so the L3/L4 non-dependency cannot be violated through that constructor; `tests/events/test_architecture_events.c` independently confirms no `containers/` include exists anywhere under `src/events/`.
- `NoteEvent` has two separate equality functions, `note_event_equals_by_id` (same id regardless of other fields) and `note_event_equals_by_value` (same content independent of id). Value-equal does not imply the same event.
- `NoteEvent` has no mutator. A revision is a new `NoteEvent` with a new id and/or provenance; constructing that from an old one needs an Activity/Transformation, which does not exist yet at this layer.
- `NoteEvent`'s `provenance` field must always be an explicit `ProvenanceReference` (present or absent), never `NULL`. Hand-authored `NoteEvent`s may use `state=absent`; nothing yet enforces `state=present` for Activity-produced `NoteEvent`s, since no Activity/Transformation exists at this layer to produce that case.
- `Tie` references its two `NoteEvent`s by `EntityID` only, never by embedded instances. Constructing a `Tie` does not alter either referenced `NoteEvent`'s own fields.
- `ControlEvent`'s payload is generic and opaque at this layer, the same treatment `PitchIdentity` gives its token. Protocol-specific meaning (MIDI, OSC, MusicXML) is an adapter concern for L9+, not this module.
- `Slur` references its NoteEvents by an array of `EntityID*` (mirroring `structure/structure.h`'s `Structure`, not `Tie`'s fixed two-field shape, since a slur spans a variable-length range) -- never by embedded instance, and `note_event_id_count` must be at least 2 (a range of one is degenerate).
- `Articulation` attaches its `kind` (an opaque, non-exhaustive string, e.g. "staccato") to a single `Event` by `EntityID` only, never embedded.
- `Dynamics` attaches its `marking` (an opaque, non-exhaustive string, e.g. "mf", "cresc.") to either an `Event` (by `EntityID`, via `dynamics_create_for_event`) or a `TimeSpan` (by value, via `dynamics_create_for_time_span` -- `TimeSpan` is immutable and has no identity to reference by, so this constructor takes ownership of it directly rather than embedding a live/mutable link).
- `Instrument` builds exactly four Registry-listed properties -- `id`, `name`, `range` (lowest/highest `PitchIdentity`), and `transposition` (a plain, uninterpreted `int` semitone offset, the same opaque-numeric-offset treatment `PitchSpelling`'s `accidental` gets). Clefs and technique are DELIBERATELY NOT implemented -- a deliberate scope choice, not an oversight, consistent with the Registry's own "property list is illustrative" framing for this type. `Instrument` does not build on `core/entity.h`'s `Entity`, mirroring `Framework`/`TuningSystem`, for the same category-boundary reason.
- `InstrumentCapability` is derived from an `Instrument` (`instrument_capability_create_from_instrument`), copying range/transposition into a standalone `Value` with no residual link back to the source `Instrument`'s id -- this is how it "depends on `Instrument`" while staying "separable from `Instrument` identity." Technique is deferred here too, for the same reason as on `Instrument`.
- `TempoMarking` is positioned by a `TemporalAnchor` like `ControlEvent`, and needs a text, a `MetronomeMarking`, or both ("Allegro (quarter = 120)"). Its text is an opaque string; what tempo words mean (grouping, order, rit./accel./a tempo) is `frameworks/standard-markings/`.
- `StructuralMarker` carries an opaque `kind` string and one optional integer `parameter` (the number of a numbered ending; 0 means none). How a sequence of markers steers playback is `frameworks/standard-markings/` (`repeat_navigation.h`), which works on plain measure indices because mapping an anchor to a measure waits on `MeterMap`.

## Deferred

Nothing in this module's own scope remains deferred as of this pass. `Slur`, `Articulation`, `Dynamics`, `Instrument`, and `InstrumentCapability` are all implemented above.

Out of scope entirely, not missing. `Voice` itself (L4, `containers`; only `VoiceReference` is used here), `Interval`, `Sonority`, `PerformanceEvent` (later layers), any Transformation/Activity-driven provenance enforcement, and `Instrument`'s clefs/technique fields (see the `Instrument` note above -- a deliberate scope choice, not a gap in this pass).

## Dependencies

Depends on `identity` (L0), `core` (L1), `time` and `pitch` (L2) only.