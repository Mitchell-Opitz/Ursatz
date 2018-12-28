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

A module must not import anything outside its own layer's allowed
dependencies. Each module has an architecture test enforcing this
(e.g. `tests/identity/test_architecture_identity.c`,
`tests/core/test_architecture_core.c`, `tests/time/test_architecture_time.c`,
`tests/pitch/test_architecture_pitch.c`).

## Modules

| Module     | Layer | Path             | Tests                |
|------------|-------|------------------|-----------------------|
| identity   | L0    | `src/identity/`  | `tests/identity/`     |
| core       | L1    | `src/core/`      | `tests/core/`         |
| time       | L2    | `src/time/`      | `tests/time/`         |
| pitch      | L2    | `src/pitch/`     | `tests/pitch/`        |

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