# time

Layer L2 (`music-time`). Path `src/time/`, tests `tests/time/`. Depends on `identity` and `core` only. Sibling of `pitch`; the two do not depend on each other.

## Provides

`Duration` (magnitude), `MusicalTime` (position), `TimeSpan` (a positioned interval between two `MusicalTime`s), `TimeInstant` (a zero-duration point wrapping `MusicalTime`), and `TemporalAnchor` (wraps a `MusicalTime`, situates a higher-layer object at that position). All rational quantities are exact, never float or double (Law 19).

## Notes

- `Duration` and `MusicalTime` are structurally identical (numerator/denominator, reduced to lowest terms) but are separate, non-convertible types. One is a magnitude, the other a position. There is no conversion between them, and none should be added.
- `TimeSpan`'s `boundary` vocabulary (`closed` / `open` / `left_closed` / `right_closed`) is provisional per Domain Spec §16.3, not frozen. It is implemented as given; a later revision to that vocabulary is not a breaking-change violation.
- `TimeSpan` requires `boundary` to be supplied explicitly at construction. There is no default. A deserializer built on top of `time_span_create` must reject a payload that omits it rather than assuming `closed`.
- `MusicalTime` has no lower bound enforced at this layer. Negative positions (pickup measures, for example) are legal here. What a negative position means is deferred to a later spec.
- `TemporalAnchor` carries no fields beyond the wrapped `MusicalTime`. It exists so callers building on `Occurrence` (L1) gain a temporal position without embedding position semantics into `Occurrence` itself.

## Dependencies

Depends on `identity` (L0) and `core` (L1) only.
