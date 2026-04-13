#include "structure_test_util.h"

#include "persistence/persistence_cadence.h"
#include "persistence/persistence_phrase_occurrence.h"

/* Cadences and PhraseOccurrences: round trips, event id order, duplicate, missing, NULL arguments, failed stores, lists. */

static const char *const EVENTS[] = {"ev-7", "ev-2", "ev-4", "ev-2"};

static void test_cadence_round_trip_with_and_without_context(void) {
    const char *path = "test_persistence_cadence_round_trip.sqlite";
    PersistenceHandle *handle = su_open(path);
    Cadence *with_context = su_cadence("cad-1", EVENTS, 4, true);
    Cadence *without_context = su_cadence("cad-2", EVENTS, 2, false);
    assert(persistence_store_cadence(handle, with_context) == PERSISTENCE_CADENCE_OK);
    assert(persistence_store_cadence(handle, without_context) == PERSISTENCE_CADENCE_OK);

    Cadence *loaded = NULL;
    assert(persistence_load_cadence(handle, "cad-1", &loaded) == PERSISTENCE_CADENCE_OK);
    su_assert_same_cadence(with_context, loaded);
    assert(cadence_optional_harmonic_context(loaded) != NULL);
    assert(strcmp(entity_id_value(cadence_event_id_at(loaded, 0)), "ev-7") == 0);
    cadence_destroy(loaded);

    assert(persistence_load_cadence(handle, "cad-2", &loaded) == PERSISTENCE_CADENCE_OK);
    su_assert_same_cadence(without_context, loaded);
    assert(cadence_optional_harmonic_context(loaded) == NULL); /* NULL round trips as NULL */
    cadence_destroy(loaded);

    persistence_close(handle);
    assert(persistence_open(path, &handle) == PERSISTENCE_OK);
    assert(persistence_load_cadence(handle, "cad-1", &loaded) == PERSISTENCE_CADENCE_OK);
    su_assert_same_cadence(with_context, loaded);
    cadence_destroy(loaded);

    cadence_destroy(with_context);
    cadence_destroy(without_context);
    persistence_close(handle);
    remove(path);
}

static void test_cadence_duplicate_missing_null_and_failed(void) {
    const char *path = "test_persistence_cadence_errors.sqlite";
    PersistenceHandle *handle = su_open(path);
    Cadence *first = su_cadence("cad-1", EVENTS, 2, true);
    Cadence *second = su_cadence("cad-1", EVENTS, 4, false);
    assert(persistence_store_cadence(handle, first) == PERSISTENCE_CADENCE_OK);
    assert(persistence_store_cadence(handle, second) == PERSISTENCE_CADENCE_ERR_DUPLICATE_ID);
    assert(su_count(handle, "cadences") == 1 && su_count(handle, "cadence_events") == 2);
    Cadence *loaded = NULL;
    assert(persistence_load_cadence(handle, "cad-1", &loaded) == PERSISTENCE_CADENCE_OK);
    su_assert_same_cadence(first, loaded);
    cadence_destroy(loaded);

    Cadence *out = (Cadence *)(uintptr_t)0x1;
    assert(persistence_load_cadence(handle, "nope", &out) == PERSISTENCE_CADENCE_ERR_NOT_FOUND);
    assert(out == (Cadence *)(uintptr_t)0x1);
    assert(persistence_store_cadence(NULL, first) == PERSISTENCE_CADENCE_ERR_NULL_ARG);
    assert(persistence_store_cadence(handle, NULL) == PERSISTENCE_CADENCE_ERR_NULL_ARG);
    assert(persistence_load_cadence(NULL, "cad-1", &out) == PERSISTENCE_CADENCE_ERR_NULL_ARG);
    assert(persistence_load_cadence(handle, NULL, &out) == PERSISTENCE_CADENCE_ERR_NULL_ARG);
    assert(persistence_load_cadence(handle, "cad-1", NULL) == PERSISTENCE_CADENCE_ERR_NULL_ARG);

    su_poison(handle, "cadence_events", "event_id", "poison");
    const char *poisoned[] = {"x", "poison", "z"};
    Cadence *bad = su_cadence("cad-bad", poisoned, 3, true);
    assert(persistence_store_cadence(handle, bad) == PERSISTENCE_CADENCE_ERR_DB);
    assert(su_count(handle, "cadences") == 1 && su_count(handle, "cadence_events") == 2);
    assert(persistence_load_cadence(handle, "cad-bad", &out) == PERSISTENCE_CADENCE_ERR_NOT_FOUND);

    cadence_destroy(first);
    cadence_destroy(second);
    cadence_destroy(bad);
    persistence_close(handle);
    remove(path);
}

