#include "corpus/corpus.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "identity/entity_id.h"

static void test_create_succeeds(void) {
    EntityID *id = NULL;
    assert(entity_id_create("corpus-1", &id) == ENTITY_ID_OK);

    Corpus *corpus = NULL;
    assert(corpus_create(id, "Chorales", &corpus) == CORPUS_OK);
    assert(corpus != NULL);

    assert(strcmp(entity_id_value(corpus_id(corpus)), "corpus-1") == 0);
    assert(strcmp(corpus_name(corpus), "Chorales") == 0);

    corpus_destroy(corpus);
}

static void test_create_null_id_fails(void) {
    Corpus *corpus = NULL;
    assert(corpus_create(NULL, "Chorales", &corpus) == CORPUS_ERR_NULL_ID);
    assert(corpus == NULL);
}

static void test_create_null_name_fails(void) {
    EntityID *id = NULL;
    assert(entity_id_create("corpus-2", &id) == ENTITY_ID_OK);

    Corpus *corpus = NULL;
    assert(corpus_create(id, NULL, &corpus) == CORPUS_ERR_NULL_NAME);
    assert(corpus == NULL);

    entity_id_destroy(id);
}

static void test_destroy_null_is_safe(void) {
    corpus_destroy(NULL);
}

int main(void) {
    test_create_succeeds();
    test_create_null_id_fails();
    test_create_null_name_fails();
    test_destroy_null_is_safe();
    printf("corpus: all tests passed\n");
    return 0;
}
