#include "structure_test_util.h"

#include "persistence/persistence_relationship.h"

/* RelationshipRepository: round trips, duplicate, missing, NULL arguments, a failed store, and the lists. */

#define SENTINEL ((Relationship *)(uintptr_t)0x1)

static void test_round_trip_every_field(void) {
    const char *path = "test_persistence_relationship_full.sqlite";
    PersistenceHandle *handle = su_open(path);
    /* the source, target, context, evidence and provenance name nothing stored anywhere: no cross-repository keys */
    Relationship *original = su_relationship("rel-1", "ghost-source", "follows", "ghost-target", true);
    assert(persistence_store_relationship(handle, original) == PERSISTENCE_RELATIONSHIP_OK);

    Relationship *loaded = NULL;
    assert(persistence_load_relationship(handle, "rel-1", &loaded) == PERSISTENCE_RELATIONSHIP_OK);
    su_assert_same_relationship(original, loaded);
    assert(relationship_context_id(loaded) != NULL && relationship_evidence_id(loaded) != NULL);
    assert(provenance_reference_state(relationship_provenance(loaded)) == PROVENANCE_REFERENCE_PRESENT);
    relationship_destroy(loaded);

    /* and again after closing and reopening the file */
    persistence_close(handle);
    assert(persistence_open(path, &handle) == PERSISTENCE_OK);
    assert(persistence_load_relationship(handle, "rel-1", &loaded) == PERSISTENCE_RELATIONSHIP_OK);
    su_assert_same_relationship(original, loaded);
    relationship_destroy(loaded);
    relationship_destroy(original);
    persistence_close(handle);
    remove(path);
}

static void test_round_trip_every_optional_field_null(void) {
    const char *path = "test_persistence_relationship_minimal.sqlite";
    PersistenceHandle *handle = su_open(path);
    Relationship *original = su_relationship("rel-1", "a", "contains", "b", false);
    assert(persistence_store_relationship(handle, original) == PERSISTENCE_RELATIONSHIP_OK);
    Relationship *loaded = NULL;
    assert(persistence_load_relationship(handle, "rel-1", &loaded) == PERSISTENCE_RELATIONSHIP_OK);
    su_assert_same_relationship(original, loaded);
    assert(relationship_scope(loaded) == NULL && relationship_context_id(loaded) == NULL);
    assert(relationship_qualifiers(loaded) == NULL && relationship_evidence_id(loaded) == NULL);
    assert(relationship_uncertainty(loaded) == NULL);
    assert(provenance_reference_state(relationship_provenance(loaded)) == PROVENANCE_REFERENCE_ABSENT);
    relationship_destroy(loaded);
    relationship_destroy(original);
    persistence_close(handle);
    remove(path);
}

static void test_duplicate_id(void) {
    const char *path = "test_persistence_relationship_duplicate.sqlite";
    PersistenceHandle *handle = su_open(path);
    Relationship *first = su_relationship("rel-1", "a", "first", "b", false);
    Relationship *second = su_relationship("rel-1", "c", "second", "d", true);
    assert(persistence_store_relationship(handle, first) == PERSISTENCE_RELATIONSHIP_OK);
    assert(persistence_store_relationship(handle, second) == PERSISTENCE_RELATIONSHIP_ERR_DUPLICATE_ID);
    assert(su_count(handle, "relationships") == 1);
    Relationship *loaded = NULL;
    assert(persistence_load_relationship(handle, "rel-1", &loaded) == PERSISTENCE_RELATIONSHIP_OK);
    su_assert_same_relationship(first, loaded); /* not overwritten */
    relationship_destroy(loaded);
    relationship_destroy(first);
    relationship_destroy(second);
    persistence_close(handle);
    remove(path);
}

static void test_missing_id_and_null_arguments(void) {
    const char *path = "test_persistence_relationship_missing.sqlite";
    PersistenceHandle *handle = su_open(path);
    Relationship *out = SENTINEL;
    assert(persistence_load_relationship(handle, "nope", &out) == PERSISTENCE_RELATIONSHIP_ERR_NOT_FOUND);
    assert(out == SENTINEL);

    Relationship *relationship = su_relationship("rel-1", "a", "p", "b", false);
    Relationship **list = NULL;
    size_t count = 0;
    assert(persistence_store_relationship(NULL, relationship) == PERSISTENCE_RELATIONSHIP_ERR_NULL_ARG);
    assert(persistence_store_relationship(handle, NULL) == PERSISTENCE_RELATIONSHIP_ERR_NULL_ARG);
    assert(persistence_load_relationship(NULL, "rel-1", &out) == PERSISTENCE_RELATIONSHIP_ERR_NULL_ARG);
    assert(persistence_load_relationship(handle, NULL, &out) == PERSISTENCE_RELATIONSHIP_ERR_NULL_ARG);
    assert(persistence_load_relationship(handle, "rel-1", NULL) == PERSISTENCE_RELATIONSHIP_ERR_NULL_ARG);
    assert(persistence_list_relationships_by_source(NULL, "a", &list, &count) ==
           PERSISTENCE_RELATIONSHIP_ERR_NULL_ARG);
    assert(persistence_list_relationships_by_source(handle, NULL, &list, &count) ==
           PERSISTENCE_RELATIONSHIP_ERR_NULL_ARG);
    assert(persistence_list_relationships_by_source(handle, "a", NULL, &count) ==
           PERSISTENCE_RELATIONSHIP_ERR_NULL_ARG);
    assert(persistence_list_relationships_by_source(handle, "a", &list, NULL) ==
           PERSISTENCE_RELATIONSHIP_ERR_NULL_ARG);
    assert(persistence_list_relationships_by_target(handle, NULL, &list, &count) ==
           PERSISTENCE_RELATIONSHIP_ERR_NULL_ARG);
    relationship_destroy(relationship);
    persistence_close(handle);
    remove(path);
}

