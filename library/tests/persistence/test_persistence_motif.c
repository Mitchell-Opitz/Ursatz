#include "structure_test_util.h"

#include "persistence/persistence_motif.h"
#include "persistence/persistence_motif_occurrence.h"

/* Motifs and MotifOccurrences: round trips, event id order, duplicate, missing, NULL arguments, a failed store, lists. */

static const char *const EVENTS[] = {"ev-9", "ev-1", "ev-5", "ev-1", "ev-0"}; /* deliberately not sorted, one repeated */

static void test_motif_round_trip_keeps_event_order(void) {
    const char *path = "test_persistence_motif_round_trip.sqlite";
    PersistenceHandle *handle = su_open(path);
    Motif *original = su_motif("motif-1", EVENTS, 5);
    assert(persistence_store_motif(handle, original) == PERSISTENCE_MOTIF_OK);
    assert(su_count(handle, "motif_events") == 5);

    Motif *loaded = NULL;
    assert(persistence_load_motif(handle, "motif-1", &loaded) == PERSISTENCE_MOTIF_OK);
    su_assert_same_motif(original, loaded);
    assert(strcmp(entity_id_value(motif_event_id_at(loaded, 0)), "ev-9") == 0);
    assert(strcmp(entity_id_value(motif_event_id_at(loaded, 4)), "ev-0") == 0);
    motif_destroy(loaded);

    persistence_close(handle);
    assert(persistence_open(path, &handle) == PERSISTENCE_OK);
    assert(persistence_load_motif(handle, "motif-1", &loaded) == PERSISTENCE_MOTIF_OK);
    su_assert_same_motif(original, loaded);
    motif_destroy(loaded);

    /* the smallest motif: one event */
    const char *one[] = {"only"};
    Motif *single = su_motif("motif-2", one, 1);
    assert(persistence_store_motif(handle, single) == PERSISTENCE_MOTIF_OK);
    assert(persistence_load_motif(handle, "motif-2", &loaded) == PERSISTENCE_MOTIF_OK);
    su_assert_same_motif(single, loaded);
    motif_destroy(loaded);
    motif_destroy(single);
    motif_destroy(original);
    persistence_close(handle);
    remove(path);
}

static void test_motif_duplicate_missing_null_and_failed(void) {
    const char *path = "test_persistence_motif_errors.sqlite";
    PersistenceHandle *handle = su_open(path);

    const char *first_events[] = {"a", "b"};
    const char *second_events[] = {"c", "d", "e"};
    Motif *first = su_motif("motif-1", first_events, 2);
    Motif *second = su_motif("motif-1", second_events, 3);
    assert(persistence_store_motif(handle, first) == PERSISTENCE_MOTIF_OK);
    assert(persistence_store_motif(handle, second) == PERSISTENCE_MOTIF_ERR_DUPLICATE_ID);
    assert(su_count(handle, "motifs") == 1 && su_count(handle, "motif_events") == 2);
    Motif *loaded = NULL;
    assert(persistence_load_motif(handle, "motif-1", &loaded) == PERSISTENCE_MOTIF_OK);
    su_assert_same_motif(first, loaded);
    motif_destroy(loaded);

    Motif *out = (Motif *)(uintptr_t)0x1;
    assert(persistence_load_motif(handle, "nope", &out) == PERSISTENCE_MOTIF_ERR_NOT_FOUND && out == (Motif *)(uintptr_t)0x1);
    assert(persistence_store_motif(NULL, first) == PERSISTENCE_MOTIF_ERR_NULL_ARG);
    assert(persistence_store_motif(handle, NULL) == PERSISTENCE_MOTIF_ERR_NULL_ARG);
    assert(persistence_load_motif(NULL, "motif-1", &out) == PERSISTENCE_MOTIF_ERR_NULL_ARG);
    assert(persistence_load_motif(handle, NULL, &out) == PERSISTENCE_MOTIF_ERR_NULL_ARG);
    assert(persistence_load_motif(handle, "motif-1", NULL) == PERSISTENCE_MOTIF_ERR_NULL_ARG);

    /* a store that fails on its third event leaves neither the parent row nor the first two events */
    su_poison(handle, "motif_events", "event_id", "poison");
    const char *poisoned[] = {"x", "y", "poison", "z"};
    Motif *bad = su_motif("motif-bad", poisoned, 4);
    assert(persistence_store_motif(handle, bad) == PERSISTENCE_MOTIF_ERR_DB);
    assert(su_count(handle, "motifs") == 1 && su_count(handle, "motif_events") == 2);
    assert(persistence_load_motif(handle, "motif-bad", &out) == PERSISTENCE_MOTIF_ERR_NOT_FOUND);
    Motif *good = su_motif("motif-good", second_events, 3);
    assert(persistence_store_motif(handle, good) == PERSISTENCE_MOTIF_OK);
    assert(su_count(handle, "motifs") == 2 && su_count(handle, "motif_events") == 5);

    motif_destroy(first);
    motif_destroy(second);
    motif_destroy(bad);
    motif_destroy(good);
    persistence_close(handle);
    remove(path);
}

