# persistence

Layer L9 (`music-persistence`). Path `src/persistence/`, tests `tests/persistence/`. Depends on `identity`, `core`, `time`, `pitch`, `events`, `interpretation`, `relationship` and `structure` only (the last two for the repositories of their own types). Must not depend on `containers`, `context`, `semantics`, `theory`, `rules`, `transform`, `analysis`, `generation`, `io`, or `frameworks/`.

## Provides

SQLite-backed storage for a NoteEvent sequence plus the Chord/Key interpretations produced by analyzing it. This is deliberately NOT the full L9 `EntityRepository`/`RelationshipRepository`/`StructureRepository` contract from the System Registry -- just enough for a personal-scale tool to save and reload an analyzed piece, byte-for-byte equivalent (same values, not necessarily same pointers).

- `persistence_open`/`persistence_close` (`persistence.h`): lifecycle. Fail-fast on a bad path, permission error, or corrupt file -- no partial handle is ever returned.
- `persistence_store_note_events`/`persistence_load_note_events` (`persistence_note_events.h`): a time-ordered `NoteEvent` sequence, preserving id, exact-rational `TemporalAnchor` and `Duration`, pitch identity token, optional pitch spelling, optional voice reference, and provenance reference.
- `persistence_store_uncertainty`/`persistence_load_uncertainty` (`persistence_uncertainty.h`): the three implemented `Uncertainty` kinds (unknown, scalar confidence, candidate set), writing/reading only the columns/child rows valid for the given kind.
- `persistence_store_interpretation`/`persistence_load_interpretation` (`persistence_interpretation.h`): a full `Interpretation`, including its embedded `Uncertainty`. `claim` (a `Value`) is opaque by design -- its type string and content bytes are persisted verbatim and never decoded.
- `persistence_context_query.h`: read-only listing of what was persisted for a context (an `Interpretation`'s optional `context_id`; by convention a piece is a context, so its interpretations carry the piece's id): `persistence_list_chord_ids_for_context`, `persistence_list_interpretation_ids_for_context` (by claim value type) and `persistence_list_claim_contents_for_context` (the raw claim bytes, so a reader that knows a claim type's format can decode it without the `Value` type). Results are in store order, which is the order the analysis produced them. An unknown context is zero results, not an error.
- `persistence_store_chord`/`persistence_load_chord` and `persistence_store_key`/`persistence_load_key` (`persistence_chord.h`, `persistence_key.h`): thin wrappers over `persistence_interpretation` for `Chord`/`Key`'s embedded `Interpretation`, plus their own `label`.

- `persistence_relationship.h`: `persistence_store_relationship`, `persistence_load_relationship`, `persistence_list_relationships_by_source` and `persistence_list_relationships_by_target`. Every field of a `Relationship` round trips (id, source, predicate name, target, scope, context id, qualifiers, evidence id, uncertainty Value, provenance reference).
- Structure repositories, one header per type: `persistence_motif.h` (`persistence_store_motif`/`persistence_load_motif`), `persistence_motif_occurrence.h` (store, load, `persistence_list_motif_occurrences_by_motif_id`), `persistence_cadence.h` (store/load, the optional harmonic-context Value included) and `persistence_phrase_occurrence.h` (store, load, `persistence_list_phrase_occurrences_by_phrase_id`). Event id lists reload in the order they were stored. These cover exactly Motif, MotifOccurrence, Cadence and PhraseOccurrence; Theme, ThemeOccurrence, Figure and the other structure types have no repository.
- `persistence_support.h` is internal to `persistence/*.c`: savepoints, ordered id lists, optional Value and EntityID columns.

A stored Relationship, Motif, MotifOccurrence, Cadence or PhraseOccurrence is a descriptive record. Storing one adds no authority and promotes nothing to a Rule.

### Behavior of the new repositories

- **Duplicate id.** Storing an id that already exists returns `..._ERR_DUPLICATE_ID` and writes nothing: no overwrite, no second row. The older repositories (note events, interpretations, chords, keys) report a duplicate key as the generic `..._ERR_DB`; they are unchanged.
- **Missing id.** A load of an id that is not stored returns `..._ERR_NOT_FOUND` and leaves `*out` untouched. (The older load functions return `ERR_DB` for a missing id.)
- **No partial rows.** A store that writes more than one table runs inside a `SAVEPOINT`, rolled back on any failure, so a store that fails partway leaves no rows. A savepoint nests, so the store also works inside a caller's own transaction.
- **No cross-repository foreign keys.** A Relationship's source, target, context and evidence, an occurrence's motif or phrase, and every event id in a list are EntityID text values, never checked against another table: a Relationship may point at any entity, and an occurrence's Motif may be stored later or never. The only foreign keys are from a child event-id table to its own parent.
- **Lists** return a newly allocated array of newly created objects in insertion (rowid) order, stable across calls and reopens. No match is OK with a NULL array and a count of 0.
- **Opening an older database.** The new tables are `CREATE TABLE IF NOT EXISTS` and the existing tables are not changed, so a database created before them opens and gains them (`tests/persistence/test_persistence_old_schema.c`).

## Schema

- `note_events` -- one row per `NoteEvent`, with an explicit `sequence_order` column (not row id) preserving original order. `pitch_spelling_letter`/`pitch_spelling_accidental`/`pitch_spelling_octave` are nullable and always NULL together, matching `optional_pitch_spelling`'s absent case -- there is no separate presence flag.
- `uncertainties` / `uncertainty_candidates` -- one `uncertainties` row per `Uncertainty`; `uncertainty_candidates` child rows only when `kind` is `CANDIDATE_SET`.
- `interpretations` -- one row per `Interpretation`, FK'd to its `uncertainties` row. `claim_value_type`/`claim_value_content`/`claim_value_content_size` hold the opaque `Value` verbatim.
- `chords` / `keys` -- thin FK'd wrappers over `interpretations`, adding only `label`. Neither `Chord` nor `Key` exposes an id of its own beyond its embedded `Interpretation`'s id, so that same `EntityID` value is used as both the `interpretations` row key and the `chords`/`keys` row key, rather than inventing a second identifier neither type has.

- `relationships` -- one row per `Relationship`; `scope`, `qualifiers` and `uncertainty` are each a type/content/size column group (all NULL when absent), `provenance_state` and `provenance_id` as for interpretations. Indexed by `source_id` and by `target_id`. The uncertainty is the Relationship's own Value, so it does not use the `uncertainties` tables.
- `motifs`, `motif_occurrences`, `cadences`, `phrase_occurrences` -- one row per object (`motif_occurrences.motif_id` and `phrase_occurrences.phrase_id` are indexed; `cadences` carries the harmonic-context Value columns). `motif_events`, `motif_occurrence_events`, `cadence_events` and `phrase_occurrence_events` -- the ordered event ids of each (`parent_id`, `position`, `event_id`), FK'd to their own parent only.

## Notes

- No float/double conversion anywhere in the storage path (Law 19): every rational field (`TemporalAnchor`/`MusicalTime`, `Duration`, `Uncertainty`'s confidence and candidate weights) round-trips as exact `numerator`/`denominator` integer columns.
- `optional_pitch_spelling` on `NoteEvent` round-trips exactly like every other field. Backfilling it for already-persisted rows from an earlier schema version is out of scope: `pitch_token`'s `PitchIdentity` bytes are opaque by design (`pitch/pitch_identity.h`), with no decoder anywhere in this codebase, so a spelling cannot be recovered from an already-stored row after the fact -- only a re-import from the original source file can populate it for old data.
- `Value` (claim) is opaque per `core/value.h`'s own design -- persistence stores type + content bytes + size and never interprets content, matching how every other consumer of `Value` treats it.

## Deferred

Deliberate scope cuts for this pass, not gaps to be filled incidentally:

- `EntityRepository` and `ProvenanceRepository`, and the `StructureRepository` for types other than Motif, MotifOccurrence, Cadence and PhraseOccurrence. Ursatz-GUI's own relationship table (`ursatz_gui_relationships`) can now be retired by the GUI using `persistence_relationship.h`; that migration is not part of this library.
- Any persistence for period, sentence, form, voice-leading, or scale-analysis output beyond the Motif, MotifOccurrence, Cadence, PhraseOccurrence and Relationship types above. Those analyzers are built (`docs/modules/analysis.md`); their results have no repository here. Ursatz-GUI stores Relationship output in its own stopgap table.
- Schema migration/versioning beyond whatever prevents a crash on schema changes during this module's own development.
- Any query/filtering API beyond "load by id" -- a list/browse API belongs to a future GUI scaffold task, not this one.
- The remaining `UncertaintyKind` shapes not yet implemented upstream (interval estimate, probability distribution, credible interval, qualitative uncertainty) have no persistence here either -- each gets its own child table (the same pattern `uncertainty_candidates` establishes) when it is actually built upstream, not a speculative table now.

## Dependencies

Depends on `identity` (L0), `core` (L1), `time` and `pitch` (L2), `events` (L3), `interpretation` (L6, for `Chord`/`Key`/`Interpretation`/`Uncertainty`'s own struct shapes), `relationship` and `structure` (for the types the new repositories persist) only.
