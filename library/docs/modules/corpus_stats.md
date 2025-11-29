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

## Style statistics

Counts over what a Corpus's persisted interpretations say, as plain `CorpusStatsCounts` (a label and an integer count per entry, highest count first with ties in first-seen order, plus the total, so a label's frequency is the exact rational `count/total`). Each returns `CORPUS_STATS_NO_DATA`, leaving its output untouched, when nothing counts. Free a result with `corpus_stats_counts_free`.

A piece ref is a context id: a piece's interpretations are the ones persisted with that `context_id` (`persistence/persistence_context_query.h`), in the order they were stored. The existing `corpus_stats_most_common_chord` still resolves refs as Chord ids and is unchanged.

- `corpus_stats_chord_progressions(..., n, out)`: every run of `n` consecutive persisted Chords (`n` from 2 to 6; anything else is `CORPUS_STATS_ERR_INVALID_ARG`), labelled by the chord labels joined with `" > "`. A run never crosses from one piece into the next; a piece with fewer than `n` chords contributes nothing; a chord that cannot be loaded breaks the run it falls in.
- `corpus_stats_cadence_types`: the label of every persisted interpretation of claim value type `CORPUS_STATS_CADENCE_CLAIM_TYPE` (`cadence-detection-top-candidate`), whose content is the label as a NUL-terminated string.
- `corpus_stats_directed_intervals`: the signed semitone size of every persisted interpretation of claim value type `CORPUS_STATS_INTERVAL_CLAIM_TYPE` (`interval-analysis-directed-semitones`), whose content is one `long`; labels are signed (`+2`, `-3`, `+0`). It counts what interval analysis persisted, nothing more.

Claims are read as raw bytes through `persistence_list_claim_contents_for_context`, so `corpus_stats` needs no dependency on `core` or `analysis`; a claim whose content does not decode is skipped, as an unresolvable piece ref is. Adding a statistic over another claim type is one decoder and one public function.

**Not counted yet** (nothing is invented to fill them): phrase forms (period and sentence claims are packed structs defined in `analysis/`), rhythm patterns (not interpretations), and motive transformations (the motif claim carries only an occurrence length, no transformation kind). They need a plain, documented claim format before they can be counted here.

Counts reach `theory/` as plain values: `theory/style_model_build.h` builds a `StyleModel` from them and `theory/style_model_blend.h` blends two (see `docs/modules/theory.md`). Nothing here fits or trains a model, and nothing here can become a Rule or Constraint.

## Notes

- No caching or precomputation -- every call recomputes the tally from scratch by iterating the Corpus's current piece refs.
- Piece order is the order `corpus_api_list_piece_refs` returns.
- No float/double anywhere (Law 19) -- tallies are plain counts and frequencies are `count/total`.

## Deferred

- Phrase forms, rhythm patterns and motive transformations (above).
- Wiring into `ursatz-gui` or the Analyzer's style-profile report.
- Existence validation of piece refs at Corpus-membership time.
- Caching or precomputation of results.

## Dependencies

Depends on `identity` (L0, for `EntityID`), `persistence` (L9, for `PersistenceHandle`/`persistence_load_chord`), `interpretation` (for `Chord`/`chord_label`), and `corpus` (for `CorpusStoreHandle`/`corpus_api_list_piece_refs`) only.
