#include "corpus_stats/corpus_stats_api.h"

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "corpus/corpus_api.h"
#include "stats_test_util.h"

/*
 * The count statistics read each piece ref of a Corpus as a context id and
 * count what was persisted for that piece. These tests use two small pieces
 * so every count can be checked by hand.
 */

typedef struct {
    CorpusStoreHandle *corpus;
    PersistenceHandle *persistence;
    const char *path;
} Store;

static Store open_store(const char *path) {
    Store store = {NULL, NULL, path};
    remove(path);
    assert(corpus_api_open(path, &store.corpus) == CORPUS_API_OK);
    assert(persistence_open(path, &store.persistence) == PERSISTENCE_OK);
    assert(corpus_api_create_corpus(store.corpus, "corpus-1", "Test corpus") == CORPUS_API_OK);
    return store;
}

static void close_store(Store *store) {
    persistence_close(store->persistence);
    corpus_api_close(store->corpus);
    remove(store->path);
}

static void assert_entry(const CorpusStatsCounts *counts, size_t index, const char *label, unsigned long count) {
    assert(index < counts->entry_count);
    assert(strcmp(counts->entries[index].label, label) == 0);
    assert(counts->entries[index].count == count);
}

static void assert_total_matches(const CorpusStatsCounts *counts) {
    unsigned long sum = 0;
    for (size_t i = 0; i < counts->entry_count; i++) {
        sum += counts->entries[i].count;
    }
    assert(sum == counts->total);
}

/* piece-a: I IV V7 I.  piece-b: I IV V7.  A chord with no piece at all: vi. */
static void store_two_pieces(Store *store) {
    stats_store_chord(store->persistence, "a1", "I", "piece-a");
    stats_store_chord(store->persistence, "a2", "IV", "piece-a");
    stats_store_chord(store->persistence, "a3", "V7", "piece-a");
    stats_store_chord(store->persistence, "a4", "I", "piece-a");
    stats_store_chord(store->persistence, "b1", "I", "piece-b");
    stats_store_chord(store->persistence, "b2", "IV", "piece-b");
    stats_store_chord(store->persistence, "b3", "V7", "piece-b");
    stats_store_chord(store->persistence, "x1", "vi", NULL);
    assert(corpus_api_add_piece_ref(store->corpus, "corpus-1", "piece-a") == CORPUS_API_OK);
    assert(corpus_api_add_piece_ref(store->corpus, "corpus-1", "piece-b") == CORPUS_API_OK);
}