static void test_a_failed_store_inside_a_callers_transaction(void) {
    const char *path = "test_persistence_motif_nested.sqlite";
    PersistenceHandle *handle = su_open(path);
    su_poison(handle, "motif_events", "event_id", "poison");
    assert(sqlite3_exec(persistence_db(handle), "BEGIN", NULL, NULL, NULL) == SQLITE_OK);
    const char *fine[] = {"a"};
    const char *poisoned[] = {"a", "poison"};
    Motif *good = su_motif("m-good", fine, 1);
    Motif *bad = su_motif("m-bad", poisoned, 2);
    assert(persistence_store_motif(handle, good) == PERSISTENCE_MOTIF_OK);
    assert(persistence_store_motif(handle, bad) == PERSISTENCE_MOTIF_ERR_DB);
    assert(sqlite3_exec(persistence_db(handle), "COMMIT", NULL, NULL, NULL) == SQLITE_OK);
    /* the caller's own work survives; only the failed store was undone */
    assert(su_count(handle, "motifs") == 1 && su_count(handle, "motif_events") == 1);
    motif_destroy(good);
    motif_destroy(bad);
    persistence_close(handle);
    remove(path);
}

static void test_occurrence_round_trip_and_errors(void) {
    const char *path = "test_persistence_motif_occurrence.sqlite";
    PersistenceHandle *handle = su_open(path);

    /* the motif is never stored: an occurrence names it by id only */
    MotifOccurrence *original = su_motif_occurrence("occ-1", "motif-not-stored", EVENTS, 5);
    assert(persistence_store_motif_occurrence(handle, original) == PERSISTENCE_MOTIF_OCCURRENCE_OK);
    MotifOccurrence *loaded = NULL;
    assert(persistence_load_motif_occurrence(handle, "occ-1", &loaded) == PERSISTENCE_MOTIF_OCCURRENCE_OK);
    su_assert_same_motif_occurrence(original, loaded);
    assert(strcmp(entity_id_value(motif_occurrence_motif_id(loaded)), "motif-not-stored") == 0);
    motif_occurrence_destroy(loaded);

    const char *other[] = {"p", "q"};
    MotifOccurrence *duplicate = su_motif_occurrence("occ-1", "another", other, 2);
    assert(persistence_store_motif_occurrence(handle, duplicate) == PERSISTENCE_MOTIF_OCCURRENCE_ERR_DUPLICATE_ID);
    assert(su_count(handle, "motif_occurrences") == 1 && su_count(handle, "motif_occurrence_events") == 5);
    assert(persistence_load_motif_occurrence(handle, "occ-1", &loaded) == PERSISTENCE_MOTIF_OCCURRENCE_OK);
    su_assert_same_motif_occurrence(original, loaded);
    motif_occurrence_destroy(loaded);

    MotifOccurrence *out = (MotifOccurrence *)(uintptr_t)0x1;
    assert(persistence_load_motif_occurrence(handle, "nope", &out) == PERSISTENCE_MOTIF_OCCURRENCE_ERR_NOT_FOUND);
    assert(out == (MotifOccurrence *)(uintptr_t)0x1);
    assert(persistence_store_motif_occurrence(NULL, original) == PERSISTENCE_MOTIF_OCCURRENCE_ERR_NULL_ARG);
    assert(persistence_store_motif_occurrence(handle, NULL) == PERSISTENCE_MOTIF_OCCURRENCE_ERR_NULL_ARG);
    assert(persistence_load_motif_occurrence(handle, NULL, &out) == PERSISTENCE_MOTIF_OCCURRENCE_ERR_NULL_ARG);
    assert(persistence_load_motif_occurrence(handle, "occ-1", NULL) == PERSISTENCE_MOTIF_OCCURRENCE_ERR_NULL_ARG);
    MotifOccurrence **list = NULL;
    size_t count = 0;
    assert(persistence_list_motif_occurrences_by_motif_id(handle, NULL, &list, &count) ==
           PERSISTENCE_MOTIF_OCCURRENCE_ERR_NULL_ARG);
    assert(persistence_list_motif_occurrences_by_motif_id(NULL, "m", &list, &count) ==
           PERSISTENCE_MOTIF_OCCURRENCE_ERR_NULL_ARG);

    su_poison(handle, "motif_occurrence_events", "event_id", "poison");
    const char *poisoned[] = {"x", "poison"};
    MotifOccurrence *bad = su_motif_occurrence("occ-bad", "m", poisoned, 2);
    assert(persistence_store_motif_occurrence(handle, bad) == PERSISTENCE_MOTIF_OCCURRENCE_ERR_DB);
    assert(su_count(handle, "motif_occurrences") == 1 && su_count(handle, "motif_occurrence_events") == 5);

    motif_occurrence_destroy(original);
    motif_occurrence_destroy(duplicate);
    motif_occurrence_destroy(bad);
    persistence_close(handle);
    remove(path);
}

