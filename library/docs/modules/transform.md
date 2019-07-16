# transform

Layer L8 (`music-transform`). Path `src/transform/`, tests `tests/transform/`. One of three mutually-restricted L8 siblings (with `analysis` and `generation`); see `docs/modules/l8-notes.md` for the sibling rule.

## Provides

`Transformation` and its eleven concrete subtypes (`Transposition`, `Inversion`, `Retrograde`, `RetrogradeInversion`, `Augmentation`, `Diminution`, `Fragmentation`, `Expansion`, `Contraction`, `Displacement`, `Ornamentation`).

## Notes

Every concrete subtype owns one `Transformation` built internally at construction time, rather than a caller assembling the base type itself. `<subtype>_id`/`<subtype>_provenance` forward to the owned `Transformation` rather than duplicating identity/provenance on the subtype. `Fragmentation`/`Expansion`/`Contraction` share one array-of-`EntityID`-references-plus-count shape (extract/append/drop by reference, never by copying `NoteEvent` payloads). `Ornamentation` is the one subtype the Registry calls out as needing provenance most, since its one-to-many fan-out (one source event to several derived events) is the trickiest case to get right, though every subtype carries provenance via the base contract regardless. Referentially transparent throughout (TDD §140). No subtype has a mutator or a reference back into an input object it could alter.

## Dependencies

Depends on `identity` (L0), `core` (L1), `time`/`pitch` (L2), `events` (L3), `containers` (L4), `structure` (L5), all four L6 modules, and both L7 modules only. Never imports `analysis` or `generation`.
