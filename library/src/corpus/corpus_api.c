#include "corpus/corpus_api.h"

#include <stdlib.h>
#include <string.h>

#include "corpus/corpus.h"
#include "corpus/persistence_corpus.h"
#include "corpus/persistence_corpus_membership.h"
#include "identity/entity_id.h"
#include "persistence/persistence.h"

struct CorpusStoreHandle {
    PersistenceHandle *persistence;
};

CorpusApiResult corpus_api_open(const char *path, CorpusStoreHandle **out) {
    if (path == NULL || out == NULL) {
        return CORPUS_API_ERR_NULL_ARG;
    }

    PersistenceHandle *persistence = NULL;
    if (persistence_open(path, &persistence) != PERSISTENCE_OK) {
        return CORPUS_API_ERR_OPEN_FAILED;
    }

    CorpusStoreHandle *handle = malloc(sizeof(*handle));
    if (handle == NULL) {
        persistence_close(persistence);
        return CORPUS_API_ERR_ALLOC;
    }

    handle->persistence = persistence;
    *out = handle;
    return CORPUS_API_OK;
}

void corpus_api_close(CorpusStoreHandle *handle) {
    if (handle == NULL) {
        return;
    }
    persistence_close(handle->persistence);
    free(handle);
}

CorpusApiResult corpus_api_create_corpus(CorpusStoreHandle *handle, const char *id, const char *name) {
    if (handle == NULL || id == NULL || name == NULL) {
        return CORPUS_API_ERR_NULL_ARG;
    }

    EntityID *entity_id = NULL;
    if (entity_id_create(id, &entity_id) != ENTITY_ID_OK) {
        return CORPUS_API_ERR_INVALID_ID;
    }

    Corpus *corpus = NULL;
    CorpusResult create_result = corpus_create(entity_id, name, &corpus);
    if (create_result != CORPUS_OK) {
        entity_id_destroy(entity_id);
        if (create_result == CORPUS_ERR_NULL_NAME) {
            return CORPUS_API_ERR_INVALID_NAME;
        }
        if (create_result == CORPUS_ERR_ALLOC) {
            return CORPUS_API_ERR_ALLOC;
        }
        return CORPUS_API_ERR_INVALID_ID;
    }

    PersistenceCorpusResult store_result = persistence_store_corpus(handle->persistence, corpus);
    corpus_destroy(corpus);

    if (store_result == PERSISTENCE_CORPUS_ERR_ALLOC) {
        return CORPUS_API_ERR_ALLOC;
    }
    if (store_result != PERSISTENCE_CORPUS_OK) {
        return CORPUS_API_ERR_DB;
    }

    return CORPUS_API_OK;
}

CorpusApiResult corpus_api_list_corpora(CorpusStoreHandle *handle, CorpusApiEntry **out_entries,
                                         size_t *out_count) {
    if (handle == NULL || out_entries == NULL || out_count == NULL) {
        return CORPUS_API_ERR_NULL_ARG;
    }

    Corpus **corpora = NULL;
    size_t count = 0;
    PersistenceCorpusResult list_result = persistence_list_corpora(handle->persistence, &corpora, &count);
    if (list_result == PERSISTENCE_CORPUS_ERR_ALLOC) {
        return CORPUS_API_ERR_ALLOC;
    }
    if (list_result != PERSISTENCE_CORPUS_OK) {
        return CORPUS_API_ERR_DB;
    }

    if (count == 0) {
        *out_entries = NULL;
        *out_count = 0;
        return CORPUS_API_OK;
    }

    CorpusApiEntry *entries = calloc(count, sizeof(*entries));
    if (entries == NULL) {
        for (size_t i = 0; i < count; i++) {
            corpus_destroy(corpora[i]);
        }
        free(corpora);
        return CORPUS_API_ERR_ALLOC;
    }

    for (size_t i = 0; i < count; i++) {
        entries[i].id = strdup(entity_id_value(corpus_id(corpora[i])));
        entries[i].name = strdup(corpus_name(corpora[i]));
        corpus_destroy(corpora[i]);

        if (entries[i].id == NULL || entries[i].name == NULL) {
            corpus_api_free_corpus_list(entries, count);
            free(corpora);
            return CORPUS_API_ERR_ALLOC;
        }
    }
    free(corpora);

    *out_entries = entries;
    *out_count = count;
    return CORPUS_API_OK;
}

void corpus_api_free_corpus_list(CorpusApiEntry *entries, size_t count) {
    if (entries == NULL) {
        return;
    }
    for (size_t i = 0; i < count; i++) {
        free(entries[i].id);
        free(entries[i].name);
    }
    free(entries);
}

static CorpusApiResult map_membership_result(PersistenceCorpusMembershipResult result) {
    switch (result) {
        case PERSISTENCE_CORPUS_MEMBERSHIP_OK:
            return CORPUS_API_OK;
        case PERSISTENCE_CORPUS_MEMBERSHIP_ERR_NULL_ARG:
            return CORPUS_API_ERR_NULL_ARG;
        case PERSISTENCE_CORPUS_MEMBERSHIP_ERR_ALLOC:
            return CORPUS_API_ERR_ALLOC;
        case PERSISTENCE_CORPUS_MEMBERSHIP_ERR_ALREADY_MEMBER:
            return CORPUS_API_ERR_ALREADY_MEMBER;
        case PERSISTENCE_CORPUS_MEMBERSHIP_ERR_NOT_MEMBER:
            return CORPUS_API_ERR_NOT_MEMBER;
        case PERSISTENCE_CORPUS_MEMBERSHIP_ERR_DB:
        default:
            return CORPUS_API_ERR_DB;
    }
}

CorpusApiResult corpus_api_add_piece_ref(CorpusStoreHandle *handle, const char *corpus_id, const char *piece_ref) {
    if (handle == NULL) {
        return CORPUS_API_ERR_NULL_ARG;
    }
    return map_membership_result(corpus_add_piece_ref(handle->persistence, corpus_id, piece_ref));
}

CorpusApiResult corpus_api_remove_piece_ref(CorpusStoreHandle *handle, const char *corpus_id,
                                             const char *piece_ref) {
    if (handle == NULL) {
        return CORPUS_API_ERR_NULL_ARG;
    }
    return map_membership_result(corpus_remove_piece_ref(handle->persistence, corpus_id, piece_ref));
}

CorpusApiResult corpus_api_list_piece_refs(CorpusStoreHandle *handle, const char *corpus_id, char ***out_refs,
                                            size_t *out_count) {
    if (handle == NULL) {
        return CORPUS_API_ERR_NULL_ARG;
    }
    return map_membership_result(corpus_list_piece_refs(handle->persistence, corpus_id, out_refs, out_count));
}
