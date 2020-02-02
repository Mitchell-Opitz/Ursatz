#include "corpus_stats/corpus_stats_api.h"

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "corpus/corpus_api.h"
#include "identity/entity_id.h"
#include "interpretation/chord.h"
#include "persistence/persistence.h"
#include "persistence/persistence_chord.h"
#include "triad_identification.h"

/*
 * corpus_stats_most_common_chord reads across the Corpus/Persistence
 * boundary, so a test needs two handles opened against the same
 * underlying store: a CorpusStoreHandle (for Corpus/membership) and a
 * plain PersistenceHandle (to store the Chords the piece refs point
 * at) -- exactly the two-handle shape the function itself takes.
 */

static Chord *make_chord(const char *sonority_target_id, const int *scale_degrees, size_t scale_degree_count) {
    EntityID *target_id = NULL;
    assert(entity_id_create(sonority_target_id, &target_id) == ENTITY_ID_OK);
    Chord *chord = NULL;
    assert(common_practice_identify_triad_chord(scale_degrees, scale_degree_count, target_id, &chord));
    return chord;
}

static const char *store_chord(PersistenceHandle *persistence, Chord *chord) {
    assert(persistence_store_chord(persistence, chord) == PERSISTENCE_CHORD_OK);
    return entity_id_value(interpretation_id(chord_interpretation(chord)));
}

static void test_tally_and_tie_break(void) {
    const char *path = "test_corpus_stats_tally.sqlite";
    remove(path);

    CorpusStoreHandle *corpus_handle = NULL;
    assert(corpus_api_open(path, &corpus_handle) == CORPUS_API_OK);
    PersistenceHandle *persistence = NULL;
    assert(persistence_open(path, &persistence) == PERSISTENCE_OK);

    assert(corpus_api_create_corpus(corpus_handle, "corpus-1", "Chorales") == CORPUS_API_OK);

    /* Tonic (I) triad, stored twice; a lone dominant (V) triad. */
    int tonic_degrees[] = {1, 3, 5};
    int dominant_degrees[] = {5, 7, 2};

    Chord *chord_a = make_chord("sonority-a", tonic_degrees, 3);
    Chord *chord_b = make_chord("sonority-b", tonic_degrees, 3);
    Chord *chord_c = make_chord("sonority-c", dominant_degrees, 3);
    assert(strcmp(chord_label(chord_a), chord_label(chord_b)) == 0);
    assert(strcmp(chord_label(chord_a), chord_label(chord_c)) != 0);

    char id_a[128], id_b[128], id_c[128];
    snprintf(id_a, sizeof(id_a), "%s", store_chord(persistence, chord_a));
    snprintf(id_b, sizeof(id_b), "%s", store_chord(persistence, chord_b));
    snprintf(id_c, sizeof(id_c), "%s", store_chord(persistence, chord_c));

    assert(corpus_api_add_piece_ref(corpus_handle, "corpus-1", id_a) == CORPUS_API_OK);
    assert(corpus_api_add_piece_ref(corpus_handle, "corpus-1", id_b) == CORPUS_API_OK);
    assert(corpus_api_add_piece_ref(corpus_handle, "corpus-1", id_c) == CORPUS_API_OK);

    char *most_common = NULL;
    assert(corpus_stats_most_common_chord(corpus_handle, persistence, "corpus-1", &most_common) == CORPUS_STATS_OK);
    assert(strcmp(most_common, chord_label(chord_a)) == 0);
    free(most_common);

    chord_destroy(chord_a);
    chord_destroy(chord_b);
    chord_destroy(chord_c);
    persistence_close(persistence);
    corpus_api_close(corpus_handle);
    remove(path);
}

