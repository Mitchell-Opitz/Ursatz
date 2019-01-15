# Architecture

Ursatz is organized into layered modules, each living under `src/<module>/`
with its tests mirrored under `tests/<module>/`.

## Layers

- **L0** - no dependencies on any other module. Currently: `identity`.
- **L1+** - may depend only on lower layers, never sideways or upward.
  Currently: `core` (music-core), which depends on `identity` only.
- **L2** - may depend only on L0/L1, never sideways or upward. Currently:
  `time` (music-time) and `pitch` (music-pitch), each depending on
  `identity` and `core` only. `time` and `pitch` are siblings and do not
  depend on each other.
- **L3** - may depend only on L0/L1/L2, never sideways or upward.
  Currently: `events` (music-events), depending on `identity`, `core`,
  `time`, and `pitch`. `events` MUST NOT depend on `music-containers`
  (L4) -- where a lower object needs to reference Voice (L4), it uses
  `VoiceReference` (L1, `music-core`) instead of a resolved Voice
  object or a `music-containers` import.
- **L4** - may depend only on L0/L1/L2/L3, never sideways or upward.
  Currently: `containers` (music-containers), depending on `identity`,
  `core`, `time`, `pitch`, and `events`. `containers` MUST NOT depend on
  anything L5+.

A module must not import anything outside its own layer's allowed
dependencies. Each module has an architecture test enforcing this
(e.g. `tests/identity/test_architecture_identity.c`,
`tests/core/test_architecture_core.c`, `tests/time/test_architecture_time.c`,
`tests/pitch/test_architecture_pitch.c`,
`tests/events/test_architecture_events.c`,
`tests/containers/test_architecture_containers.c`).

## Modules

| Module     | Layer | Path              | Tests                 |
|------------|-------|-------------------|------------------------|
| identity   | L0    | `src/identity/`   | `tests/identity/`      |
| core       | L1    | `src/core/`       | `tests/core/`          |
| time       | L2    | `src/time/`       | `tests/time/`          |
| pitch      | L2    | `src/pitch/`      | `tests/pitch/`         |
| events     | L3    | `src/events/`     | `tests/events/`        |
| containers | L4    | `src/containers/` | `tests/containers/`    |

## music-core notes

`music-core` provides the base types other modules build concrete types
from: `Entity`, `Value`, `Occurrence`, the five equality concepts
(identity, value, structural, semantic, canonical), and three reference
types (`VoiceReference`, `RelationshipReference`, `ProvenanceReference`).

A few things worth knowing before building on top of it:

- The Entity/Occurrence/Event category boundary is still contested
  upstream (Domain Spec, System Registry). `Entity` and `Occurrence` are
  implemented without assuming how that boundary settles, and code built
  on them should do the same.
- `SemanticEquality` is a contract only at this layer (`SemanticEqualsFn`
  in `core/equality.h`). A real comparison needs Context, which is an L6
  concept, so it is deferred until that layer exists.
- `Entity`'s `provenance_reference` field from the System Registry is not
  implemented yet. `ProvenanceReference` exists as a type
  (`core/provenance_reference.h`), but wiring it into `Entity` is separate
  follow-up work, not part of this layer's initial pass.
- The "Canonical Musical Model / IR" described in the System Registry is
  not a module music-core imports. It is the graph formed when these L1
  base types are combined with higher-layer objects at consumption time.
  There is nothing to import here to satisfy that row.

## music-time notes

`music-time` provides `Duration` (magnitude), `MusicalTime` (position),
`TimeSpan` (a positioned interval between two `MusicalTime`s), and
`TimeInstant` (a zero-duration point, wrapping `MusicalTime`). All four
are exact rationals, never float/double (Law 19).

A few things worth knowing before building on top of it:

- `Duration` and `MusicalTime` are structurally identical (numerator/
  denominator, reduced to lowest terms) but are separate, non-convertible
  types: one is a magnitude, the other a position. Do not add a
  conversion between them.
- `TimeSpan`'s `boundary` vocabulary (`closed` / `open` / `left_closed` /
  `right_closed`) is PROVISIONAL per Domain Spec §16.3, not frozen. It is
  implemented as given, but a later revision to that vocabulary is not a
  breaking-change violation.
- `TimeSpan` requires `boundary` to be supplied explicitly at
  construction; there is no default. A deserializer built on top of
  `time_span_create` must reject a payload that omits it rather than
  assuming `closed`.
- `MusicalTime` has no lower bound enforced at this layer: negative
  positions (e.g. pickup measures) are legal here. Semantics for what a
  negative position means are deferred to a later spec.
- `TemporalAnchor` wraps a `MusicalTime` and situates a higher-layer
  object at that position, so callers building on `Occurrence` (L1) gain
  a temporal position without embedding position semantics in `Occurrence`
  itself, and without reimplementing `MusicalTime`'s rational
  representation outside `music-time`. Added as an L2 prerequisite for L3
  `music-events`' `Event` type; it carries no fields beyond the wrapped
  `MusicalTime`.

## music-pitch notes

`music-pitch` provides `PitchIdentity` (an opaque token identifying a
pitch) and `PitchSpelling` (a written pitch name: letter, accidental,
octave).

A few things worth knowing before building on top of it:

- `PitchIdentity`'s equality is a PLACEHOLDER: value equality on the raw
  token bytes. The true equivalence relation is deferred to Deliverable D,
  which does not exist yet. Expect this to be revised.
- `PitchIdentity`'s token is intentionally opaque and is NOT an EntityID.
  Its constructor takes a raw byte buffer, not an `EntityID*`, so there is
  no path to construct one from an EntityID-typed value.
