# time

Layer L2 (`music-time`). Path `src/time/`, tests `tests/time/`. Depends on `identity` and `core` only. Sibling of `pitch`; the two do not depend on each other.

## Provides

`Duration` (magnitude), `MusicalTime` (position), `TimeSpan` (a positioned interval between two `MusicalTime`s), `TimeInstant` (a zero-duration point wrapping `MusicalTime`), and `TemporalAnchor` (wraps a `MusicalTime`, situates a higher-layer object at that position). All rational quantities are exact, never float or double (Law 19).


`MetronomeMarking` (a beat unit plus beats per minute, both exact rationals), `HistoricalPeriod` (a named stretch of calendar years, optionally open-ended), `NoteValue` (a written rhythmic value, breve through 1024th, with up to four dots), `TimeSignature` (a top number and an undotted bottom unit), and `Tuplet` (a ratio of notes played to notes replaced) are also provided.

## Notes

- `Duration` and `MusicalTime` are structurally identical (numerator/denominator, reduced to lowest terms) but are separate, non-convertible types. One is a magnitude, the other a position. There is no conversion between them, and none should be added.
- `TimeSpan`'s `boundary` vocabulary (`closed` / `open` / `left_closed` / `right_closed`) is provisional per Domain Spec §16.3, not frozen. It is implemented as given; a later revision to that vocabulary is not a breaking-change violation.
- `TimeSpan` requires `boundary` to be supplied explicitly at construction. There is no default. A deserializer built on top of `time_span_create` must reject a payload that omits it rather than assuming `closed`.
- `MusicalTime` has no lower bound enforced at this layer. Negative positions (pickup measures, for example) are legal here. What a negative position means is deferred to a later spec.
- `TemporalAnchor` carries no fields beyond the wrapped `MusicalTime`. It exists so callers building on `Occurrence` (L1) gain a temporal position without embedding position semantics into `Occurrence` itself.

## Dependencies

Depends on `identity` (L0) and `core` (L1) only.

- `MetronomeMarking` derives seconds from the marking (one beat unit lasts 60/bpm seconds) as an exact rational, never float/double (Law 19). Textual tempo words are not metronome markings; they live in `events/tempo_marking.h` and `frameworks/standard-markings/`.
- `HistoricalPeriod` is historical time, not musical time, and has nothing to do with `MusicalTime`/`Duration`. Years are inclusive on both ends, the end year is optional so a period still going never needs updating, and periods may overlap -- nothing here resolves that. The conventional periods themselves are `frameworks/western-periods/`.
- `NoteValue` is notation vocabulary, not a magnitude: it holds a power-of-two exponent (breve -1, whole 0, half 1, quarter 2, ...) and a dot count, and converts one way to a `Duration` (`note_value_duration`, exact, dots included). There is no conversion back, because many written values share a length. It carries no names, flags, or glyph conventions; those are `frameworks/standard-rhythm/`.
- `TimeSignature` is exactly the two numbers of a time signature: a top number (1 to 64) and an undotted bottom unit from the whole note to the sixteenth. It claims nothing about beats. In a simple meter the top counts beats over a beat unit; in a compound meter it counts divisions (6/8 is two beats of three eighths), and that reading is `frameworks/standard-meter/`. It is not a fraction: 2/4 and 4/8 have the same measure length (`time_signature_measure_duration`, top times the unit's length, true for simple and compound alike) but are not equal. A dotted bottom unit is rejected. `Measure` still holds only an opaque `MeterMap` region reference; `MeterMap` is not built yet.
- `Tuplet` is only a ratio: `actual` notes in the time of `normal` (3:2, 2:3, 5:4). It holds no note value or position; `tuplet_note_duration` and `tuplet_span` scale a `Duration` exactly. The counts are bounded (2-64 and 1-64) and unequal, and the ratio is not reduced, so 6:4 and 3:2 differ. Names and the written number are `frameworks/standard-rhythm/`; which meter a tuplet is borrowed from is `frameworks/standard-meter/`.