static void test_chord_progressions(void) {
    Store store = open_store("test_corpus_stats_counts_progressions.sqlite");
    store_two_pieces(&store);
    /* A ref that resolves to nothing is skipped silently. */
    assert(corpus_api_add_piece_ref(store.corpus, "corpus-1", "no-such-piece") == CORPUS_API_OK);

    CorpusStatsCounts counts = {NULL, 0, 0};
    assert(corpus_stats_chord_progressions(store.corpus, store.persistence, "corpus-1", 2, &counts) ==
           CORPUS_STATS_OK);
    /* Pairs never run from piece-a's last chord into piece-b's first: V7 > I once, not I > I. */
    assert(counts.entry_count == 3 && counts.total == 5);
    assert_entry(&counts, 0, "I > IV", 2);
    assert_entry(&counts, 1, "IV > V7", 2);
    assert_entry(&counts, 2, "V7 > I", 1);
    assert_total_matches(&counts);
    corpus_stats_counts_free(&counts);
    assert(counts.entries == NULL && counts.entry_count == 0 && counts.total == 0);

    assert(corpus_stats_chord_progressions(store.corpus, store.persistence, "corpus-1", 3, &counts) ==
           CORPUS_STATS_OK);
    assert(counts.entry_count == 2 && counts.total == 3);
    assert_entry(&counts, 0, "I > IV > V7", 2);
    assert_entry(&counts, 1, "IV > V7 > I", 1);
    corpus_stats_counts_free(&counts);

    /* Only piece-a has four chords. */
    assert(corpus_stats_chord_progressions(store.corpus, store.persistence, "corpus-1", 4, &counts) ==
           CORPUS_STATS_OK);
    assert(counts.entry_count == 1 && counts.total == 1);
    assert_entry(&counts, 0, "I > IV > V7 > I", 1);
    corpus_stats_counts_free(&counts);

    /* No piece has five chords: no data, and the output is left alone. */
    counts.entry_count = 77;
    assert(corpus_stats_chord_progressions(store.corpus, store.persistence, "corpus-1", 5, &counts) ==
           CORPUS_STATS_NO_DATA);
    assert(counts.entry_count == 77);
    counts.entry_count = 0;

    /* The length must be 2 to 6. */
    assert(corpus_stats_chord_progressions(store.corpus, store.persistence, "corpus-1", 1, &counts) ==
           CORPUS_STATS_ERR_INVALID_ARG);
    assert(corpus_stats_chord_progressions(store.corpus, store.persistence, "corpus-1", 0, &counts) ==
           CORPUS_STATS_ERR_INVALID_ARG);
    assert(corpus_stats_chord_progressions(store.corpus, store.persistence, "corpus-1", 7, &counts) ==
           CORPUS_STATS_ERR_INVALID_ARG);
    assert(corpus_stats_chord_progressions(store.corpus, store.persistence, "corpus-1", 6, &counts) ==
           CORPUS_STATS_NO_DATA);

    assert(corpus_stats_chord_progressions(NULL, store.persistence, "corpus-1", 2, &counts) ==
           CORPUS_STATS_ERR_NULL_ARG);
    assert(corpus_stats_chord_progressions(store.corpus, NULL, "corpus-1", 2, &counts) == CORPUS_STATS_ERR_NULL_ARG);
    assert(corpus_stats_chord_progressions(store.corpus, store.persistence, NULL, 2, &counts) ==
           CORPUS_STATS_ERR_NULL_ARG);
    assert(corpus_stats_chord_progressions(store.corpus, store.persistence, "corpus-1", 2, NULL) ==
           CORPUS_STATS_ERR_NULL_ARG);
    close_store(&store);
}

static void test_empty_corpus_has_no_data(void) {
    Store store = open_store("test_corpus_stats_counts_empty.sqlite");
    CorpusStatsCounts counts = {NULL, 5, 9};
    assert(corpus_stats_chord_progressions(store.corpus, store.persistence, "corpus-1", 2, &counts) ==
           CORPUS_STATS_NO_DATA);
    assert(corpus_stats_cadence_types(store.corpus, store.persistence, "corpus-1", &counts) == CORPUS_STATS_NO_DATA);
    assert(corpus_stats_directed_intervals(store.corpus, store.persistence, "corpus-1", &counts) ==
           CORPUS_STATS_NO_DATA);
    assert(counts.entry_count == 5 && counts.total == 9);

    /* A corpus with a ref but nothing persisted for it is also empty. */
    assert(corpus_api_add_piece_ref(store.corpus, "corpus-1", "piece-a") == CORPUS_API_OK);
    assert(corpus_stats_cadence_types(store.corpus, store.persistence, "corpus-1", &counts) == CORPUS_STATS_NO_DATA);

    /* An unknown corpus lists no refs: no data too. */
    assert(corpus_stats_directed_intervals(store.corpus, store.persistence, "no-such-corpus", &counts) ==
           CORPUS_STATS_NO_DATA);
    corpus_stats_counts_free(NULL);
    close_store(&store);
}

