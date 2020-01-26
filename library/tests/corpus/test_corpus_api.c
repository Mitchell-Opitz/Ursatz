#include "corpus/corpus_api.h"

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static CorpusStoreHandle *open_fresh(const char *path) {
    remove(path);
    CorpusStoreHandle *handle = NULL;
    assert(corpus_api_open(path, &handle) == CORPUS_API_OK);
    return handle;
}

static void test_end_to_end(void) {
    const char *path = "test_corpus_api_end_to_end.sqlite";
    CorpusStoreHandle *handle = open_fresh(path);

    assert(corpus_api_create_corpus(handle, "corpus-1", "Chorales") == CORPUS_API_OK);
    assert(corpus_api_create_corpus(handle, "corpus-2", "Fugues") == CORPUS_API_OK);

    CorpusApiEntry *entries = NULL;
    size_t entry_count = 0;
    assert(corpus_api_list_corpora(handle, &entries, &entry_count) == CORPUS_API_OK);
    assert(entry_count == 2);
    int found_1 = 0;
    int found_2 = 0;
    for (size_t i = 0; i < entry_count; i++) {
        if (strcmp(entries[i].id, "corpus-1") == 0) {
            assert(strcmp(entries[i].name, "Chorales") == 0);
            found_1 = 1;
        }
        if (strcmp(entries[i].id, "corpus-2") == 0) {
            assert(strcmp(entries[i].name, "Fugues") == 0);
            found_2 = 1;
        }
    }
    assert(found_1);
    assert(found_2);
    corpus_api_free_corpus_list(entries, entry_count);

    assert(corpus_api_add_piece_ref(handle, "corpus-1", "chord-1") == CORPUS_API_OK);
    assert(corpus_api_add_piece_ref(handle, "corpus-1", "chord-2") == CORPUS_API_OK);

    char **refs = NULL;
    size_t ref_count = 0;
    assert(corpus_api_list_piece_refs(handle, "corpus-1", &refs, &ref_count) == CORPUS_API_OK);
    assert(ref_count == 2);
    for (size_t i = 0; i < ref_count; i++) {
        free(refs[i]);
    }
    free(refs);

    assert(corpus_api_remove_piece_ref(handle, "corpus-1", "chord-1") == CORPUS_API_OK);

    refs = NULL;
    ref_count = 0;
    assert(corpus_api_list_piece_refs(handle, "corpus-1", &refs, &ref_count) == CORPUS_API_OK);
    assert(ref_count == 1);
    assert(strcmp(refs[0], "chord-2") == 0);
    free(refs[0]);
    free(refs);

    corpus_api_close(handle);
    remove(path);
}

static void test_duplicate_add_fails(void) {
    const char *path = "test_corpus_api_dup.sqlite";
    CorpusStoreHandle *handle = open_fresh(path);

    assert(corpus_api_add_piece_ref(handle, "corpus-1", "chord-1") == CORPUS_API_OK);
    assert(corpus_api_add_piece_ref(handle, "corpus-1", "chord-1") == CORPUS_API_ERR_ALREADY_MEMBER);

    corpus_api_close(handle);
    remove(path);
}

static void test_remove_non_member_fails(void) {
    const char *path = "test_corpus_api_nonmember.sqlite";
    CorpusStoreHandle *handle = open_fresh(path);

    assert(corpus_api_remove_piece_ref(handle, "corpus-1", "chord-1") == CORPUS_API_ERR_NOT_MEMBER);

    corpus_api_close(handle);
    remove(path);
}

static void test_open_bad_path_fails(void) {
    CorpusStoreHandle *handle = NULL;
    assert(corpus_api_open("/nonexistent-dir/does-not-exist.sqlite", &handle) == CORPUS_API_ERR_OPEN_FAILED);
    assert(handle == NULL);
}

static void test_close_null_is_safe(void) {
    corpus_api_close(NULL);
}

int main(void) {
    test_end_to_end();
    test_duplicate_add_fails();
    test_remove_non_member_fails();
    test_open_bad_path_fails();
    test_close_null_is_safe();
    printf("corpus_api: all tests passed\n");
    return 0;
}