static void test_phrase_occurrence_round_trip_and_errors(void) {
    const char *path = "test_persistence_phrase_occurrence.sqlite";
    PersistenceHandle *handle = su_open(path);

    /* the phrase is never stored: an occurrence names it by id only */
    PhraseOccurrence *original = su_phrase_occurrence("pocc-1", "phrase-not-stored", EVENTS, 4);
    assert(persistence_store_phrase_occurrence(handle, original) == PERSISTENCE_PHRASE_OCCURRENCE_OK);
    PhraseOccurrence *loaded = NULL;
    assert(persistence_load_phrase_occurrence(handle, "pocc-1", &loaded) == PERSISTENCE_PHRASE_OCCURRENCE_OK);
    su_assert_same_phrase_occurrence(original, loaded);
    assert(strcmp(entity_id_value(phrase_occurrence_phrase_id(loaded)), "phrase-not-stored") == 0);
    phrase_occurrence_destroy(loaded);

    const char *other[] = {"p"};
    PhraseOccurrence *duplicate = su_phrase_occurrence("pocc-1", "another", other, 1);
    assert(persistence_store_phrase_occurrence(handle, duplicate) == PERSISTENCE_PHRASE_OCCURRENCE_ERR_DUPLICATE_ID);
    assert(su_count(handle, "phrase_occurrences") == 1 && su_count(handle, "phrase_occurrence_events") == 4);
    assert(persistence_load_phrase_occurrence(handle, "pocc-1", &loaded) == PERSISTENCE_PHRASE_OCCURRENCE_OK);
    su_assert_same_phrase_occurrence(original, loaded);
    phrase_occurrence_destroy(loaded);

    PhraseOccurrence *out = (PhraseOccurrence *)(uintptr_t)0x1;
    assert(persistence_load_phrase_occurrence(handle, "nope", &out) == PERSISTENCE_PHRASE_OCCURRENCE_ERR_NOT_FOUND);
    assert(out == (PhraseOccurrence *)(uintptr_t)0x1);
    assert(persistence_store_phrase_occurrence(NULL, original) == PERSISTENCE_PHRASE_OCCURRENCE_ERR_NULL_ARG);
    assert(persistence_store_phrase_occurrence(handle, NULL) == PERSISTENCE_PHRASE_OCCURRENCE_ERR_NULL_ARG);
    assert(persistence_load_phrase_occurrence(handle, NULL, &out) == PERSISTENCE_PHRASE_OCCURRENCE_ERR_NULL_ARG);
    assert(persistence_load_phrase_occurrence(handle, "pocc-1", NULL) == PERSISTENCE_PHRASE_OCCURRENCE_ERR_NULL_ARG);
    PhraseOccurrence **list = NULL;
    size_t count = 0;
    assert(persistence_list_phrase_occurrences_by_phrase_id(handle, NULL, &list, &count) ==
           PERSISTENCE_PHRASE_OCCURRENCE_ERR_NULL_ARG);

    su_poison(handle, "phrase_occurrence_events", "event_id", "poison");
    const char *poisoned[] = {"x", "poison"};
    PhraseOccurrence *bad = su_phrase_occurrence("pocc-bad", "ph", poisoned, 2);
    assert(persistence_store_phrase_occurrence(handle, bad) == PERSISTENCE_PHRASE_OCCURRENCE_ERR_DB);
    assert(su_count(handle, "phrase_occurrences") == 1 && su_count(handle, "phrase_occurrence_events") == 4);

    phrase_occurrence_destroy(original);
    phrase_occurrence_destroy(duplicate);
    phrase_occurrence_destroy(bad);
    persistence_close(handle);
    remove(path);
}

static void test_list_phrase_occurrences_by_phrase(void) {
    const char *path = "test_persistence_phrase_occurrence_list.sqlite";
    PersistenceHandle *handle = su_open(path);
    const char *a[] = {"e1", "e2"};
    const char *b[] = {"e3"};
    const char *c[] = {"e9", "e4"};
    PhraseOccurrence *items[] = {su_phrase_occurrence("pocc-z", "PH1", a, 2), su_phrase_occurrence("pocc-m", "PH2", b, 1),
                                 su_phrase_occurrence("pocc-a", "PH1", c, 2)};
    for (size_t i = 0; i < 3; i++) {
        assert(persistence_store_phrase_occurrence(handle, items[i]) == PERSISTENCE_PHRASE_OCCURRENCE_OK);
    }
    PhraseOccurrence **list = NULL;
    size_t count = 0;
    assert(persistence_list_phrase_occurrences_by_phrase_id(handle, "PH1", &list, &count) ==
           PERSISTENCE_PHRASE_OCCURRENCE_OK);
    assert(count == 2); /* insertion order, not id order */
    su_assert_same_phrase_occurrence(items[0], list[0]);
    su_assert_same_phrase_occurrence(items[2], list[1]);
    for (size_t i = 0; i < count; i++) {
        phrase_occurrence_destroy(list[i]);
    }
    free(list);

    list = (PhraseOccurrence **)(uintptr_t)0x1;
    count = 99;
    assert(persistence_list_phrase_occurrences_by_phrase_id(handle, "none", &list, &count) ==
           PERSISTENCE_PHRASE_OCCURRENCE_OK);
    assert(list == NULL && count == 0);

    for (size_t i = 0; i < 3; i++) {
        phrase_occurrence_destroy(items[i]);
    }
    persistence_close(handle);
    remove(path);
}

int main(void) {
    test_cadence_round_trip_with_and_without_context();
    test_cadence_duplicate_missing_null_and_failed();
    test_phrase_occurrence_round_trip_and_errors();
    test_list_phrase_occurrences_by_phrase();
    printf("test_persistence_cadence_phrase: all tests passed\n");
    return 0;
}
