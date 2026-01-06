#ifndef MUSIC_QUERY_CORPUS_SEARCH_H
#define MUSIC_QUERY_CORPUS_SEARCH_H

#include <stddef.h>

#include "corpus/corpus_api.h"
#include "persistence/persistence.h"
#include "query/search_result.h"
#include "query/similarity_model.h"
#include "query/similarity_scoring.h"

/*
 * Corpus search: finds melodic material in a Corpus's persisted
 * interpretations that is similar to a query, under a SimilarityModel.
 *
 * WHAT IT SEARCHES. Only what analysis has persisted -- a search never runs
 * analysis. A piece ref of the Corpus is a context id: a piece's material is
 * the interval interpretations persisted for it (claim value type
 * CORPUS_SEARCH_INTERVAL_CLAIM_TYPE, whose content is one signed long, the
 * interval's size in semitones; persistence/persistence_context_query.h), in
 * the order they were stored, taken as consecutive intervals of one line. The
 * search slides a window the length of the query over that sequence, one
 * interval at a time, and scores each window against the query. Overlapping
 * windows are all candidates. A claim that does not decode (not one long)
 * breaks the windows it falls in.
 *
 * Interval and contour data exist in that persisted material (contour is the
 * direction of each interval). Durations are not persisted, so the rhythm
 * dimension has nothing to score: a model that counts rhythm (declared with a
 * non-zero weight) is rejected with CORPUS_SEARCH_ERR_DIMENSION_UNAVAILABLE
 * rather than scored 0, which would pretend a measurement. Rhythm can still be
 * scored on plain sequences (query/similarity_scoring.h). Motif and
 * MotifOccurrence are not persisted either, and chord progressions persist as
 * labels only, with no interval, contour or rhythm to compare, so neither is
 * searched; nothing is invented to fill them.
 *
 * THE QUERY is a SimilarityInput: its intervals (signed semitones), and
 * optionally its directions. The window length is its interval count, or its
 * direction count if it has no intervals. A counted dimension needs its data
 * in the query (interval needs intervals; contour needs directions or
 * intervals).
 *
 * THE RESULT is every window whose combined similarity is at least the
 * minimum (an exact rational in [0, 1], compared exactly), best first.
 * Windows with equal similarity keep piece order (the Corpus's piece-ref
 * order), then window start. At most max_results are returned (at least 1;
 * pass SIZE_MAX for all). Each SearchResult states the criteria and evidence
 * used (query/search_result.h).
 *
 * Like corpus_stats, it takes a CorpusStoreHandle and a PersistenceHandle
 * opened against the same store, skips piece refs with nothing persisted
 * silently, and is a read-only aggregate that does not fail fast over one bad
 * piece. Results are non-authoritative: a descriptive match on the stated
 * criteria, never a framework claim, and nothing here becomes a Rule.
 */

#define CORPUS_SEARCH_INTERVAL_CLAIM_TYPE "interval-analysis-directed-semitones"

typedef enum {
    CORPUS_SEARCH_OK = 0,
    CORPUS_SEARCH_NO_DATA, /* nothing meets the minimum (including: nothing persisted); output untouched */
    CORPUS_SEARCH_ERR_NULL_ARG,
    CORPUS_SEARCH_ERR_INVALID_ARG, /* max_results 0, a minimum outside [0, 1], an empty query, or query data a counted
                                      dimension needs is missing */
    CORPUS_SEARCH_ERR_DIMENSION_UNAVAILABLE, /* the model counts a dimension no persisted data has (rhythm) */
    CORPUS_SEARCH_ERR_DB,
    CORPUS_SEARCH_ERR_ALLOC,
    CORPUS_SEARCH_ERR_OVERFLOW /* an exact score did not fit in unsigned long */
} CorpusSearchResult;

typedef struct {
    SearchResult **results;
    size_t count;
} CorpusSearchResults;

/*
 * On CORPUS_SEARCH_OK *out holds count >= 1 results, owned by the caller
 * (free with corpus_search_results_free). On any other result *out is left
 * untouched.
 */
CorpusSearchResult corpus_search_melodic(CorpusStoreHandle *corpus_handle, PersistenceHandle *persistence_handle,
                                         const char *corpus_id, const SimilarityInput *query,
                                         const SimilarityModel *model, unsigned long minimum_numerator,
                                         unsigned long minimum_denominator, size_t max_results,
                                         CorpusSearchResults *out);

/* Destroys every result and the array, and zeroes the struct. Safe on NULL. */
void corpus_search_results_free(CorpusSearchResults *results);

#endif
