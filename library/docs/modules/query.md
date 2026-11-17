# query

[Docs index](../README.md): the Ursatz docs by topic.

Path `src/query/`, tests `tests/query/`. Depends on `identity`, `core` (for `ProvenanceReference`) and `semantics` (for `ContourDirection`) only. Must not depend on `time`, `pitch`, `events`, `containers`, `structure`, `context`, `interpretation`, `relationship`, `theory`, `rules`, `transform`, `analysis`, `generation`, `io` or `frameworks/`. Enforced by the layer table.

## Provides

Similarity scoring over melodic sequences, with exact-rational scores:

- `similarity_model.h`: `SimilarityModel`, which dimensions count (interval, rhythm, contour) and an exact weight for each.
- `similarity_scoring.h`: how alike two sequences are on each declared dimension, and the weighted combination.

`query_fraction.h` is internal. Only the two headers above may be included from outside `query/`.

## Similarity is explicit-dimension

The caller declares the dimensions that count and a weight for each; there is no undeclared similarity. A dimension that is not declared (or has weight 0) contributes nothing and its sequence is not read. Every `SearchResult` states the criteria and the per-dimension evidence it used. All scores and weights are exact rationals (numerator/denominator in lowest terms); no floating point anywhere, and an exact result that does not fit in `unsigned long` is an explicit `OVERFLOW` result, never a wrapped value.

## SimilarityModel

`similarity_model_create(weights, count, &model)` takes one `SimilarityDimensionWeight` (dimension, signed numerator, signed denominator) per dimension. It rejects an empty set, an unknown or repeated dimension, a zero denominator, a negative weight, weights that are all zero, and weights whose sum overflows. A zero weight is allowed (the dimension counts for nothing). Weights are stored in lowest terms.

## The dimensions (query/similarity_scoring.h)

- **interval**: the sequences are directed semitone intervals (upward positive). Score = the fraction of positions with equal intervals. It compares intervals, not pitches, so it is transposition-invariant.
- **contour**: the sequences are directions (up, down, same). Score = the fraction of positions with equal direction. An input's directions are its own array if given, otherwise derived from its intervals.
- **rhythm**: the sequences are durations as positive ratios of whole notes. Score = the fraction of the n - 1 consecutive duration ratios `d[i+1]/d[i]` that are equal, so a uniformly augmented or diminished pattern still scores 1. A single duration scores 1 against another single duration.

In every dimension unequal lengths score 0, identical sequences score 1, and sequences that disagree everywhere score 0. The combined score is `sum(weight * score) / sum(weight)` over the counted dimensions, so it stays in [0, 1]. An empty sequence on a counted dimension is `INVALID_INPUT`.

## Non-authority

Similarity is a descriptive match on the stated criteria, never a framework-validated claim. Nothing here becomes a Rule or Constraint, and no result says a match is musically significant.
