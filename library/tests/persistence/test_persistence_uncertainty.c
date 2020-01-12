#include "persistence/persistence_uncertainty.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "persistence/persistence.h"

/*
 * Uncertainty round-trip tests: all three implemented kinds (unknown,
 * scalar confidence, candidate set) store and reload with exact
 * numerator/denominator and label equality.
 */

static PersistenceHandle *open_test_db(const char *path) {
    remove(path);
    PersistenceHandle *handle = NULL;
    assert(persistence_open(path, &handle) == PERSISTENCE_OK);
    return handle;
}

static void test_round_trip_unknown(void) {
    const char *path = "test_persistence_uncertainty_unknown.sqlite";
    PersistenceHandle *handle = open_test_db(path);

    Uncertainty *original = NULL;
    assert(uncertainty_create_unknown(&original) == UNCERTAINTY_OK);

    sqlite3_int64 id = 0;
    assert(persistence_store_uncertainty(handle, original, &id) == PERSISTENCE_UNCERTAINTY_OK);

    Uncertainty *reloaded = NULL;
    assert(persistence_load_uncertainty(handle, id, &reloaded) == PERSISTENCE_UNCERTAINTY_OK);
    assert(uncertainty_kind(reloaded) == UNCERTAINTY_KIND_UNKNOWN);

    uncertainty_destroy(original);
    uncertainty_destroy(reloaded);
    persistence_close(handle);
    remove(path);
}

static void test_round_trip_scalar_confidence(void) {
    const char *path = "test_persistence_uncertainty_scalar.sqlite";
    PersistenceHandle *handle = open_test_db(path);

    Uncertainty *original = NULL;
    assert(uncertainty_create_scalar_confidence(3, 4, &original) == UNCERTAINTY_OK);

    sqlite3_int64 id = 0;
    assert(persistence_store_uncertainty(handle, original, &id) == PERSISTENCE_UNCERTAINTY_OK);

    Uncertainty *reloaded = NULL;
    assert(persistence_load_uncertainty(handle, id, &reloaded) == PERSISTENCE_UNCERTAINTY_OK);
    assert(uncertainty_kind(reloaded) == UNCERTAINTY_KIND_SCALAR_CONFIDENCE);
    assert(uncertainty_scalar_confidence_numerator(reloaded) == 3);
    assert(uncertainty_scalar_confidence_denominator(reloaded) == 4);

    uncertainty_destroy(original);
    uncertainty_destroy(reloaded);
    persistence_close(handle);
    remove(path);
}

static void test_round_trip_candidate_set(void) {
    const char *path = "test_persistence_uncertainty_candidates.sqlite";
    PersistenceHandle *handle = open_test_db(path);

    UncertaintyCandidate candidates[2] = {
        {.label = "V7", .weight_numerator = 3, .weight_denominator = 5},
        {.label = "ii6", .weight_numerator = 2, .weight_denominator = 5},
    };

    Uncertainty *original = NULL;
    assert(uncertainty_create_candidate_set(candidates, 2, &original) == UNCERTAINTY_OK);

    sqlite3_int64 id = 0;
    assert(persistence_store_uncertainty(handle, original, &id) == PERSISTENCE_UNCERTAINTY_OK);

    Uncertainty *reloaded = NULL;
    assert(persistence_load_uncertainty(handle, id, &reloaded) == PERSISTENCE_UNCERTAINTY_OK);
    assert(uncertainty_kind(reloaded) == UNCERTAINTY_KIND_CANDIDATE_SET);
    assert(uncertainty_candidate_count(reloaded) == 2);

    for (size_t i = 0; i < 2; i++) {
        assert(strcmp(uncertainty_candidate_label_at(reloaded, i), candidates[i].label) == 0);
        assert(uncertainty_candidate_weight_numerator_at(reloaded, i) == candidates[i].weight_numerator);
        assert(uncertainty_candidate_weight_denominator_at(reloaded, i) == candidates[i].weight_denominator);
    }

    uncertainty_destroy(original);
    uncertainty_destroy(reloaded);
    persistence_close(handle);
    remove(path);
}

int main(void) {
    test_round_trip_unknown();
    test_round_trip_scalar_confidence();
    test_round_trip_candidate_set();
    printf("persistence uncertainty: all tests passed\n");
    return 0;
}
