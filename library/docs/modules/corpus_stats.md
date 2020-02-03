# corpus_stats

Sibling to `music-corpus`. Path `src/corpus_stats/`, tests `tests/corpus_stats/`. Depends on `identity`, `persistence`, `interpretation` (for `Chord`/`chord_label`), and `corpus` (for `CorpusStoreHandle` and its piece-ref listing) only. Must not depend on `core`, `time`, `pitch`, `events`, `containers`, `structure`, `context`, `semantics`, `relationship`, `theory`, `rules`, `transform`, `analysis`, `generation`, `io`, or `frameworks/`.

## Provides

Corpus Statistics: the first cross-piece computation over a Corpus's membership (a distinct Registry Concept from Corpus's own membership operations). Kept as its own module rather than folded into `corpus_api.h` so `corpus/` itself stays dependency-pure (identity + persistence only, per `docs/modules/corpus.md`).

**Results here are explicitly non-authoritative**: they describe what a Corpus's already-persisted Chord interpretations happen to say, not a framework-validated musical truth. This is the first output Theory's Tendency/StatisticalModel will eventually consume as an opaque value (per Registry) -- the public API exposes only a plain result value, no internal types.

- `corpus_stats_api.h`: the public API -- see below. Internal to `corpus_stats/*.c` otherwise; only `corpus_stats_api.h` is meant to be included from outside `corpus_stats/` (enforced by the architecture guard test).

## Public API (`corpus_stats_api.h`)

`corpus_stats_most_common_chord(corpus_handle, persistence_handle, corpus_id, out_chord_label)`: computes the most frequently occurring chord label across every piece ref in the given Corpus that resolves to a stored Chord.

- Takes both a `CorpusStoreHandle` and a `PersistenceHandle` explicitly -- this is the first operation that reads across the Corpus/Persistence module boundary, so the two handles must be opened against the same underlying store (piece refs recorded by Corpus are resolved as Chord ids by Persistence).
- **Skip, don't fail, on an unresolvable piece ref**: a piece ref that doesn't resolve to a stored Chord (no existence validation exists for Corpus membership today, per `corpus-minimal`'s design) is skipped silently and the tally continues. This is a deliberate exception to this codebase's general fail-fast convention -- a read-only aggregate query should not fail its whole computation over one bad ref, unlike a fail-fast mutation.
- **Deterministic tie-break**: when multiple chord labels are tied for the highest count, the first one encountered in piece-ref iteration order wins.
- **Empty result is not an error**: if the Corpus has zero piece refs that resolve to a stored Chord (including an empty Corpus), returns `CORPUS_STATS_NO_DATA` and leaves `*out_chord_label` untouched -- distinct from both success and any error code, so callers can't mistake "no data" for a fabricated chord.
- On success (`CORPUS_STATS_OK`), `*out_chord_label` is a newly allocated, owned, NUL-terminated string the caller must free.

Every function returns a `CorpusStatsResult` (`CORPUS_STATS_OK` on success); DB-layer failures not covered by a more specific code map to `CORPUS_STATS_ERR_DB`, allocation failures to `CORPUS_STATS_ERR_ALLOC`.

## Notes

- No caching or precomputation -- every call recomputes the tally from scratch by iterating the Corpus's current piece refs.
- One statistic only in this pass (most-common-chord); other statistics (key frequency, progressions, etc.) are deferred to later passes that would extend this module, not `corpus/`.
- No float/double anywhere (Law 19 has nothing to apply to here -- tallies are plain counts).

## Deferred

- Any statistic beyond most-common-chord (key frequency, progressions, etc.).
- Wiring into `ursatz-gui` or Theory/Tendency -- this module only builds the library-level capability.
- Existence validation of piece refs at Corpus-membership time -- a separate, larger decision that this module's skip-on-unresolvable behavior works around rather than resolves.
- Caching or precomputation of results.

## Dependencies

Depends on `identity` (L0, for `EntityID`), `persistence` (L9, for `PersistenceHandle`/`persistence_load_chord`), `interpretation` (for `Chord`/`chord_label`), and `corpus` (for `CorpusStoreHandle`/`corpus_api_list_piece_refs`) only.
