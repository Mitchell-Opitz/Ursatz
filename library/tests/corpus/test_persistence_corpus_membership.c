#include "corpus/persistence_corpus_membership.h"

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "persistence/persistence.h"

static PersistenceHandle *open_fresh(const char *path) {
    remove(path);
    PersistenceHandle *handle = NULL;
    assert(persistence_open(path, &handle) == PERSISTENCE_OK);
    return handle;
}

static void test_add_list_remove(void) {
    const char *path = "test_persistence_corpus_membership_basic.sqlite";
    PersistenceHandle *handle = open_fresh(path);

    char **refs = NULL;
    size_t count = 999;
    assert(corpus_list_piece_refs(handle, "corpus-1", &refs, &count) == PERSISTENCE_CORPUS_MEMBERSHIP_OK);
    assert(count == 0);
    assert(refs == NULL);

    assert(corpus_add_piece_ref(handle, "corpus-1", "chord-1") == PERSISTENCE_CORPUS_MEMBERSHIP_OK);
    assert(corpus_add_piece_ref(handle, "corpus-1", "chord-2") == PERSISTENCE_CORPUS_MEMBERSHIP_OK);

    assert(corpus_list_piece_refs(handle, "corpus-1", &refs, &count) == PERSISTENCE_CORPUS_MEMBERSHIP_OK);
    assert(count == 2);
    int found_1 = 0;
    int found_2 = 0;
    for (size_t i = 0; i < count; i++) {
        if (strcmp(refs[i], "chord-1") == 0) {
            found_1 = 1;
        }
        if (strcmp(refs[i], "chord-2") == 0) {
            found_2 = 1;
        }
        free(refs[i]);
    }
    free(refs);
    assert(found_1);
    assert(found_2);

    assert(corpus_remove_piece_ref(handle, "corpus-1", "chord-1") == PERSISTENCE_CORPUS_MEMBERSHIP_OK);

    refs = NULL;
    assert(corpus_list_piece_refs(handle, "corpus-1", &refs, &count) == PERSISTENCE_CORPUS_MEMBERSHIP_OK);
    assert(count == 1);
    assert(strcmp(refs[0], "chord-2") == 0);
    free(refs[0]);
    free(refs);

    persistence_close(handle);
    remove(path);
}

static void test_duplicate_add_fails(void) {
    const char *path = "test_persistence_corpus_membership_dup.sqlite";
    PersistenceHandle *handle = open_fresh(path);

    assert(corpus_add_piece_ref(handle, "corpus-1", "chord-1") == PERSISTENCE_CORPUS_MEMBERSHIP_OK);
    assert(corpus_add_piece_ref(handle, "corpus-1", "chord-1") == PERSISTENCE_CORPUS_MEMBERSHIP_ERR_ALREADY_MEMBER);

    persistence_close(handle);
    remove(path);
}

static void test_remove_non_member_fails(void) {
    const char *path = "test_persistence_corpus_membership_nonmember.sqlite";
    PersistenceHandle *handle = open_fresh(path);

    assert(corpus_remove_piece_ref(handle, "corpus-1", "chord-1") == PERSISTENCE_CORPUS_MEMBERSHIP_ERR_NOT_MEMBER);

    persistence_close(handle);
    remove(path);
}

static void test_membership_scoped_per_corpus(void) {
    const char *path = "test_persistence_corpus_membership_scoped.sqlite";
    PersistenceHandle *handle = open_fresh(path);

    assert(corpus_add_piece_ref(handle, "corpus-1", "chord-1") == PERSISTENCE_CORPUS_MEMBERSHIP_OK);
    assert(corpus_add_piece_ref(handle, "corpus-2", "chord-1") == PERSISTENCE_CORPUS_MEMBERSHIP_OK);

    char **refs = NULL;
    size_t count = 0;
    assert(corpus_list_piece_refs(handle, "corpus-1", &refs, &count) == PERSISTENCE_CORPUS_MEMBERSHIP_OK);
    assert(count == 1);
    free(refs[0]);
    free(refs);

    assert(corpus_remove_piece_ref(handle, "corpus-1", "chord-1") == PERSISTENCE_CORPUS_MEMBERSHIP_OK);
    assert(corpus_remove_piece_ref(handle, "corpus-2", "chord-1") == PERSISTENCE_CORPUS_MEMBERSHIP_OK);

    persistence_close(handle);
    remove(path);
}

static void test_null_args_fail(void) {
    const char *path = "test_persistence_corpus_membership_null.sqlite";
    PersistenceHandle *handle = open_fresh(path);

    assert(corpus_add_piece_ref(NULL, "corpus-1", "chord-1") == PERSISTENCE_CORPUS_MEMBERSHIP_ERR_NULL_ARG);
    assert(corpus_add_piece_ref(handle, NULL, "chord-1") == PERSISTENCE_CORPUS_MEMBERSHIP_ERR_NULL_ARG);
    assert(corpus_add_piece_ref(handle, "corpus-1", NULL) == PERSISTENCE_CORPUS_MEMBERSHIP_ERR_NULL_ARG);
    assert(corpus_remove_piece_ref(NULL, "corpus-1", "chord-1") == PERSISTENCE_CORPUS_MEMBERSHIP_ERR_NULL_ARG);
    assert(corpus_list_piece_refs(handle, NULL, NULL, NULL) == PERSISTENCE_CORPUS_MEMBERSHIP_ERR_NULL_ARG);

    persistence_close(handle);
    remove(path);
}

int main(void) {
    test_add_list_remove();
    test_duplicate_add_fails();
    test_remove_non_member_fails();
    test_membership_scoped_per_corpus();
    test_null_args_fail();
    printf("persistence_corpus_membership: all tests passed\n");
    return 0;
}
