#include "corpus_service.h"

#include <ctype.h>
#include <stdlib.h>
#include <string.h>

#include <corpus/corpus_api.h>

static CorpusStoreHandle *g_handle = NULL;

bool ursatz_gui_corpus_service_init(const char *db_path) {
    CorpusStoreHandle *handle = NULL;
    if (corpus_api_open(db_path, &handle) != CORPUS_API_OK) {
        return false;
    }
    g_handle = handle;
    return true;
}

void ursatz_gui_corpus_service_shutdown(void) {
    if (g_handle) {
        corpus_api_close(g_handle);
        g_handle = NULL;
    }
}

bool ursatz_gui_corpus_is_valid_id(const char *id) {
    if (!id || id[0] == '\0') {
        return false;
    }
    for (const char *c = id; *c; c++) {
        if (!(isalnum((unsigned char)*c) || *c == '-' || *c == '_')) {
            return false;
        }
    }
    return true;
}

static UrsatzGuiCorpusResult map_result(CorpusApiResult result) {
    switch (result) {
        case CORPUS_API_OK:
            return URSATZ_GUI_CORPUS_OK;
        case CORPUS_API_ERR_INVALID_ID:
            return URSATZ_GUI_CORPUS_ERR_INVALID_ID;
        case CORPUS_API_ERR_INVALID_NAME:
            return URSATZ_GUI_CORPUS_ERR_INVALID_NAME;
        case CORPUS_API_ERR_ALREADY_MEMBER:
            return URSATZ_GUI_CORPUS_ERR_ALREADY_MEMBER;
        case CORPUS_API_ERR_NOT_MEMBER:
            return URSATZ_GUI_CORPUS_ERR_NOT_MEMBER;
        case CORPUS_API_ERR_ALLOC:
            return URSATZ_GUI_CORPUS_ERR_ALLOC;
        case CORPUS_API_ERR_NULL_ARG:
        case CORPUS_API_ERR_OPEN_FAILED:
        case CORPUS_API_ERR_DB:
        default:
            return URSATZ_GUI_CORPUS_ERR_FAILED;
    }
}

UrsatzGuiCorpusResult ursatz_gui_corpus_create(const char *id, const char *name) {
    if (!g_handle) {
        return URSATZ_GUI_CORPUS_ERR_NOT_INITIALIZED;
    }
    return map_result(corpus_api_create_corpus(g_handle, id, name));
}

UrsatzGuiCorpusResult ursatz_gui_corpus_list(UrsatzGuiCorpusEntry **out_entries, size_t *out_count) {
    if (!g_handle) {
        return URSATZ_GUI_CORPUS_ERR_NOT_INITIALIZED;
    }

    CorpusApiEntry *entries = NULL;
    size_t count = 0;
    CorpusApiResult result = corpus_api_list_corpora(g_handle, &entries, &count);
    if (result != CORPUS_API_OK) {
        return map_result(result);
    }

    if (count == 0) {
        *out_entries = NULL;
        *out_count = 0;
        corpus_api_free_corpus_list(entries, count);
        return URSATZ_GUI_CORPUS_OK;
    }

    UrsatzGuiCorpusEntry *out = malloc(count * sizeof(*out));
    if (!out) {
        corpus_api_free_corpus_list(entries, count);
        return URSATZ_GUI_CORPUS_ERR_ALLOC;
    }

    for (size_t i = 0; i < count; i++) {
        out[i].id = malloc(strlen(entries[i].id) + 1);
        out[i].name = malloc(strlen(entries[i].name) + 1);
        strcpy(out[i].id, entries[i].id);
        strcpy(out[i].name, entries[i].name);
    }
    corpus_api_free_corpus_list(entries, count);

    *out_entries = out;
    *out_count = count;
    return URSATZ_GUI_CORPUS_OK;
}

void ursatz_gui_corpus_free_entries(UrsatzGuiCorpusEntry *entries, size_t count) {
    if (!entries) {
        return;
    }
    for (size_t i = 0; i < count; i++) {
        free(entries[i].id);
        free(entries[i].name);
    }
    free(entries);
}

UrsatzGuiCorpusResult ursatz_gui_corpus_add_piece(const char *corpus_id, const char *piece_id) {
    if (!g_handle) {
        return URSATZ_GUI_CORPUS_ERR_NOT_INITIALIZED;
    }
    return map_result(corpus_api_add_piece_ref(g_handle, corpus_id, piece_id));
}

UrsatzGuiCorpusResult ursatz_gui_corpus_remove_piece(const char *corpus_id, const char *piece_id) {
    if (!g_handle) {
        return URSATZ_GUI_CORPUS_ERR_NOT_INITIALIZED;
    }
    return map_result(corpus_api_remove_piece_ref(g_handle, corpus_id, piece_id));
}

UrsatzGuiCorpusResult ursatz_gui_corpus_list_pieces(const char *corpus_id, char ***out_ids, size_t *out_count) {
    if (!g_handle) {
        return URSATZ_GUI_CORPUS_ERR_NOT_INITIALIZED;
    }
    return map_result(corpus_api_list_piece_refs(g_handle, corpus_id, out_ids, out_count));
}
