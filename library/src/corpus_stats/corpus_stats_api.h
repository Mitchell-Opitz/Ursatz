#ifndef MUSIC_CORPUS_STATS_CORPUS_STATS_API_H
#define MUSIC_CORPUS_STATS_CORPUS_STATS_API_H

#include <stddef.h>

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
    CORPUS_STATS_ERR_ALLOC,
    /* An argument is out of its documented range (e.g. an n-gram length outside 2 to 6). */
    CORPUS_STATS_ERR_INVALID_ARG
} CorpusStatsResult;

/*
 * A count result: every distinct label seen, with how many times it was seen.
 * Plain values only (a label string and an unsigned integer), the same opaque
 * boundary as corpus_stats_most_common_chord: nothing here is an internal
 * type, and nothing is authoritative -- it says what the persisted
 * interpretations say.
 *
 * entries are ordered by count, highest first; labels with equal counts keep
 * the order in which they were first seen (piece-ref order, then store order
 * within a piece). total is the sum of all counts, so a label's frequency is
 * count/total (an exact rational; no floating point anywhere). A result with
 * no entries is never returned as OK: the function returns
 * CORPUS_STATS_NO_DATA and leaves the output untouched.
 *
 * On CORPUS_STATS_OK the caller owns the result and frees it with
 * corpus_stats_counts_free.
 */
typedef struct {
    char *label;
    unsigned long count;
} CorpusStatsCount;

typedef struct {
    CorpusStatsCount *entries;
    size_t entry_count;
    unsigned long total;
} CorpusStatsCounts;

/* Frees the labels and entries and zeroes the struct. Safe on NULL and on an already-freed result. */
void corpus_stats_counts_free(CorpusStatsCounts *counts);

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

/*
 * Chord progressions: counts every run of n consecutive chords (an n-gram,
 * n from CORPUS_STATS_MIN_PROGRESSION_LENGTH to _MAX_PROGRESSION_LENGTH)
 * across the Corpus, reading each piece's persisted Chord interpretations in
 * the order they were stored. A piece is a piece ref used as a context id:
 * its chords are the stored Chords whose Interpretation has that context_id
 * (persistence/persistence_context_query.h). A run never crosses from one
 * piece into the next, and a piece with fewer than n chords contributes
 * nothing. A chord that cannot be loaded breaks the run it falls in.
 *
 * A progression's label is its chord labels joined by " > " (e.g.
 * "I > V > I"). Piece refs that resolve to no chords are skipped silently,
 * as for corpus_stats_most_common_chord.
 *
 * CORPUS_STATS_ERR_INVALID_ARG if n is outside 2 to 6;
 * CORPUS_STATS_NO_DATA (output untouched) if no n-gram was counted.
 */
#define CORPUS_STATS_MIN_PROGRESSION_LENGTH 2
#define CORPUS_STATS_MAX_PROGRESSION_LENGTH 6

CorpusStatsResult corpus_stats_chord_progressions(CorpusStoreHandle *corpus_handle,
                                                  PersistenceHandle *persistence_handle, const char *corpus_id,
                                                  size_t n, CorpusStatsCounts *out_counts);

#endif