- `PitchIdentity` construction takes no tuning-system parameters (no
  12-TET, no A440, no MIDI note number). Do not add any before
  Deliverable D exists.
- `PitchSpelling`'s `accidental` field is treated opaquely (an integer
  offset); its exact model is also deferred to Deliverable D.
  Enharmonic equivalents (e.g. C# and Db) are intentionally NOT
  value-equal, even though they may sound identical under some tunings.
- `PitchClass`, `PitchRealization`, `TuningSystem`, `WrittenPitch`, and
  `SoundingPitch` are Phase 2/3 (Deliverable D) and are out of scope for
  this module's initial pass.

## music-events notes

`music-events` provides `Event` (base type composing `Occurrence` (L1)
with `TemporalAnchor` (L2)), `NoteEvent`, `RestEvent`, `ControlEvent`
(the three Phase-1 event kinds), and `Tie` (a relationship connecting
two `NoteEvent`s by id).

A few things worth knowing before building on top of it:

- STATUS FLAG, carried forward, not resolved here: `NoteEvent`'s
  category (Entity vs Event vs pure Occurrence) is UNRESOLVED upstream
  (Domain Spec §9, §16.1). `NoteEvent` is implemented under the current
  provisional "Event" treatment and is deliberately not built on top of
  `core/entity.h`'s `Entity` type, since doing so would itself assume a
  resolution. Code built on `NoteEvent` should do the same.
- `NoteEvent`'s `optional_voice` field is typed as `VoiceReference*`
  (L1). There is no C type representing a resolved Voice (L4) anywhere
  in this codebase, so the L3/L4 non-dependency cannot be violated
  through that constructor -- and
  `tests/events/test_architecture_events.c` independently confirms no
  `containers/` include exists anywhere under `src/events/`.
- `NoteEvent` has two separate equality functions, not one:
  `note_event_equals_by_id` (IdentityEquality, same id regardless of
  other fields) and `note_event_equals_by_value` (StructuralEquality,
  same content independent of id). Value-equal does NOT imply the same
  event -- two independently constructed `NoteEvent`s can carry
  identical content under different ids.
- `NoteEvent` has no mutator. A revision is a new `NoteEvent` with a new
  id and/or provenance; constructing that from an old one needs an
  Activity/Transformation, which does not exist yet at this layer and is
  out of scope here.
- `NoteEvent`'s `provenance` field must always be an explicit
  `ProvenanceReference` (present or absent) -- never `NULL`. Hand-authored
  `NoteEvent`s may use `state=absent`; nothing yet enforces
  `state=present` for Activity-produced `NoteEvent`s, since no Activity/
  Transformation exists at this layer to produce that case.
- `Tie` references its two `NoteEvent`s by `EntityID` only, never by
  embedded `NoteEvent` instances -- constructing a `Tie` does not alter
  either referenced `NoteEvent`'s own fields.
- `ControlEvent`'s payload is generic/opaque at this layer, the same
  treatment `PitchIdentity` gives its token (`pitch/pitch_identity.h`).
  Protocol-specific meaning (MIDI, OSC, MusicXML, ...) is an adapter
  concern (L9+), not this module's job.
- Deferred, not implemented in this pass: `Slur`, `Articulation`,
  `Dynamics` (Registry Phase 2). `Instrument`, `InstrumentCapability`
  (Registry Phase "1+", provisional placement, property list explicitly
  flagged illustrative/non-normative) are also skipped this pass.
- Out of scope for `music-events` entirely, not missing: `Voice` itself
  (L4, `music-containers` -- only `VoiceReference` is used here),
  `Interval`, `Sonority`, `PerformanceEvent` (later layers, referenced
  only in `NoteEvent`'s relationships documentation), and any
  Transformation/Activity-driven provenance enforcement.

## music-containers notes

`music-containers` provides `Container` (a pure grouping base
category, no fields of its own) and `Voice` (Container), the only
Phase-1 concrete container type.

A few things worth knowing before building on top of it:

- `Voice`'s schema is `{id}` only (Domain Spec §10). It has no
  membership field -- no owned/mutable list of notes or events. The
  mechanism by which a Voice's membership is represented is deferred
  to a later Container/Structure spec that does not exist yet.
- A `Voice` MUST NOT be assumed to map 1:1 to an Instrument, Staff, or
  Part. A `Voice` MAY appear on multiple staves; a Staff MAY contain
  multiple voices -- semantic notes only, since `Staff` does not exist
  this pass.
- `Voice` has no mutator: a revision is a new `Voice` construction, not
  an in-place field change (Law C2).
- `Voice`'s identity equality (`voice_identity_equals`) compares by
  `EntityID` only, the same pattern `Entity` (`core/entity.h`) uses,
  reduced to the one field `Voice` actually has -- `Voice` carries no
  separate schema/version field to compare.
- This is also the layer where `VoiceReference` (built at L1 back in
  the `music-core` pass) gets a real resolution target. No code change
  was needed to `VoiceReference` itself; nothing in `music-containers`
  redefines or duplicates what it already does.
- Deferred, not implemented in this pass: `Layer`, `Staff`, `Part`,
  `Measure` (Registry Phase 2); `Phrase` (container use), `Section`,
  `Movement`, `Work`, `Edition`, `Arrangement`, `Revision`,
  `SourceVariant` (Registry Phase 5, several marked "Reference
  Concept" / non-normative).
- Out of scope for `music-containers` entirely, not missing:
  resolution logic that takes a `VoiceReference` and looks up the
  actual `Voice` (a repository/registry pattern is an application/
  query-layer, L9, concern), and any `Voice`<->`Instrument`,
  `Voice`<->`Staff`, or `Voice`<->`Part` relationship (deferred to
  `music-relationship`, L6, or later container work).