# query

[Docs index](../README.md): the Ursatz docs by topic.

Sibling to `music-corpus` and `corpus_stats`. Path `src/query/`, tests `tests/query/`. Depends on `identity`, `core` (for `ProvenanceReference`), `semantics` (for `ContourDirection`), `persistence` (for `PersistenceHandle` and its context queries) and `corpus` (for `CorpusStoreHandle` and its piece-ref listing) only. Must not depend on `time`, `pitch`, `events`, `containers`, `structure`, `context`, `interpretation`, `relationship`, `theory`, `rules`, `transform`, `analysis`, `generation`, `io`, `corpus_stats` or `frameworks/` -- in particular not on `analysis`: a search returns what analysis has already persisted, it never runs analysis. Enforced by `tests/query/test_architecture_query.c`.

## Provides

Similarity search over persisted melodic material, with exact-rational scores:

- `similarity_model.h`: `SimilarityModel`, which dimensions count (interval, rhythm, contour) and an exact weight for each.
- `similarity_scoring.h`: how alike two sequences are on each declared dimension, and the weighted combination.
- `search_result.h`: `SearchResult`, one match with its criteria and evidence.
- `corpus_search.h`: `corpus_search_melodic`, a search of a Corpus's persisted interval claims.

`query_fraction.h` is internal. Only the four headers above may be included from outside `query/`.

## Similarity is explicit-dimension

The caller declares the dimensions that count and a weight for each; there is no undeclared similarity. A dimension that is not declared (or has weight 0) contributes nothing and its sequence is not read. Every `SearchResult` states the criteria and the per-dimension evidence it used. All scores and weights are exact rationals (numerator/denominator in lowest terms); no floating point anywhere, and an exact result that does not fit in `unsigned long` is an explicit `OVERFLOW` result, never a wrapped value.

## SimilarityModel

`similarity_model_create(weights, count, &model)` takes one `SimilarityDimensionWeight` (dimension, signed numerator, signed denominator) per dimension. It rejects an empty set, an unknown or repeated dimension, a zero denominator, a negative weight, weights that are all zero, and weights whose sum overflows. A zero weight is allowed (the dimension counts for nothing). Weights are stored in lowest terms.

## The dimensions (query/similarity_scoring.h)

- **interval**: the sequences are directed semitone intervals (upward positive). Score = the fraction of positions with equal intervals. It compares intervals, not pitches, so it is transposition-invariant.
- **contour**: the sequences are directions (up, down, same). Score = the fraction of positions with equal direction. An input's directions are its own array if given, otherwise derived from its intervals.
- **rhythm**: the sequences are durations as positive ratios of whole notes. Score = the fraction of the n - 1 consecutive duration ratios `d[i+1]/d[i]` that are equal, so a uniformly augmented or diminished pattern still scores 1. A single duration scores 1 against another single duration.

In every dimension unequal lengths score 0, identical sequences score 1, and sequences that disagree everywhere score 0. The combined score is `sum(weight * score) / sum(weight)` over the counted dimensions, so it stays in [0, 1]. An empty sequence on a counted dimension is `INVALID_INPUT`.

## Corpus search (query/corpus_search.h)

`corpus_search_melodic(corpus_handle, persistence_handle, corpus_id, query, model, minimum, max_results, &results)`:

- A piece ref is a context id; a piece's material is the interval interpretations persisted for it (claim value type `interval-analysis-directed-semitones`, one signed `long`), in store order, taken as consecutive intervals of one line. A window the length of the query slides over it one interval at a time; overlapping windows are all candidates; a claim that does not decode breaks the windows it falls in.
- The result is every window whose similarity is at least `minimum` (an exact rational in [0, 1]), best first; equal similarity keeps piece order, then window start; at most `max_results` (at least 1). `NO_DATA` when nothing qualifies, leaving the output untouched.
- Like `corpus_stats`: both handles opened against the same store; piece refs with nothing persisted are skipped silently; a read-only aggregate that does not fail fast.
- A `SearchResult` has the piece ref and the id of the window's first interpretation (target), the first and last interpretation ids (match region), the similarity, the criteria (counted dimensions and weights), the evidence (each dimension's score), and provenance referring to the first matched interpretation. It owns its strings and arrays.

## Non-authority

Similarity is a descriptive match on the stated criteria, never a framework-validated claim. Nothing here becomes a Rule or Constraint, and no result says a match is musically significant.

## Not searched yet (nothing is invented to fill them)

- **Rhythm**: no durations are persisted, so a model that counts rhythm is refused with `CORPUS_SEARCH_ERR_DIMENSION_UNAVAILABLE` (scored 0 would pretend a measurement). Rhythm is scored on plain sequences through `similarity_score`.
- **Motifs**: `Motif`/`MotifOccurrence` are not persisted (the motif claim carries only an occurrence length), so no motif is searched; melodic windows over persisted intervals stand in for them.
- **Chord progressions**: a persisted Chord is a label with no interval, contour or rhythm to compare. They need a persisted chord structure (root and quality); root motion would then map to the interval dimension.
- `PatternMatching` is not implemented: this module scores sequences and searches windows, with no general pattern language.

## Deferred

The query interface (`find_notes`, `find_interpretations`, ...) and views; inversion, retrograde and fragmentation similarity; the Analyzer's similar-material mode.

## Notes

- `query/` keeps its own small exact-fraction helper, as `theory/` does (`style_fraction.h`), because it cannot depend on `rules/` or `theory/`; a shared lower-layer helper is a follow-up.
- No float/double anywhere (Law 19).
