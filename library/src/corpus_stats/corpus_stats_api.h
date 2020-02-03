#ifndef MUSIC_CORPUS_STATS_CORPUS_STATS_API_H
#define MUSIC_CORPUS_STATS_CORPUS_STATS_API_H

#include "corpus/corpus_api.h"
#include "persistence/persistence.h"

/*
 * Corpus Statistics: the first cross-piece computation over a Corpus's
 * membership (Registry's "Corpus Statistics" Concept), kept as its own
 * module rather than folded into corpus_api.h -- corpus/ itself stays
 * dependency-pure (identity + persistence only, per docs/modules/
 * corpus.md); this module additionally depends on corpus/ (for
 * CorpusStoreHandle and its piece-ref listing) and interpretation/ (for
 * Chord, via persistence/persistence_chord.h) to actually resolve piece
 * refs to chord data.
 *
 * Results here are a plain output value (a chord label string), not an
 * internal type -- the same "opaque value" boundary Theory's Tendency/
 * StatisticalModel will eventually consume this through, per Registry.
 * These results are explicitly non-authoritative: they describe what a
 * Corpus's already-persisted Chord interpretations say, not a
 * framework-validated musical truth.
 *
 * Every function here takes both a CorpusStoreHandle and a
 * PersistenceHandle explicitly, since this is the first operation that
 * reads across the Corpus/Persistence module boundary: the two handles
 * must be opened against the same underlying store, since piece refs
 * recorded by Corpus are resolved as Chord ids by Persistence.
 */

typedef enum {
    CORPUS_STATS_OK = 0,
    CORPUS_STATS_NO_DATA,
    CORPUS_STATS_ERR_NULL_ARG,
    CORPUS_STATS_ERR_DB,
    CORPUS_STATS_ERR_ALLOC
} CorpusStatsResult;

/*
 * Computes the most frequently occurring chord label across every
 * piece ref in the given Corpus that resolves to a stored Chord.
 *
 * Piece refs that don't resolve to a Chord in persistence_handle (no
 * existence validation exists for Corpus membership today, per
 * corpus-minimal's design) are skipped silently and the tally
 * continues -- a deliberate exception to this codebase's general
 * fail-fast convention, since this is a read-only aggregate query, not
 * a fail-fast mutation, and one bad ref should not fail the whole
 * computation.
 *
 * Ties are broken deterministically: the first label encountered (in
 * piece-ref iteration order) among those tied for the highest count
 * wins.
 *
 * On success (CORPUS_STATS_OK), *out_chord_label is a newly allocated,
 * owned, NUL-terminated string the caller must free. If the Corpus has
 * zero piece refs that resolve to a stored Chord (including an empty
 * Corpus), returns CORPUS_STATS_NO_DATA and leaves *out_chord_label
 * untouched -- this is not an error.
 */
CorpusStatsResult corpus_stats_most_common_chord(CorpusStoreHandle *corpus_handle,
                                                  PersistenceHandle *persistence_handle, const char *corpus_id,
                                                  char **out_chord_label);

#endif
