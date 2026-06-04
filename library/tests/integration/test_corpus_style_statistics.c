#include <stdarg.h>

#include "framework_v1_chain.h"
#include "framework_v1_deviations.h"

#include "corpus/corpus_api.h"
#include "corpus_stats/corpus_stats_api.h"
#include "stats_test_util.h"

/*
 * Corpus style statistics over the five Framework v1 reference pieces.
 *
 * THE GLUE. Each piece goes through the full chain (framework_v1_chain.h). What the chain found is persisted the way a piece's analysis is read
 * back: every identified chord as a stored Chord, and every cadence the chain classified as a cadence interpretation (claim value type
 * CORPUS_STATS_CADENCE_CLAIM_TYPE, the cadence type's label), each carrying the piece's file name as its context_id. Only chords the chain
 * identified are stored, so a segment it could not identify is absent from the stored sequence and the chords either side of it are neighbours
 * there. The chain makes no interval interpretations, so there are none to count here (the interval count is tested on synthetic data).
 *
 * THE CHECKS. For each piece, a Corpus of that piece alone, and for the whole set, a Corpus of all five, the corpus_stats counts are compared
 * with a tally taken directly from the chain's own result: chord progressions of length 2 and 3 (a run never crosses from one piece into the
 * next) and cadence types. A corpus with no cadences gives NO_DATA.
 */

#define MAX_LABELS 4096

typedef struct {
    char label[256];
    unsigned long count;
} Label;

typedef struct {
    Label items[MAX_LABELS];
    size_t count;
    unsigned long total;
} Tally;

static Tally *tally_new(void) {
    Tally *tally = (Tally *)calloc(1, sizeof(*tally));
    assert(tally != NULL);
    return tally;
}

static void tally_add(Tally *tally, const char *label) {
    for (size_t i = 0; i < tally->count; i++) {
        if (strcmp(tally->items[i].label, label) == 0) {
            tally->items[i].count++;
            tally->total++;
            return;
        }
    }
    assert(tally->count < MAX_LABELS);
    snprintf(tally->items[tally->count].label, sizeof(tally->items[0].label), "%s", label);
    tally->items[tally->count].count = 1;
    tally->count++;
    tally->total++;
}

typedef struct {
    char **labels;
    size_t count;
} Sequence;

static void sequence_add(Sequence *sequence, const char *label) {
    sequence->labels = (char **)realloc(sequence->labels, (sequence->count + 1) * sizeof(char *));
    assert(sequence->labels != NULL);
    sequence->labels[sequence->count] = strdup(label);
    assert(sequence->labels[sequence->count] != NULL);
    sequence->count++;
}

static void sequence_free(Sequence *sequence) {
    for (size_t i = 0; i < sequence->count; i++) {
        free(sequence->labels[i]);
    }
    free(sequence->labels);
    sequence->labels = NULL;
    sequence->count = 0;
}

/* The n-grams of a sequence, added to a tally. */
static void tally_ngrams(Tally *tally, const Sequence *sequence, size_t n) {
    for (size_t start = 0; start + n <= sequence->count; start++) {
        char joined[256] = "";
        for (size_t i = 0; i < n; i++) {
            if (i > 0) {
                strcat(joined, " > ");
            }
            strcat(joined, sequence->labels[start + i]);
        }
        tally_add(tally, joined);
    }
}

static const char *cadence_label(CadenceType type) {
    switch (type) {
        case CADENCE_TYPE_PERFECT_AUTHENTIC:
            return "pac";
        case CADENCE_TYPE_IMPERFECT_AUTHENTIC:
            return "iac";
        case CADENCE_TYPE_HALF:
            return "hc";
        case CADENCE_TYPE_DECEPTIVE:
            return "deceptive";
        case CADENCE_TYPE_PLAGAL:
            return "plagal";
        case CADENCE_TYPE_SUBVERTED:
            return "subverted";
        default:
            return "other";
    }
}

/* Same labels, same counts, same total, whatever the order. */
static void assert_counts_equal_tally(const CorpusStatsCounts *counts, const Tally *tally) {
    assert(counts->entry_count == tally->count);
    assert(counts->total == tally->total);
    for (size_t i = 0; i < counts->entry_count; i++) {
        int found = 0;
        for (size_t j = 0; j < tally->count; j++) {
            if (strcmp(counts->entries[i].label, tally->items[j].label) == 0) {
                assert(counts->entries[i].count == tally->items[j].count);
                found = 1;
            }
        }
        assert(found);
        if (i > 0) {
            assert(counts->entries[i - 1].count >= counts->entries[i].count);
        }
    }
}