static void test_tie_break_first_encountered(void) {
    const char *path = "test_corpus_stats_tie_break.sqlite";
    remove(path);

    CorpusStoreHandle *corpus_handle = NULL;
    assert(corpus_api_open(path, &corpus_handle) == CORPUS_API_OK);
    PersistenceHandle *persistence = NULL;
    assert(persistence_open(path, &persistence) == PERSISTENCE_OK);

    assert(corpus_api_create_corpus(corpus_handle, "corpus-1", "Chorales") == CORPUS_API_OK);

    int tonic_degrees[] = {1, 3, 5};
    int dominant_degrees[] = {5, 7, 2};

    /* Dominant added first, tonic second -- each occurs once, so the
     * first-encountered piece-ref-order winner is the dominant label. */
    Chord *chord_dominant = make_chord("sonority-dominant", dominant_degrees, 3);
    Chord *chord_tonic = make_chord("sonority-tonic", tonic_degrees, 3);

    char id_dominant[128], id_tonic[128];
    snprintf(id_dominant, sizeof(id_dominant), "%s", store_chord(persistence, chord_dominant));
    snprintf(id_tonic, sizeof(id_tonic), "%s", store_chord(persistence, chord_tonic));

    assert(corpus_api_add_piece_ref(corpus_handle, "corpus-1", id_dominant) == CORPUS_API_OK);
    assert(corpus_api_add_piece_ref(corpus_handle, "corpus-1", id_tonic) == CORPUS_API_OK);

    char *most_common = NULL;
    assert(corpus_stats_most_common_chord(corpus_handle, persistence, "corpus-1", &most_common) == CORPUS_STATS_OK);
    assert(strcmp(most_common, chord_label(chord_dominant)) == 0);
    free(most_common);

    chord_destroy(chord_dominant);
    chord_destroy(chord_tonic);
    persistence_close(persistence);
    corpus_api_close(corpus_handle);
    remove(path);
}

static void test_empty_corpus_is_no_data(void) {
    const char *path = "test_corpus_stats_empty.sqlite";
    remove(path);

    CorpusStoreHandle *corpus_handle = NULL;
    assert(corpus_api_open(path, &corpus_handle) == CORPUS_API_OK);
    PersistenceHandle *persistence = NULL;
    assert(persistence_open(path, &persistence) == PERSISTENCE_OK);

    assert(corpus_api_create_corpus(corpus_handle, "corpus-empty", "Empty") == CORPUS_API_OK);

    char *most_common = NULL;
    assert(corpus_stats_most_common_chord(corpus_handle, persistence, "corpus-empty", &most_common) ==
           CORPUS_STATS_NO_DATA);
    assert(most_common == NULL);

    persistence_close(persistence);
    corpus_api_close(corpus_handle);
    remove(path);
}

static void test_unresolvable_ref_is_skipped(void) {
    const char *path = "test_corpus_stats_skip.sqlite";
    remove(path);

    CorpusStoreHandle *corpus_handle = NULL;
    assert(corpus_api_open(path, &corpus_handle) == CORPUS_API_OK);
    PersistenceHandle *persistence = NULL;
    assert(persistence_open(path, &persistence) == PERSISTENCE_OK);

    assert(corpus_api_create_corpus(corpus_handle, "corpus-1", "Chorales") == CORPUS_API_OK);

    int tonic_degrees[] = {1, 3, 5};
    Chord *chord = make_chord("sonority-only", tonic_degrees, 3);
    char id[128];
    snprintf(id, sizeof(id), "%s", store_chord(persistence, chord));

    /* A piece ref that was never stored as a Chord anywhere. */
    assert(corpus_api_add_piece_ref(corpus_handle, "corpus-1", "never-stored") == CORPUS_API_OK);
    assert(corpus_api_add_piece_ref(corpus_handle, "corpus-1", id) == CORPUS_API_OK);

    char *most_common = NULL;
    assert(corpus_stats_most_common_chord(corpus_handle, persistence, "corpus-1", &most_common) == CORPUS_STATS_OK);
    assert(strcmp(most_common, chord_label(chord)) == 0);
    free(most_common);

    chord_destroy(chord);
    persistence_close(persistence);
    corpus_api_close(corpus_handle);
    remove(path);
}

int main(void) {
    test_tally_and_tie_break();
    test_tie_break_first_encountered();
    test_empty_corpus_is_no_data();
    test_unresolvable_ref_is_skipped();
    printf("corpus_stats_api: all tests passed\n");
    return 0;
}
