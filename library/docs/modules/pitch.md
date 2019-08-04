# pitch

Layer L2 (`music-pitch`). Path `src/pitch/`, tests `tests/pitch/`. Depends on `identity` and `core` only. Sibling of `time`; the two do not depend on each other.

## Provides

`PitchIdentity` (an opaque token identifying a pitch) and `PitchSpelling` (a written pitch name made of letter, accidental, and octave).

## Notes

- `PitchIdentity`'s equality is RESOLVED, not deferred: it is strict identity (value equality on the raw token bytes) by design, permanently. This answers "is this the same token," a different question from theory-relative equivalence (octave equivalence, enharmonic equivalence under some tuning, etc.), which is `PitchClass`'s job (`pitch/pitch_class.h`) -- `PitchClass` wraps a `PitchIdentity` together with a caller-supplied equivalence relation, so no particular theory is hard-coded into either type. `PitchIdentity` itself will not gain a second equality notion.
- `PitchIdentity`'s token is intentionally opaque and is not an `EntityID`. Its constructor takes a raw byte buffer, not an `EntityID*`, so there is no path to construct one from an EntityID-typed value.
- `PitchIdentity` construction takes no tuning-system parameters. No 12-TET, no A440, no MIDI note number. None should be added before Deliverable D exists.
- `PitchSpelling`'s `accidental` field is treated opaquely (an integer offset); its exact model is also deferred to Deliverable D. Enharmonic equivalents (C# and Db, for example) are intentionally not value-equal, even though they may sound identical under some tunings.
- `PitchClass`, `PitchRealization`, `TuningSystem`, `WrittenPitch`, and `SoundingPitch` are Phase 2/3 (Deliverable D) and out of scope for this module.

## Dependencies

Depends on `identity` (L0) and `core` (L1) only.