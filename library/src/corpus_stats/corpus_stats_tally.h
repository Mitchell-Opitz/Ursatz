#ifndef MUSIC_CORPUS_STATS_CORPUS_STATS_TALLY_H
#define MUSIC_CORPUS_STATS_CORPUS_STATS_TALLY_H

#include "corpus_stats/corpus_stats_api.h"

/*
 * Internal to corpus_stats/*.c: a running tally of labels, and the piece-by-
 * piece walk every statistic shares. Not part of the public API.
 */

typedef struct {
    CorpusStatsCount *entries;
    size_t entry_count;
    unsigned long total;
} CorpusStatsTally;

/* An empty tally. */
void corpus_stats_tally_init(CorpusStatsTally *tally);

/* Adds one to label's count, adding the label (copied) if it is new. Returns CORPUS_STATS_OK or ERR_ALLOC. */
CorpusStatsResult corpus_stats_tally_add(CorpusStatsTally *tally, const char *label);

/* Frees a tally that was not handed to a caller. */
void corpus_stats_tally_free(CorpusStatsTally *tally);

/*
 * Moves the tally into *out (ordered by count, highest first, ties in
 * first-seen order) and empties the tally. An empty tally leaves *out
 * untouched and returns CORPUS_STATS_NO_DATA.
 */
CorpusStatsResult corpus_stats_tally_finish(CorpusStatsTally *tally, CorpusStatsCounts *out);

/*
 * Calls visit for each piece ref of the corpus in order, with the piece ref
 * (a context id: the interpretations an analysis of that piece persisted
 * carry it as their context_id). A visit that returns anything other than
 * CORPUS_STATS_OK stops the walk with that result. A corpus whose refs cannot
 * be listed is CORPUS_STATS_ERR_DB.
 */
typedef CorpusStatsResult (*CorpusStatsPieceVisit)(PersistenceHandle *persistence_handle, const char *piece_ref,
                                                   void *user_data);
CorpusStatsResult corpus_stats_for_each_piece(CorpusStoreHandle *corpus_handle,
                                              PersistenceHandle *persistence_handle, const char *corpus_id,
                                              CorpusStatsPieceVisit visit, void *user_data);

#endif