/* Persists a piece's chords and cadences into its context and adds them to the sequences. */
static void persist_piece(PersistenceHandle *persistence, const Chain *c, const char *piece_ref, Sequence *chords,
                          Sequence *cadences) {
    char id[160];
    size_t stored = 0;
    for (size_t i = 0; i < c->count; i++) {
        if (!c->chords[i].matched || c->chords[i].best_chord == NULL) {
            continue;
        }
        const char *label = chord_label(c->chords[i].best_chord);
        snprintf(id, sizeof(id), "%s/chord/%zu", piece_ref, stored++);
        stats_store_chord(persistence, id, label, piece_ref);
        sequence_add(chords, label);
    }
    for (size_t k = 0; k < c->cadences.count; k++) {
        const char *label = cadence_label(c->cadences.items[k].type);
        snprintf(id, sizeof(id), "%s/cadence/%zu", piece_ref, k);
        stats_store_claim(persistence, id, CORPUS_STATS_CADENCE_CLAIM_TYPE, label, strlen(label) + 1, piece_ref);
        sequence_add(cadences, label);
    }
}

static void check_corpus(CorpusStoreHandle *corpus, PersistenceHandle *persistence, const char *corpus_id,
                         const Sequence *chord_sequences, const Sequence *cadence_sequences, size_t piece_count) {
    for (size_t n = 2; n <= 3; n++) {
        Tally *expected = tally_new();
        for (size_t p = 0; p < piece_count; p++) {
            tally_ngrams(expected, &chord_sequences[p], n);
        }
        CorpusStatsCounts counts = {NULL, 0, 0};
        CorpusStatsResult result = corpus_stats_chord_progressions(corpus, persistence, corpus_id, n, &counts);
        if (expected->count == 0) {
            assert(result == CORPUS_STATS_NO_DATA);
        } else {
            assert(result == CORPUS_STATS_OK);
            assert_counts_equal_tally(&counts, expected);
            corpus_stats_counts_free(&counts);
        }
        free(expected);
    }

    Tally *expected = tally_new();
    for (size_t p = 0; p < piece_count; p++) {
        for (size_t i = 0; i < cadence_sequences[p].count; i++) {
            tally_add(expected, cadence_sequences[p].labels[i]);
        }
    }
    CorpusStatsCounts counts = {NULL, 0, 0};
    CorpusStatsResult result = corpus_stats_cadence_types(corpus, persistence, corpus_id, &counts);
    if (expected->count == 0) {
        assert(result == CORPUS_STATS_NO_DATA);
    } else {
        assert(result == CORPUS_STATS_OK);
        assert_counts_equal_tally(&counts, expected);
        corpus_stats_counts_free(&counts);
    }
    free(expected);
}

/*
 * KNOWN COUNTS. What the chain finds on each piece, pinned: the cadence types it classifies, one chord progression of length 2 and the top one
 * of length 3, each with its count and the n-gram total of the piece. These are the chain's own results (cross-checked above against a tally
 * taken straight from the chain), not the hand annotations: the chain finds more cadences than the annotated ones in places (Mozart K. 545:
 * 14 against 4), a listed deviation of the cadence stage (tests/fixtures/framework-v1-corpus/expected_deviations.md). Field 5, whose
 * minor region is read in the minor vocabulary, has no cadence the chain finds.
 */
typedef struct {
    const char *cadence_labels[4];
    unsigned long cadence_counts[4];
    size_t cadence_entries;
    const char *pair;
    unsigned long pair_count;
    unsigned long pair_total;
    const char *triple;
    unsigned long triple_count;
    unsigned long triple_total;
} Known;

static const Known KNOWN[5] = {
    /* Bach 114 */ {{"subverted", "pac"}, {1, 2}, 2, "I > I", 19, 148, "I > I > I", 8, 147},
    /* Mozart 545 */ {{"pac", "subverted", "other"}, {10, 2, 2}, 3, "I > I", 101, 578, "I > I > I", 61, 577},
    /* Clementi 36 */ {{"hc", "pac", "iac", "subverted"}, {7, 3, 2, 1}, 4, "I > V", 7, 198, "I > I > I", 13, 197},
    /* Field 1 */ {{"pac", "hc"}, {1, 2}, 2, "I > I", 24, 59, "I > I > I", 19, 58},
    /* Field 5 */ {{NULL}, {0}, 0, "I > I", 9, 28, "I > I > I", 7, 27},
};

static unsigned long count_of(const CorpusStatsCounts *counts, const char *label) {
    for (size_t i = 0; i < counts->entry_count; i++) {
        if (strcmp(counts->entries[i].label, label) == 0) {
            return counts->entries[i].count;
        }
    }
    return 0;
}

