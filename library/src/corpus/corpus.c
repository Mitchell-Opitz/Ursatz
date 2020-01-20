#include "corpus/corpus.h"

#include <stdlib.h>
#include <string.h>

struct Corpus {
    EntityID *id;
    char *name;
};

static char *copy_string(const char *value) {
    size_t len = strlen(value);
    char *copy = (char *)malloc(len + 1);
    if (copy != NULL) {
        memcpy(copy, value, len + 1);
    }
    return copy;
}

CorpusResult corpus_create(EntityID *id, const char *name, Corpus **out) {
    if (id == NULL) {
        return CORPUS_ERR_NULL_ID;
    }
    if (name == NULL) {
        return CORPUS_ERR_NULL_NAME;
    }

    Corpus *corpus = (Corpus *)malloc(sizeof(Corpus));
    if (corpus == NULL) {
        return CORPUS_ERR_ALLOC;
    }

    char *copied_name = copy_string(name);
    if (copied_name == NULL) {
        free(corpus);
        return CORPUS_ERR_ALLOC;
    }

    corpus->id = id;
    corpus->name = copied_name;

    *out = corpus;
    return CORPUS_OK;
}

void corpus_destroy(Corpus *corpus) {
    if (corpus == NULL) {
        return;
    }
    entity_id_destroy(corpus->id);
    free(corpus->name);
    free(corpus);
}

const EntityID *corpus_id(const Corpus *corpus) {
    return corpus->id;
}

const char *corpus_name(const Corpus *corpus) {
    return corpus->name;
}