static void test_cadence_types(void) {
    Store store = open_store("test_corpus_stats_counts_cadences.sqlite");
    assert(corpus_api_add_piece_ref(store.corpus, "corpus-1", "piece-a") == CORPUS_API_OK);
    assert(corpus_api_add_piece_ref(store.corpus, "corpus-1", "piece-b") == CORPUS_API_OK);
    const char pac[] = "PAC";
    const char hc[] = "HC";
    stats_store_claim(store.persistence, "c1", CORPUS_STATS_CADENCE_CLAIM_TYPE, hc, sizeof(hc), "piece-a");
    stats_store_claim(store.persistence, "c2", CORPUS_STATS_CADENCE_CLAIM_TYPE, pac, sizeof(pac), "piece-a");
    stats_store_claim(store.persistence, "c3", CORPUS_STATS_CADENCE_CLAIM_TYPE, pac, sizeof(pac), "piece-b");
    stats_store_claim(store.persistence, "c4", CORPUS_STATS_CADENCE_CLAIM_TYPE, pac, sizeof(pac), "piece-b");
    /* Not NUL-terminated, empty, and another claim type: none of these count. */
    const char unterminated[] = {'P', 'A', 'C'};
    const char empty[] = "";
    stats_store_claim(store.persistence, "c5", CORPUS_STATS_CADENCE_CLAIM_TYPE, unterminated, sizeof(unterminated),
                      "piece-a");
    stats_store_claim(store.persistence, "c6", CORPUS_STATS_CADENCE_CLAIM_TYPE, empty, sizeof(empty), "piece-a");
    stats_store_claim(store.persistence, "c7", "phrase-segmentation-claim", pac, sizeof(pac), "piece-a");
    stats_store_claim(store.persistence, "c8", CORPUS_STATS_CADENCE_CLAIM_TYPE, pac, sizeof(pac), "another-piece");

    CorpusStatsCounts counts = {NULL, 0, 0};
    assert(corpus_stats_cadence_types(store.corpus, store.persistence, "corpus-1", &counts) == CORPUS_STATS_OK);
    assert(counts.entry_count == 2 && counts.total == 4);
    assert_entry(&counts, 0, "PAC", 3);
    assert_entry(&counts, 1, "HC", 1);
    assert_total_matches(&counts);
    corpus_stats_counts_free(&counts);

    assert(corpus_stats_cadence_types(NULL, store.persistence, "corpus-1", &counts) == CORPUS_STATS_ERR_NULL_ARG);
    assert(corpus_stats_cadence_types(store.corpus, store.persistence, "corpus-1", NULL) ==
           CORPUS_STATS_ERR_NULL_ARG);
    close_store(&store);
}

static void test_directed_intervals(void) {
    Store store = open_store("test_corpus_stats_counts_intervals.sqlite");
    assert(corpus_api_add_piece_ref(store.corpus, "corpus-1", "piece-a") == CORPUS_API_OK);
    const long semitones[] = {2, 2, -3, 0, 2, -3, 7};
    char id[16];
    for (size_t i = 0; i < sizeof(semitones) / sizeof(semitones[0]); i++) {
        snprintf(id, sizeof(id), "i%zu", i);
        stats_store_claim(store.persistence, id, CORPUS_STATS_INTERVAL_CLAIM_TYPE, &semitones[i], sizeof(long),
                          "piece-a");
    }
    /* A claim of the wrong size does not count. */
    const char short_content[] = "ab";
    stats_store_claim(store.persistence, "bad", CORPUS_STATS_INTERVAL_CLAIM_TYPE, short_content, 2, "piece-a");

    CorpusStatsCounts counts = {NULL, 0, 0};
    assert(corpus_stats_directed_intervals(store.corpus, store.persistence, "corpus-1", &counts) == CORPUS_STATS_OK);
    assert(counts.entry_count == 4 && counts.total == 7);
    assert_entry(&counts, 0, "+2", 3);
    assert_entry(&counts, 1, "-3", 2);
    assert_entry(&counts, 2, "+0", 1);
    assert_entry(&counts, 3, "+7", 1);
    assert_total_matches(&counts);
    corpus_stats_counts_free(&counts);
    close_store(&store);
}

int main(void) {
    test_chord_progressions();
    test_empty_corpus_has_no_data();
    test_cadence_types();
    test_directed_intervals();
    printf("test_corpus_stats_counts: ok\n");
    return 0;
}
