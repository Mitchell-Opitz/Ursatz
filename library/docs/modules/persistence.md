# persistence

Layer L9 (`music-persistence`). Path `src/persistence/`, tests `tests/persistence/`. Depends on `identity`, `core`, `time`, `pitch`, `events`, and `interpretation` only. Must not depend on `containers`, `structure`, `context`, `semantics`, `relationship`, `theory`, `rules`, `transform`, `analysis`, `generation`, `io`, or `frameworks/`.

## Provides

SQLite-backed storage for a NoteEvent sequence plus the Chord/Key interpretations produced by analyzing it. This is deliberately NOT the full L9 `EntityRepository`/`RelationshipRepository`/`StructureRepository` contract from the System Registry -- just enough for a personal-scale tool to save and reload an analyzed piece, byte-for-byte equivalent (same values, not necessarily same pointers).

- `persistence_open`/`persistence_close` (`persistence.h`): lifecycle. Fail-fast on a bad path, permission error, or corrupt file -- no partial handle is ever returned.
- `persistence_store_note_events`/`persistence_load_note_events` (`persistence_note_events.h`): a time-ordered `NoteEvent` sequence, preserving id, exact-rational `TemporalAnchor` and `Duration`, pitch identity token, optional voice reference, and provenance reference.
- `persistence_store_uncertainty`/`persistence_load_uncertainty` (`persistence_uncertainty.h`): the three implemented `Uncertainty` kinds (unknown, scalar confidence, candidate set), writing/reading only the columns/child rows valid for the given kind.
- `persistence_store_interpretation`/`persistence_load_interpretation` (`persistence_interpretation.h`): a full `Interpretation`, including its embedded `Uncertainty`. `claim` (a `Value`) is opaque by design -- its type string and content bytes are persisted verbatim and never decoded.
- `persistence_store_chord`/`persistence_load_chord` and `persistence_store_key`/`persistence_load_key` (`persistence_chord.h`, `persistence_key.h`): thin wrappers over `persistence_interpretation` for `Chord`/`Key`'s embedded `Interpretation`, plus their own `label`.

## Schema

- `note_events` -- one row per `NoteEvent`, with an explicit `sequence_order` column (not row id) preserving original order.
- `uncertainties` / `uncertainty_candidates` -- one `uncertainties` row per `Uncertainty`; `uncertainty_candidates` child rows only when `kind` is `CANDIDATE_SET`.
- `interpretations` -- one row per `Interpretation`, FK'd to its `uncertainties` row. `claim_value_type`/`claim_value_content`/`claim_value_content_size` hold the opaque `Value` verbatim.
- `chords` / `keys` -- thin FK'd wrappers over `interpretations`, adding only `label`. Neither `Chord` nor `Key` exposes an id of its own beyond its embedded `Interpretation`'s id, so that same `EntityID` value is used as both the `interpretations` row key and the `chords`/`keys` row key, rather than inventing a second identifier neither type has.

## Notes

- No float/double conversion anywhere in the storage path (Law 19): every rational field (`TemporalAnchor`/`MusicalTime`, `Duration`, `Uncertainty`'s confidence and candidate weights) round-trips as exact `numerator`/`denominator` integer columns.
- `optional_pitch_spelling` on `NoteEvent` is out of scope for this pass: no producer in this codebase populates it yet (the MIDI importer never sets it), so there is nothing real to round-trip.
- `Value` (claim) is opaque per `core/value.h`'s own design -- persistence stores type + content bytes + size and never interprets content, matching how every other consumer of `Value` treats it.

## Deferred

Deliberate scope cuts for this pass, not gaps to be filled incidentally:

- The full L9 `EntityRepository`/`RelationshipRepository`/`StructureRepository` contract from the System Registry.
- Any persistence for `Motif`, `Phrase Segmentation`, `Voice Leading`, `Scale Analysis`, or `Cadence Detection` -- all still unbuilt/deferred in `analysis`/`structure`.
- Schema migration/versioning beyond whatever prevents a crash on schema changes during this module's own development.
- Any query/filtering API beyond "load by id" -- a list/browse API belongs to a future GUI scaffold task, not this one.
- The remaining `UncertaintyKind` shapes not yet implemented upstream (interval estimate, probability distribution, credible interval, qualitative uncertainty) have no persistence here either -- each gets its own child table (the same pattern `uncertainty_candidates` establishes) when it is actually built upstream, not a speculative table now.

## Dependencies

Depends on `identity` (L0), `core` (L1), `time` and `pitch` (L2), `events` (L3), and `interpretation` (L6, for `Chord`/`Key`/`Interpretation`/`Uncertainty`'s own struct shapes) only.
