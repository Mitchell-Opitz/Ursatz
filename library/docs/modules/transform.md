# transform

Layer L8 (`music-transform`). Path `src/transform/`, tests `tests/transform/`. One of three mutually-restricted L8 siblings (with `analysis` and `generation`); see `docs/modules/l8-notes.md` for the sibling rule.

## Provides

`Transformation` and its twelve concrete subtypes (`Transposition`, `Inversion`, `Retrograde`, `RetrogradeInversion`, `Augmentation`, `Diminution`, `Fragmentation`, `Expansion`, `Contraction`, `Displacement`, `Ornamentation`, `IntervallicManipulation`).

## Notes

Every concrete subtype owns one `Transformation` built internally at construction time, rather than a caller assembling the base type itself. `<subtype>_id`/`<subtype>_provenance` forward to the owned `Transformation` rather than duplicating identity/provenance on the subtype. `Fragmentation`/`Expansion`/`Contraction` share one array-of-`EntityID`-references-plus-count shape (extract/append/drop by reference, never by copying `NoteEvent` payloads). `Ornamentation` is the one subtype the Registry calls out as needing provenance most, since its one-to-many fan-out (one source event to several derived events) is the trickiest case to get right, though every subtype carries provenance via the base contract regardless. Referentially transparent throughout (TDD §140). No subtype has a mutator or a reference back into an input object it could alter.

Every concrete subtype now implements real transformation logic via a `<subtype>_apply` function, not just construction. Each `_apply` returns a `TransformResult` (`transform/transform_result.h`): an owned array of newly-allocated `NoteEvent`s, never a mutation of the input. All arithmetic is exact-rational (Law 19); no subtype touches `float`/`double`.

`PitchIdentity` (L2) is intentionally opaque -- an arbitrary caller-defined token with no numeric representation. `Transposition`, `Inversion`, and `RetrogradeInversion` need to add intervals and reflect about an axis, which is impossible against an opaque token. `PitchNumericValue` (`pitch/pitch_numeric_value.h`, L2) was added alongside `PitchIdentity`, not in place of it: an exact-rational position tied to a copy of a `TuningSystem`, supporting `add_interval`, `distance`, and `reflect_about_axis`, plus a one-directional `to_identity` conversion. There is deliberately no generic decode from `PitchIdentity` back to a numeric value -- callers that need numeric transposition/inversion supply the `PitchNumericValue` themselves; `PitchIdentity`'s opacity is preserved for every other subtype.

`IntervallicManipulation` changes the size of each interval between successive notes while keeping its direction. It takes one signed semitone adjustment per interval over caller-supplied `PitchNumericValue`s; the first note stays put and each later note moves by the running sum. Rhythm, order, articulation and contour are preserved.

`RetrogradeInversion` composes `Retrograde` and `Inversion`'s own `_apply` rather than duplicating either's logic, using `transform_result_take_event_at` to move ownership of an event out of one `TransformResult` and into another.

`Ornamentation`'s `_apply` takes a caller-supplied `OrnamentEventSpec` array (offset/duration/pitch, i.e. the "derivation rule") -- `Ornamentation` itself hardcodes no ornament vocabulary (no built-in trill/turn/mordent). Each derived event's id and `ProvenanceReference` are copies of what `ornamentation_create` already declared and validated, not newly minted at apply time.

Two judgment calls, made explicitly rather than left implicit: `Contraction`'s "remove a declared sub-range" is implemented as "remove by declared `EntityID` set" (an arbitrary subset, not a contiguous range) -- flagged in `contraction.h`'s construction doc comment. `Expansion`'s insertion point is a single caller-supplied index (`position`) into the source sequence, not a more general splice description.

`Retrograde`, `Augmentation`, and `Diminution` currently implement only one dimension each (`RETROGRADE_DIMENSION_EVENT_ORDER`, `AUGMENTATION_DIMENSION_DURATIONS`/`DIMINUTION_DIMENSION_DURATIONS` respectively); their other declared dimensions return a dedicated "not implemented" error rather than silently doing nothing.
`transform_support.h` is an internal helper for the transform files only and is not part of the public API. It holds the note clone helpers every transform used to carry its own copy of (`transform_support_clone_note_event`, `_with_new_pitch`, `_with_new_duration`, `_with_new_onset`, the pitch, spelling and voice clones, `make_derived_id`, `make_provenance_from`, `dimension_name`). The copies differed only in the one field a transform changes, which is the argument of the matching helper.


## Dependencies

Depends on `identity` (L0), `core` (L1), `time`/`pitch` (L2), `events` (L3), `containers` (L4), `structure` (L5), all four L6 modules, and both L7 modules only. Never imports `analysis` or `generation`.