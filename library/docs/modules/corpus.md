# corpus

Layer L9 (`music-corpus`), sibling to `music-persistence`. Path `src/corpus/`, tests `tests/corpus/`. Depends on `identity` and `persistence` only. Must not depend on `core`, `time`, `pitch`, `events`, `containers`, `structure`, `context`, `semantics`, `relationship`, `interpretation`, `theory`, `rules`, `transform`, `analysis`, `generation`, `io`, or `frameworks/`. Cross-cutting: Analyzer and Generator may each depend on `corpus` as a peer, but neither routes through the other.

## Provides

A named collection with a stable id, grouping references to already-persisted pieces. Corpus never stores or duplicates piece data -- membership is reference-only, by the same EntityID value Persistence already uses to identify a stored Interpretation/Chord/Key.

- `corpus_create`/`corpus_destroy` (`corpus.h`): in-memory `Corpus` type holding only id and name. Fail-fast on a null id or name.
- `persistence_store_corpus`/`persistence_load_corpus`/`persistence_list_corpora` (`persistence_corpus.h`): durable storage for a Corpus's id and name in the `corpora` table.
- `corpus_add_piece_ref`/`corpus_remove_piece_ref`/`corpus_list_piece_refs` (`persistence_corpus_membership.h`): durable membership in the `corpus_members` table.
- `corpus_api.h`: the public API -- see below.

## Public API (`corpus_api.h`)

This is the boundary both Analyzer and Generator call as peers to use Corpus, the same kind of surface `persistence.h` already exposes for Analyzer to link against. It exposes exactly the operations above -- no new capability, no stats, no validation beyond what the internals already enforce. `corpus.h`, `persistence_corpus.h`, and `persistence_corpus_membership.h` are internal; only `corpus_api.h` is meant to be included from outside `corpus/` (enforced by the architecture guard test).

`CorpusStoreHandle` is opaque and does not expose `PersistenceHandle` -- consumers link against Corpus without pulling in Persistence's own types. Corpora and piece refs are returned as plain owned strings (`CorpusApiEntry { char *id; char *name; }`, and `char **` for piece refs), not the internal `Corpus` type.

- `corpus_api_open(path, out)` / `corpus_api_close(handle)`: store lifecycle, mirroring `persistence_open`/`persistence_close`. `corpus_api_open` fails fast (`CORPUS_API_ERR_OPEN_FAILED`) with no handle allocated on a bad path, permission error, or corrupt file.
- `corpus_api_create_corpus(handle, id, name)`: creates and persists a Corpus with a caller-supplied id and name. Fails on a null/invalid id or name (`CORPUS_API_ERR_INVALID_ID`/`CORPUS_API_ERR_INVALID_NAME`).
- `corpus_api_list_corpora(handle, out_entries, out_count)` / `corpus_api_free_corpus_list(entries, count)`: lists every Corpus as an array of `CorpusApiEntry`; an empty store is not an error (`*out_count == 0`, `*out_entries == NULL`). Free the result with `corpus_api_free_corpus_list`.
- `corpus_api_add_piece_ref(handle, corpus_id, piece_ref)` / `corpus_api_remove_piece_ref(handle, corpus_id, piece_ref)`: fail-fast, no silent no-ops -- adding a reference already present is an error (`CORPUS_API_ERR_ALREADY_MEMBER`), removing one not present is an error (`CORPUS_API_ERR_NOT_MEMBER`).
- `corpus_api_list_piece_refs(handle, corpus_id, out_refs, out_count)`: lists every piece ref in a Corpus as a `char **`; caller frees each string and the array. An empty Corpus is not an error.

Every function returns a `CorpusApiResult` (`CORPUS_API_OK` on success); DB-layer failures not covered by a more specific code map to `CORPUS_API_ERR_DB`, allocation failures to `CORPUS_API_ERR_ALLOC`.

## Schema

- `corpora` -- one row per Corpus (id, name).
- `corpus_members` -- one row per (corpus_id, piece_ref) pair, unique on the pair. `piece_ref` is an opaque, caller-supplied string (an Interpretation/Chord/Key's own EntityID value); this module never resolves or validates it against anything actually stored.

Both tables are created on demand (`CREATE TABLE IF NOT EXISTS`) by `corpus/persistence_corpus.c` and `corpus/persistence_corpus_membership.c` respectively, rather than being folded into `persistence.c`'s own up-front schema -- `persistence/` cannot depend on `corpus/` (the architecture guard forbids it), so Corpus's own tables must be created from Corpus's side of the boundary.

## Notes

- Names are not required to be unique -- two Corpora may share a name; only id disambiguates.
- Fail-fast, no silent no-ops: `corpus_add_piece_ref` on a reference already present in a Corpus is an error (`PERSISTENCE_CORPUS_MEMBERSHIP_ERR_ALREADY_MEMBER`); `corpus_remove_piece_ref` on a reference not present is an error (`PERSISTENCE_CORPUS_MEMBERSHIP_ERR_NOT_MEMBER`).
- No float/double anywhere (Law 19 has nothing to apply to here -- Corpus carries no numeric fields in this pass).

## Deferred

Deliberate scope cuts for this pass, not gaps to be filled incidentally:

- Statistics, feature extraction, or any cross-piece computation over a Corpus's membership -- a later pass (`feat/corpus-stats-minimal`).
- Actual wiring of `ursatz-gui` or Generator against `corpus_api.h` -- later passes (`feat/corpus-tool`, Generator integration); this module only exposes the API surface.
- Provenance of Corpus membership (who added what, when).
- Tags, labels, or "feeling" metadata on Corpus entries.
- Existence validation of `piece_ref` against Persistence's own tables -- a reference to a piece that was never actually stored, or was since deleted, is not detected here.

## Dependencies

Depends on `identity` (L0, for `EntityID`) and `persistence` (L9, for `PersistenceHandle`/`persistence_open`/`persistence_db`) only.
