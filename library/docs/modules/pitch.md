# pitch

Layer L2 (`music-pitch`). Path `src/pitch/`, tests `tests/pitch/`. Depends on `identity` and `core` only. Sibling of `time`; the two do not depend on each other.

## Provides

`PitchIdentity` (an opaque token identifying a pitch), `PitchSpelling` (a written pitch name made of letter, accidental, and octave), `TuningSystem` (a tuning definition), `PitchClass` (an equivalence class of `PitchIdentity` under a caller-supplied relation), `PitchRealization` (a sounding/physical pitch as an exact rational ratio against a `TuningSystem`), `WrittenPitch` (a notated pitch, pre-transposition, wrapping `PitchSpelling`), and `SoundingPitch` (a sounding pitch, post-transposition, wrapping `PitchRealization`).

## Notes

- `PitchIdentity`'s equality is RESOLVED, not deferred: it is strict identity (value equality on the raw token bytes) by design, permanently. This answers "is this the same token," a different question from theory-relative equivalence (octave equivalence, enharmonic equivalence under some tuning, etc.), which is `PitchClass`'s job (`pitch/pitch_class.h`) -- `PitchClass` wraps a `PitchIdentity` together with a caller-supplied equivalence relation, so no particular theory is hard-coded into either type. `PitchIdentity` itself will not gain a second equality notion.
- `PitchIdentity`'s token is intentionally opaque and is not an `EntityID`. Its constructor takes a raw byte buffer, not an `EntityID*`, so there is no path to construct one from an EntityID-typed value.
- `PitchIdentity` construction takes no tuning-system parameters. No 12-TET, no A440, no MIDI note number. None should be added before Deliverable D exists.
- `PitchSpelling`'s `accidental` field is treated opaquely (an integer offset); its exact model is also deferred to Deliverable D. Enharmonic equivalents (C# and Db, for example) are intentionally not value-equal, even though they may sound identical under some tunings.
- `TuningSystem` holds only identity and a name (e.g. "12-TET", "just intonation"); it does not encode any tuning's actual ratio set. It does not build on `core/entity.h`'s `Entity`, mirroring `theory/framework.h`'s `Framework`, for the same category-boundary reason.
- `PitchClass` bakes in NO equivalence relation: the relation is supplied at construction as a `PitchClassEquivalenceFn` (a function pointer), the same caller-supplies-the-relation pattern `core/equality.h` already uses for `CanonicalizeFn`/`SemanticEqualsFn`. **Open judgment call, not silently decided:** this module does not pick a default equivalence relation (e.g. octave equivalence) for callers to use out of the box -- none is provided, tested only against a test-local stand-in (`tests/pitch/test_pitch_class.c`'s `mod12_equivalent`). Whoever needs a concrete relation (a Scale/Chord/Key consumer at L6+, most likely) must define and property-test their own. `pitch_class_equals` only compares two `PitchClass` values built under the *same* relation (by function-pointer identity); comparing across different relations always returns false.
- `PitchRealization` represents a sounding pitch as an exact rational ratio against a `TuningSystem`'s reference -- never float/double (Law 19). Frequency is derivable (`pitch_realization_frequency`) given a caller-supplied exact rational reference frequency (e.g. 440/1); no default reference is hard-coded. **Open judgment call, not silently decided:** "cents" is NOT supported. Converting a ratio to cents is a log2-based operation, irrational in the general case (12-TET's semitone, 2^(1/12), has no exact rational value), so exact cents support would require float/double and is out of scope until an exact-rational or fixed-point cents model is designed (Deliverable D).
- `WrittenPitch` and `SoundingPitch` are thin wrappers (`PitchSpelling` and `PitchRealization` respectively) with no fields of their own. Neither models the pre/post-transposition transformation itself -- there is no Transformation/Activity machinery yet to drive it.

## Dependencies

Depends on `identity` (L0) and `core` (L1) only.