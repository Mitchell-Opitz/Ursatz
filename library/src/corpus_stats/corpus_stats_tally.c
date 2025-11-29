#include "corpus_stats/corpus_stats_tally.h"

#include <stdlib.h>
#include <string.h>

void corpus_stats_tally_init(CorpusStatsTally *tally) {
    tally->entries = NULL;
    tally->entry_count = 0;
    tally->total = 0;
}

CorpusStatsResult corpus_stats_tally_add(CorpusStatsTally *tally, const char *label) {
    for (size_t i = 0; i < tally->entry_count; i++) {
        if (strcmp(tally->entries[i].label, label) == 0) {
            tally->entries[i].count++;
            tally->total++;
            return CORPUS_STATS_OK;
        }
    }
    CorpusStatsCount *grown = (CorpusStatsCount *)realloc(tally->entries, (tally->entry_count + 1) * sizeof(*grown));
    if (grown == NULL) {
        return CORPUS_STATS_ERR_ALLOC;
    }
    tally->entries = grown;
    tally->entries[tally->entry_count].label = strdup(label);
    if (tally->entries[tally->entry_count].label == NULL) {
        return CORPUS_STATS_ERR_ALLOC;
    }
    tally->entries[tally->entry_count].count = 1;
    tally->entry_count++;
    tally->total++;
    return CORPUS_STATS_OK;
}

void corpus_stats_tally_free(CorpusStatsTally *tally) {
    for (size_t i = 0; i < tally->entry_count; i++) {
        free(tally->entries[i].label);
    }
    free(tally->entries);
    corpus_stats_tally_init(tally);
}

CorpusStatsResult corpus_stats_tally_finish(CorpusStatsTally *tally, CorpusStatsCounts *out) {
    if (tally->entry_count == 0) {
        return CORPUS_STATS_NO_DATA;
    }
    /* Stable insertion sort, highest count first: equal counts keep first-seen order. */
    for (size_t i = 1; i < tally->entry_count; i++) {
        CorpusStatsCount moving = tally->entries[i];
        size_t j = i;
        while (j > 0 && tally->entries[j - 1].count < moving.count) {
            tally->entries[j] = tally->entries[j - 1];
            j--;
        }
        tally->entries[j] = moving;
    }
    out->entries = tally->entries;
    out->entry_count = tally->entry_count;
    out->total = tally->total;
    corpus_stats_tally_init(tally);
    return CORPUS_STATS_OK;
}

CorpusStatsResult corpus_stats_for_each_piece(CorpusStoreHandle *corpus_handle,
                                              PersistenceHandle *persistence_handle, const char *corpus_id,
                                              CorpusStatsPieceVisit visit, void *user_data) {
    char **refs = NULL;
    size_t ref_count = 0;
    if (corpus_api_list_piece_refs(corpus_handle, corpus_id, &refs, &ref_count) != CORPUS_API_OK) {
        return CORPUS_STATS_ERR_DB;
    }
    CorpusStatsResult result = CORPUS_STATS_OK;
    for (size_t i = 0; i < ref_count && result == CORPUS_STATS_OK; i++) {
        result = visit(persistence_handle, refs[i], user_data);
    }
    for (size_t i = 0; i < ref_count; i++) {
        free(refs[i]);
    }
    free(refs);
    return result;
}
