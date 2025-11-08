#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "corpus_stats/corpus_stats_api.h"
#include "corpus_stats/corpus_stats_tally.h"
#include "interpretation/chord.h"
#include "persistence/persistence_chord.h"
#include "persistence/persistence_context_query.h"

/*
 * The count statistics other than most-common-chord. Each reads, for every
 * piece ref of the Corpus taken as a context id, the interpretations
 * persisted for that piece, and tallies a label per fact. Unresolvable refs
 * are skipped silently and a read-only aggregate never fails fast; only an
 * allocation failure stops a walk.
 */

#define PROGRESSION_SEPARATOR " > "

typedef struct {
    CorpusStatsTally *tally;
    size_t n;
} ProgressionWalk;

static void free_labels(char **labels, size_t count) {
    for (size_t i = 0; i < count; i++) {
        free(labels[i]);
    }
    free(labels);
}

/* Adds the progression labels[start .. start + n) to the tally. */
static CorpusStatsResult add_progression(CorpusStatsTally *tally, char *const *labels, size_t start, size_t n) {
    size_t length = 1;
    for (size_t i = 0; i < n; i++) {
        length += strlen(labels[start + i]) + strlen(PROGRESSION_SEPARATOR);
    }
    char *joined = (char *)malloc(length);
    if (joined == NULL) {
        return CORPUS_STATS_ERR_ALLOC;
    }
    joined[0] = '\0';
    for (size_t i = 0; i < n; i++) {
        if (i > 0) {
            strcat(joined, PROGRESSION_SEPARATOR);
        }
        strcat(joined, labels[start + i]);
    }
    CorpusStatsResult result = corpus_stats_tally_add(tally, joined);
    free(joined);
    return result;
}

static CorpusStatsResult visit_progressions(PersistenceHandle *persistence_handle, const char *piece_ref,
                                            void *user_data) {
    ProgressionWalk *walk = (ProgressionWalk *)user_data;
    char **chord_ids = NULL;
    size_t chord_count = 0;
    if (persistence_list_chord_ids_for_context(persistence_handle, piece_ref, &chord_ids, &chord_count) !=
        PERSISTENCE_QUERY_OK) {
        return CORPUS_STATS_OK;
    }
    if (chord_count < walk->n) {
        persistence_free_id_list(chord_ids, chord_count);
        return CORPUS_STATS_OK;
    }

    /* The label of each chord in piece order; NULL where a chord could not be loaded. */
    char **labels = (char **)calloc(chord_count, sizeof(*labels));
    if (labels == NULL) {
        persistence_free_id_list(chord_ids, chord_count);
        return CORPUS_STATS_ERR_ALLOC;
    }
    CorpusStatsResult result = CORPUS_STATS_OK;
    for (size_t i = 0; i < chord_count && result == CORPUS_STATS_OK; i++) {
        Chord *chord = NULL;
        if (persistence_load_chord(persistence_handle, chord_ids[i], &chord) != PERSISTENCE_CHORD_OK) {
            continue;
        }
        labels[i] = strdup(chord_label(chord));
        chord_destroy(chord);
        if (labels[i] == NULL) {
            result = CORPUS_STATS_ERR_ALLOC;
        }
    }

    for (size_t start = 0; start + walk->n <= chord_count && result == CORPUS_STATS_OK; start++) {
        int complete = 1;
        for (size_t i = 0; i < walk->n; i++) {
            if (labels[start + i] == NULL) {
                complete = 0;
                break;
            }
        }
        if (complete) {
            result = add_progression(walk->tally, labels, start, walk->n);
        }
    }

    free_labels(labels, chord_count);
    persistence_free_id_list(chord_ids, chord_count);
    return result;
}

CorpusStatsResult corpus_stats_chord_progressions(CorpusStoreHandle *corpus_handle,
                                                  PersistenceHandle *persistence_handle, const char *corpus_id,
                                                  size_t n, CorpusStatsCounts *out_counts) {
    if (corpus_handle == NULL || persistence_handle == NULL || corpus_id == NULL || out_counts == NULL) {
        return CORPUS_STATS_ERR_NULL_ARG;
    }
    if (n < CORPUS_STATS_MIN_PROGRESSION_LENGTH || n > CORPUS_STATS_MAX_PROGRESSION_LENGTH) {
        return CORPUS_STATS_ERR_INVALID_ARG;
    }

    CorpusStatsTally tally;
    corpus_stats_tally_init(&tally);
    ProgressionWalk walk = {&tally, n};
    CorpusStatsResult result =
        corpus_stats_for_each_piece(corpus_handle, persistence_handle, corpus_id, visit_progressions, &walk);
    if (result != CORPUS_STATS_OK) {
        corpus_stats_tally_free(&tally);
        return result;
    }
    return corpus_stats_tally_finish(&tally, out_counts);
}