static void test_failed_store_leaves_no_row(void) {
    const char *path = "test_persistence_relationship_failed.sqlite";
    PersistenceHandle *handle = su_open(path);
    su_poison(handle, "relationships", "predicate_name", "poison");
    Relationship *bad = su_relationship("rel-bad", "a", "poison", "b", true);
    assert(persistence_store_relationship(handle, bad) == PERSISTENCE_RELATIONSHIP_ERR_DB);
    assert(su_count(handle, "relationships") == 0);
    Relationship *out = SENTINEL;
    assert(persistence_load_relationship(handle, "rel-bad", &out) == PERSISTENCE_RELATIONSHIP_ERR_NOT_FOUND);

    /* the handle is still usable */
    Relationship *good = su_relationship("rel-good", "a", "fine", "b", false);
    assert(persistence_store_relationship(handle, good) == PERSISTENCE_RELATIONSHIP_OK);
    assert(su_count(handle, "relationships") == 1);
    relationship_destroy(bad);
    relationship_destroy(good);
    persistence_close(handle);
    remove(path);
}

static void assert_ids(Relationship **list, size_t count, const char *const *expected, size_t expected_count) {
    assert(count == expected_count);
    for (size_t i = 0; i < count; i++) {
        assert(strcmp(entity_id_value(relationship_id(list[i])), expected[i]) == 0);
    }
}

static void free_list(Relationship **list, size_t count) {
    for (size_t i = 0; i < count; i++) {
        relationship_destroy(list[i]);
    }
    free(list);
}

static void test_lists_by_source_and_target(void) {
    const char *path = "test_persistence_relationship_lists.sqlite";
    PersistenceHandle *handle = su_open(path);
    /* inserted in this order; ids are not in alphabetical order so insertion order is visible */
    const char *rows[][4] = {{"r-d", "A", "p", "X"}, {"r-b", "A", "p", "Y"}, {"r-c", "B", "p", "X"},
                             {"r-a", "A", "q", "X"}};
    for (size_t i = 0; i < 4; i++) {
        Relationship *relationship = su_relationship(rows[i][0], rows[i][1], rows[i][2], rows[i][3], i % 2 == 0);
        assert(persistence_store_relationship(handle, relationship) == PERSISTENCE_RELATIONSHIP_OK);
        relationship_destroy(relationship);
    }

    Relationship **list = NULL;
    size_t count = 0;
    const char *from_a[] = {"r-d", "r-b", "r-a"};
    assert(persistence_list_relationships_by_source(handle, "A", &list, &count) == PERSISTENCE_RELATIONSHIP_OK);
    assert_ids(list, count, from_a, 3);
    free_list(list, count);

    const char *to_x[] = {"r-d", "r-c", "r-a"};
    assert(persistence_list_relationships_by_target(handle, "X", &list, &count) == PERSISTENCE_RELATIONSHIP_OK);
    assert_ids(list, count, to_x, 3);
    free_list(list, count);

    /* a list entry is the whole Relationship */
    Relationship *expected = su_relationship("r-d", "A", "p", "X", true);
    assert(persistence_list_relationships_by_source(handle, "A", &list, &count) == PERSISTENCE_RELATIONSHIP_OK);
    su_assert_same_relationship(expected, list[0]);
    relationship_destroy(expected);
    free_list(list, count);

    /* no match is OK and empty */
    list = (Relationship **)(uintptr_t)0x1;
    count = 99;
    assert(persistence_list_relationships_by_source(handle, "nobody", &list, &count) == PERSISTENCE_RELATIONSHIP_OK);
    assert(list == NULL && count == 0);
    assert(persistence_list_relationships_by_target(handle, "nobody", &list, &count) == PERSISTENCE_RELATIONSHIP_OK);
    assert(list == NULL && count == 0);

    /* the same order after reopening */
    persistence_close(handle);
    assert(persistence_open(path, &handle) == PERSISTENCE_OK);
    assert(persistence_list_relationships_by_source(handle, "A", &list, &count) == PERSISTENCE_RELATIONSHIP_OK);
    assert_ids(list, count, from_a, 3);
    free_list(list, count);
    persistence_close(handle);
    remove(path);
}

int main(void) {
    test_round_trip_every_field();
    test_round_trip_every_optional_field_null();
    test_duplicate_id();
    test_missing_id_and_null_arguments();
    test_failed_store_leaves_no_row();
    test_lists_by_source_and_target();
    printf("test_persistence_relationship: all tests passed\n");
    return 0;
}
