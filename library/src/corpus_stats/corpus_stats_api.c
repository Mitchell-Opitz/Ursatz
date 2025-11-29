#include "corpus_stats/corpus_stats_api.h"

#include <stdlib.h>
#include <string.h>

#include "interpretation/chord.h"
#include "persistence/persistence_chord.h"

typedef struct {
    char *label;
    size_t count;
} ChordTally;

static void free_tallies(ChordTally *tallies, size_t count) {
    for (size_t i = 0; i < count; i++) {
        free(tallies[i].label);
    }
    free(tallies);
}

static void free_piece_refs(char **refs, size_t count) {
    for (size_t i = 0; i < count; i++) {
        free(refs[i]);
    }
    free(refs);
}

CorpusStatsResult corpus_stats_most_common_chord(CorpusStoreHandle *corpus_handle,
                                                  PersistenceHandle *persistence_handle, const char *corpus_id,
                                                  char **out_chord_label) {
    if (corpus_handle == NULL || persistence_handle == NULL || corpus_id == NULL || out_chord_label == NULL) {
        return CORPUS_STATS_ERR_NULL_ARG;
    }

    char **refs = NULL;
    size_t ref_count = 0;
    if (corpus_api_list_piece_refs(corpus_handle, corpus_id, &refs, &ref_count) != CORPUS_API_OK) {
        return CORPUS_STATS_ERR_DB;
    }

    ChordTally *tallies = NULL;
    size_t tally_count = 0;
    CorpusStatsResult result = CORPUS_STATS_OK;

    for (size_t i = 0; i < ref_count; i++) {
        Chord *chord = NULL;
        /*
         * A piece ref that doesn't resolve to a stored Chord (no
         * existence validation exists at Corpus-membership time) is
         * skipped, not an error -- see the header comment.
         */
        if (persistence_load_chord(persistence_handle, refs[i], &chord) != PERSISTENCE_CHORD_OK) {
            continue;
        }

        const char *label = chord_label(chord);
        size_t match = tally_count;
        for (size_t j = 0; j < tally_count; j++) {
            if (strcmp(tallies[j].label, label) == 0) {
                match = j;
                break;
            }
        }

        if (match < tally_count) {
            tallies[match].count++;
        } else {
            ChordTally *grown = realloc(tallies, (tally_count + 1) * sizeof(*grown));
            if (grown == NULL) {
                chord_destroy(chord);
                result = CORPUS_STATS_ERR_ALLOC;
                break;
            }
            tallies = grown;
            tallies[tally_count].label = strdup(label);
            tallies[tally_count].count = 1;
            if (tallies[tally_count].label == NULL) {
                chord_destroy(chord);
                result = CORPUS_STATS_ERR_ALLOC;
                break;
            }
            tally_count++;
        }

        chord_destroy(chord);
    }

    free_piece_refs(refs, ref_count);

    if (result != CORPUS_STATS_OK) {
        free_tallies(tallies, tally_count);
        return result;
    }

    if (tally_count == 0) {
        return CORPUS_STATS_NO_DATA;
    }

    /* First-encountered wins ties: strictly-greater only replaces the winner. */
    size_t winner = 0;
    for (size_t i = 1; i < tally_count; i++) {
        if (tallies[i].count > tallies[winner].count) {
            winner = i;
        }
    }

    char *winning_label = strdup(tallies[winner].label);
    free_tallies(tallies, tally_count);

    if (winning_label == NULL) {
        return CORPUS_STATS_ERR_ALLOC;
    }

    *out_chord_label = winning_label;
    return CORPUS_STATS_OK;
}

void corpus_stats_counts_free(CorpusStatsCounts *counts) {
    if (counts == NULL) {
        return;
    }
    for (size_t i = 0; i < counts->entry_count; i++) {
        free(counts->entries[i].label);
    }
    free(counts->entries);
    counts->entries = NULL;
    counts->entry_count = 0;
    counts->total = 0;
}
