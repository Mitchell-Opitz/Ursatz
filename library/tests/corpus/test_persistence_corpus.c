#include "corpus/persistence_corpus.h"

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "identity/entity_id.h"
#include "persistence/persistence.h"

static Corpus *make_corpus(const char *id_value, const char *name) {
    EntityID *id = NULL;
    assert(entity_id_create(id_value, &id) == ENTITY_ID_OK);
    Corpus *corpus = NULL;
    assert(corpus_create(id, name, &corpus) == CORPUS_OK);
    return corpus;
}

static void test_store_and_load_round_trips(void) {
    const char *path = "test_persistence_corpus_roundtrip.sqlite";
    remove(path);

    PersistenceHandle *handle = NULL;
    assert(persistence_open(path, &handle) == PERSISTENCE_OK);

    Corpus *corpus = make_corpus("corpus-1", "Chorales");
    assert(persistence_store_corpus(handle, corpus) == PERSISTENCE_CORPUS_OK);
    corpus_destroy(corpus);

    Corpus *loaded = NULL;
    assert(persistence_load_corpus(handle, "corpus-1", &loaded) == PERSISTENCE_CORPUS_OK);
    assert(strcmp(entity_id_value(corpus_id(loaded)), "corpus-1") == 0);
    assert(strcmp(corpus_name(loaded), "Chorales") == 0);
    corpus_destroy(loaded);

    persistence_close(handle);
    remove(path);
}

static void test_load_missing_fails(void) {
    const char *path = "test_persistence_corpus_missing.sqlite";
    remove(path);

    PersistenceHandle *handle = NULL;
    assert(persistence_open(path, &handle) == PERSISTENCE_OK);

    Corpus *loaded = NULL;
    assert(persistence_load_corpus(handle, "does-not-exist", &loaded) == PERSISTENCE_CORPUS_ERR_DB);
    assert(loaded == NULL);

    persistence_close(handle);
    remove(path);
}

static void test_list_corpora(void) {
    const char *path = "test_persistence_corpus_list.sqlite";
    remove(path);

    PersistenceHandle *handle = NULL;
    assert(persistence_open(path, &handle) == PERSISTENCE_OK);

    Corpus **empty = NULL;
    size_t empty_count = 999;
    assert(persistence_list_corpora(handle, &empty, &empty_count) == PERSISTENCE_CORPUS_OK);
    assert(empty_count == 0);
    assert(empty == NULL);

    Corpus *a = make_corpus("corpus-a", "A");
    Corpus *b = make_corpus("corpus-b", "B");
    assert(persistence_store_corpus(handle, a) == PERSISTENCE_CORPUS_OK);
    assert(persistence_store_corpus(handle, b) == PERSISTENCE_CORPUS_OK);
    corpus_destroy(a);
    corpus_destroy(b);

    Corpus **corpora = NULL;
    size_t count = 0;
    assert(persistence_list_corpora(handle, &corpora, &count) == PERSISTENCE_CORPUS_OK);
    assert(count == 2);

    int found_a = 0;
    int found_b = 0;
    for (size_t i = 0; i < count; i++) {
        if (strcmp(entity_id_value(corpus_id(corpora[i])), "corpus-a") == 0) {
            found_a = 1;
        }
        if (strcmp(entity_id_value(corpus_id(corpora[i])), "corpus-b") == 0) {
            found_b = 1;
        }
        corpus_destroy(corpora[i]);
    }
    free(corpora);
    assert(found_a);
    assert(found_b);

    persistence_close(handle);
    remove(path);
}

static void test_null_args_fail(void) {
    Corpus *corpus = make_corpus("corpus-null", "N");
    assert(persistence_store_corpus(NULL, corpus) == PERSISTENCE_CORPUS_ERR_NULL_ARG);
    assert(persistence_store_corpus((PersistenceHandle *)1, NULL) == PERSISTENCE_CORPUS_ERR_NULL_ARG);
    corpus_destroy(corpus);
}

int main(void) {
    test_store_and_load_round_trips();
    test_load_missing_fails();
    test_list_corpora();
    test_null_args_fail();
    printf("persistence_corpus: all tests passed\n");
    return 0;
}
