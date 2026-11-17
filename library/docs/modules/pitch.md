# pitch

[Docs index](../README.md): the Ursatz docs by topic.

Layer L2 (`music-pitch`). Path `model/pitch/`, tests `tests/pitch/`. Depends on `identity` and `core` only. Sibling of `time`; the two do not depend on each other.

## Provides

`PitchIdentity` (an opaque token identifying a pitch), `PitchSpelling` (a written pitch name made of letter, accidental, and octave), `TuningSystem` (a tuning definition), `KeySignature` (an ordered list of letter and accidental-offset entries, offsets -2 to +2), and `PitchMaterial` (a K1 pitch material: a collection of pitch classes above a center, with its family, brightness and color notes; it moved here from `theory/knowledge`, and the knowledge snapshot reads it from the entries).

The notation types `Clef`, `StaffPosition`, `Accidental` and `GenericInterval`, and clef resolution, were deleted (P55): no application and no framework used them.

## Notes

- `PitchIdentity`'s equality is RESOLVED, not deferred: it is strict identity (value equality on the raw token bytes, read with `pitch_identity_token`) by design, permanently. This answers "is this the same token," a different question from theory-relative equivalence (octave equivalence, enharmonic equivalence under some tuning, etc.), which no kernel type holds. `PitchIdentity` itself will not gain a second equality notion.
- `PitchIdentity`'s token is intentionally opaque and is not an `EntityID`. Its constructor takes a raw byte buffer, not an `EntityID*`, so there is no path to construct one from an EntityID-typed value.
- `PitchIdentity` construction takes no tuning-system parameters. No 12-TET, no A440, no MIDI note number. None should be added before Deliverable D exists.
- `PitchSpelling`'s `accidental` field is treated opaquely (an integer offset); its exact model is also deferred to Deliverable D. Enharmonic equivalents (C# and Db, for example) are intentionally not value-equal, even though they may sound identical under some tunings.
- `TuningSystem` holds only identity and a name (e.g. "12-TET", "just intonation"); it does not encode any tuning's actual ratio set. It does not build on `core/entity.h`'s `Entity`, mirroring `theory/framework.h`'s `Framework`, for the same category-boundary reason.
- `KeySignature` is a value holding entries in written order (letter plus an offset from `KEY_SIGNATURE_MIN_ACCIDENTAL` to `KEY_SIGNATURE_MAX_ACCIDENTAL`, -2 to +2, the same integer `PitchSpelling` stores); the empty signature is valid and is the one with no sharps or flats. It carries no theory: mixed, repeated, or out-of-order entries are representable. The conventional order (F C G D A E B, flats reversed), the limit of seven, and which key a signature belongs to are rules of `theory/tonality/`. It has no `EntityID` and no mutator.
- `pitch/pitch_arithmetic.h` is the public spelling arithmetic, theory-neutral (no key, mode, or scale): `pitch_letter_semitone` (C=0 ... B=11), `pitch_letter_index` (C=0 ... B=6), `pitch_letter_offset` (wraps both ways), `pitch_class_of_letter` (0 to 11), `pitch_spelling_fifths` and `pitch_spelling_from_fifths` (line of fifths, C=0, G=1, F=-1, F#=6, Bb=-2; each sharp adds 7), `pitch_spelling_name` and `pitch_spelling_parse_name` (`C`, `F#`, `Bb`, `Cx`, `Ebb`; parse takes `#`, `b`, `x`, `bb`, `##` and an optional octave, and rejects trailing text), and `pitch_class_name`. It assumes twelve semitones to the octave (the MIDI layout), the one tuning assumption in this module; enharmonic spellings stay distinct. Frameworks use these instead of private letter tables.
- `pitch/midi_pitch.h` is the one reading of a MIDI number: `pitch_class_of_midi` (0 to 11 for any int), `pitch_spelling_of_midi` (octave 4 C is 60; sharps for `key_fifths >= 0`, flats below; the caller destroys the spelling), `key_signature_from_fifths` (-7 to 7; sharps in the order F C G D A E B, flats in the reverse) and `key_tonic_name` (C, Db, D, Eb, E, F, F#, G, Ab, A, Bb, B by tonic pitch class). Callers use these instead of their own mod-12, spelling or fifths code.
- `pitch/pitch_material.h` is the value of one K1 material entry: `steps` above the center, an optional descending form, `key_word`, `family`, an optional `brightness_rank`, `color_notes` against major, and `variant_of` and `rotation_of` pointers into the snapshot that owns it. `pitch_material_step`, `pitch_material_contains`, `pitch_material_mask` (12 bits on a center), `pitch_material_third` and `pitch_material_is_diatonic` read it. The knowledge snapshot (`theory/knowledge`) builds it.
- Degree and key signature functions are `theory/tonality/tonal_key.h` (see that README). The major-scale tables in `common-practice-minimal`, `standard-harmony` and `galant-schemas` stay: those frameworks do not depend on standard-tonality, and frameworks may not depend on each other.

## Dependencies

Depends on `identity` (L0) and `core` (L1) only.