static void test_list_occurrences_by_motif(void) {
    const char *path = "test_persistence_motif_occurrence_list.sqlite";
    PersistenceHandle *handle = su_open(path);
    const char *a[] = {"e1", "e2"};
    const char *b[] = {"e3"};
    const char *c[] = {"e9", "e4", "e5"};
    MotifOccurrence *items[] = {su_motif_occurrence("occ-z", "M1", a, 2), su_motif_occurrence("occ-m", "M2", b, 1),
                                su_motif_occurrence("occ-a", "M1", c, 3)};
    for (size_t i = 0; i < 3; i++) {
        assert(persistence_store_motif_occurrence(handle, items[i]) == PERSISTENCE_MOTIF_OCCURRENCE_OK);
    }

    MotifOccurrence **list = NULL;
    size_t count = 0;
    assert(persistence_list_motif_occurrences_by_motif_id(handle, "M1", &list, &count) ==
           PERSISTENCE_MOTIF_OCCURRENCE_OK);
    assert(count == 2); /* insertion order, not id order */
    su_assert_same_motif_occurrence(items[0], list[0]);
    su_assert_same_motif_occurrence(items[2], list[1]);
    for (size_t i = 0; i < count; i++) {
        motif_occurrence_destroy(list[i]);
    }
    free(list);

    assert(persistence_list_motif_occurrences_by_motif_id(handle, "M2", &list, &count) ==
           PERSISTENCE_MOTIF_OCCURRENCE_OK);
    assert(count == 1);
    su_assert_same_motif_occurrence(items[1], list[0]);
    motif_occurrence_destroy(list[0]);
    free(list);

    list = (MotifOccurrence **)(uintptr_t)0x1;
    count = 99;
    assert(persistence_list_motif_occurrences_by_motif_id(handle, "none", &list, &count) ==
           PERSISTENCE_MOTIF_OCCURRENCE_OK);
    assert(list == NULL && count == 0);

    for (size_t i = 0; i < 3; i++) {
        motif_occurrence_destroy(items[i]);
    }
    persistence_close(handle);
    remove(path);
}

int main(void) {
    test_motif_round_trip_keeps_event_order();
    test_motif_duplicate_missing_null_and_failed();
    test_a_failed_store_inside_a_callers_transaction();
    test_occurrence_round_trip_and_errors();
    test_list_occurrences_by_motif();
    printf("test_persistence_motif: all tests passed\n");
    return 0;
}
