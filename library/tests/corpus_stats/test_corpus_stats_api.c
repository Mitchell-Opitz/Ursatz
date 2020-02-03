#include "corpus_stats/corpus_stats_api.h"

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "core/framework_reference.h"
#include "core/provenance_reference.h"
#include "core/value.h"
#include "corpus/corpus_api.h"
#include "identity/entity_id.h"
#include "interpretation/chord.h"
#include "interpretation/uncertainty.h"
#include "persistence/persistence.h"
#include "persistence/persistence_chord.h"

/*
 * corpus_stats_most_common_chord reads across the Corpus/Persistence
 * boundary, so a test needs two handles opened against the same
 * underlying store: a CorpusStoreHandle (for Corpus/membership) and a
 * plain PersistenceHandle (to store the Chords the piece refs point
 * at) -- exactly the two-handle shape the function itself takes.
 *
 * Chords are built directly (not via a chord-vocabulary helper) so
 * each has its own unique interpretation id -- two independently
 * persisted Chords sharing the same label (as would happen across two
 * different pieces both containing, say, a "V7" chord) is exactly the
 * case the tally needs to count, and every id here must be distinct
 * for persistence_store_chord's PRIMARY KEY.
 */

static Chord *make_chord(const char *id_value, const char *sonority_target_id, const char *label) {
    EntityID *id = NULL;
    assert(entity_id_create(id_value, &id) == ENTITY_ID_OK);
    EntityID *target_id = NULL;
    assert(entity_id_create(sonority_target_id, &target_id) == ENTITY_ID_OK);

    Value *claim = NULL;
    assert(value_create("chord-label", label, strlen(label) + 1, &claim) == VALUE_OK);

    EntityID *framework_target_id = NULL;
    assert(entity_id_create("test-framework", &framework_target_id) == ENTITY_ID_OK);
    FrameworkReference *framework_reference = NULL;
    assert(framework_reference_create(framework_target_id, &framework_reference) == FRAMEWORK_REFERENCE_OK);

    Uncertainty *uncertainty = NULL;
    assert(uncertainty_create_unknown(&uncertainty) == UNCERTAINTY_OK);

    ProvenanceReference *provenance = NULL;
    assert(provenance_reference_create_absent(&provenance) == PROVENANCE_REFERENCE_OK);

    Interpretation *interpretation = NULL;
    assert(interpretation_create(id, target_id, claim, NULL, NULL, framework_reference, uncertainty, "test-author",
                                  "test-method", INTERPRETATION_STATUS_MACHINE_GENERATED, provenance,
                                  &interpretation) == INTERPRETATION_OK);

    Chord *chord = NULL;
    assert(chord_create(interpretation, label, &chord) == CHORD_OK);
    return chord;
}

static void store_chord(PersistenceHandle *persistence, Chord *chord) {
    assert(persistence_store_chord(persistence, chord) == PERSISTENCE_CHORD_OK);
}

static void test_tally_and_tie_break(void) {
    const char *path = "test_corpus_stats_tally.sqlite";
    remove(path);

    CorpusStoreHandle *corpus_handle = NULL;
    assert(corpus_api_open(path, &corpus_handle) == CORPUS_API_OK);
    PersistenceHandle *persistence = NULL;
    assert(persistence_open(path, &persistence) == PERSISTENCE_OK);

    assert(corpus_api_create_corpus(corpus_handle, "corpus-1", "Chorales") == CORPUS_API_OK);

    Chord *chord_a = make_chord("chord-a", "sonority-a", "I");
    Chord *chord_b = make_chord("chord-b", "sonority-b", "I");
    Chord *chord_c = make_chord("chord-c", "sonority-c", "V7");

    store_chord(persistence, chord_a);
    store_chord(persistence, chord_b);
    store_chord(persistence, chord_c);

    assert(corpus_api_add_piece_ref(corpus_handle, "corpus-1", "chord-a") == CORPUS_API_OK);
    assert(corpus_api_add_piece_ref(corpus_handle, "corpus-1", "chord-b") == CORPUS_API_OK);
    assert(corpus_api_add_piece_ref(corpus_handle, "corpus-1", "chord-c") == CORPUS_API_OK);

    char *most_common = NULL;
    assert(corpus_stats_most_common_chord(corpus_handle, persistence, "corpus-1", &most_common) == CORPUS_STATS_OK);
    assert(strcmp(most_common, "I") == 0);
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

    /* V7 added first, I second -- each occurs once, so the
     * first-encountered piece-ref-order winner is V7. */
    Chord *chord_dominant = make_chord("chord-dominant", "sonority-dominant", "V7");
    Chord *chord_tonic = make_chord("chord-tonic", "sonority-tonic", "I");

    store_chord(persistence, chord_dominant);
    store_chord(persistence, chord_tonic);

    assert(corpus_api_add_piece_ref(corpus_handle, "corpus-1", "chord-dominant") == CORPUS_API_OK);
    assert(corpus_api_add_piece_ref(corpus_handle, "corpus-1", "chord-tonic") == CORPUS_API_OK);

    char *most_common = NULL;
    assert(corpus_stats_most_common_chord(corpus_handle, persistence, "corpus-1", &most_common) == CORPUS_STATS_OK);
    assert(strcmp(most_common, "V7") == 0);
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

    Chord *chord = make_chord("chord-only", "sonority-only", "I");
    store_chord(persistence, chord);

    /* A piece ref that was never stored as a Chord anywhere. */
    assert(corpus_api_add_piece_ref(corpus_handle, "corpus-1", "never-stored") == CORPUS_API_OK);
    assert(corpus_api_add_piece_ref(corpus_handle, "corpus-1", "chord-only") == CORPUS_API_OK);

    char *most_common = NULL;
    assert(corpus_stats_most_common_chord(corpus_handle, persistence, "corpus-1", &most_common) == CORPUS_STATS_OK);
    assert(strcmp(most_common, "I") == 0);
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
