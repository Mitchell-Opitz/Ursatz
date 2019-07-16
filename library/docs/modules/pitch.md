# pitch

Layer L2 (`music-pitch`). Path `src/pitch/`, tests `tests/pitch/`. Depends on `identity` and `core` only. Sibling of `time`; the two do not depend on each other.

## Provides

`PitchIdentity` (an opaque token identifying a pitch) and `PitchSpelling` (a written pitch name made of letter, accidental, and octave).

## Notes

- `PitchIdentity`'s equality is a placeholder, value equality on the raw token bytes. The true equivalence relation is deferred to Deliverable D, which does not exist yet. Expect this to be revised.
- `PitchIdentity`'s token is intentionally opaque and is not an `EntityID`. Its constructor takes a raw byte buffer, not an `EntityID*`, so there is no path to construct one from an EntityID-typed value.
- `PitchIdentity` construction takes no tuning-system parameters. No 12-TET, no A440, no MIDI note number. None should be added before Deliverable D exists.
- `PitchSpelling`'s `accidental` field is treated opaquely (an integer offset); its exact model is also deferred to Deliverable D. Enharmonic equivalents (C# and Db, for example) are intentionally not value-equal, even though they may sound identical under some tunings.
- `PitchClass`, `PitchRealization`, `TuningSystem`, `WrittenPitch`, and `SoundingPitch` are Phase 2/3 (Deliverable D) and out of scope for this module.

## Dependencies

Depends on `identity` (L0) and `core` (L1) only.