static void check_known(CorpusStoreHandle *corpus, PersistenceHandle *persistence, size_t piece) {
    char corpus_id[64];
    snprintf(corpus_id, sizeof(corpus_id), "piece-%zu", piece);
    const Known *known = &KNOWN[piece];
    CorpusStatsCounts counts = {NULL, 0, 0};

    CorpusStatsResult result = corpus_stats_cadence_types(corpus, persistence, corpus_id, &counts);
    if (known->cadence_entries == 0) {
        assert(result == CORPUS_STATS_NO_DATA);
    } else {
        assert(result == CORPUS_STATS_OK);
        assert(counts.entry_count == known->cadence_entries);
        unsigned long total = 0;
        for (size_t i = 0; i < known->cadence_entries; i++) {
            assert(count_of(&counts, known->cadence_labels[i]) == known->cadence_counts[i]);
            total += known->cadence_counts[i];
        }
        assert(counts.total == total);
        corpus_stats_counts_free(&counts);
    }

    assert(corpus_stats_chord_progressions(corpus, persistence, corpus_id, 2, &counts) == CORPUS_STATS_OK);
    assert(counts.total == known->pair_total && count_of(&counts, known->pair) == known->pair_count);
    corpus_stats_counts_free(&counts);
    assert(corpus_stats_chord_progressions(corpus, persistence, corpus_id, 3, &counts) == CORPUS_STATS_OK);
    assert(counts.total == known->triple_total && count_of(&counts, known->triple) == known->triple_count);
    corpus_stats_counts_free(&counts);
}

int main(void) {
    const char *path = "test_corpus_style_statistics.sqlite";
    remove(path);
    CorpusStoreHandle *corpus = NULL;
    assert(corpus_api_open(path, &corpus) == CORPUS_API_OK);
    PersistenceHandle *persistence = NULL;
    assert(persistence_open(path, &persistence) == PERSISTENCE_OK);
    assert(corpus_api_create_corpus(corpus, "all-five", "Framework v1 reference set") == CORPUS_API_OK);

    Sequence chords[5] = {{NULL, 0}, {NULL, 0}, {NULL, 0}, {NULL, 0}, {NULL, 0}};
    Sequence cadences[5] = {{NULL, 0}, {NULL, 0}, {NULL, 0}, {NULL, 0}, {NULL, 0}};
    for (size_t i = 0; i < 5; i++) {
        Chain c;
        fv1_chain_run(&c, &FORM_PIECES[i]);
        const char *piece_ref = FORM_PIECES[i].piece.midi;
        persist_piece(persistence, &c, piece_ref, &chords[i], &cadences[i]);
        printf("%s: %zu chords, %zu cadences persisted\n", piece_ref, chords[i].count, cadences[i].count);
        assert(chords[i].count > 20); /* the check has something to count */
        fv1_chain_destroy(&c);

        /* A Corpus of this piece alone. */
        char corpus_id[64];
        snprintf(corpus_id, sizeof(corpus_id), "piece-%zu", i);
        assert(corpus_api_create_corpus(corpus, corpus_id, piece_ref) == CORPUS_API_OK);
        assert(corpus_api_add_piece_ref(corpus, corpus_id, piece_ref) == CORPUS_API_OK);
        assert(corpus_api_add_piece_ref(corpus, "all-five", piece_ref) == CORPUS_API_OK);
    }

    for (size_t i = 0; i < 5; i++) {
        char corpus_id[64];
        snprintf(corpus_id, sizeof(corpus_id), "piece-%zu", i);
        check_corpus(corpus, persistence, corpus_id, &chords[i], &cadences[i], 1);
        check_known(corpus, persistence, i);
    }
    check_corpus(corpus, persistence, "all-five", chords, cadences, 5);

    /* A corpus of a piece that was never analysed has no data. */
    assert(corpus_api_create_corpus(corpus, "unanalysed", "unanalysed") == CORPUS_API_OK);
    assert(corpus_api_add_piece_ref(corpus, "unanalysed", "Never Analysed.mid") == CORPUS_API_OK);
    CorpusStatsCounts counts = {NULL, 0, 0};
    assert(corpus_stats_chord_progressions(corpus, persistence, "unanalysed", 2, &counts) == CORPUS_STATS_NO_DATA);
    assert(corpus_stats_cadence_types(corpus, persistence, "unanalysed", &counts) == CORPUS_STATS_NO_DATA);

    for (size_t i = 0; i < 5; i++) {
        sequence_free(&chords[i]);
        sequence_free(&cadences[i]);
    }
    persistence_close(persistence);
    corpus_api_close(corpus);
    remove(path);
    printf("test_corpus_style_statistics: ok\n");
    return 0;
}
