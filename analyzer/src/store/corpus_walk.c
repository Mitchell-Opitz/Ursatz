#include "store/corpus_walk.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <cJSON.h>

#include <common/files.h>
#include <common/json.h>
#include "util/misc_util.h"

CorpusWalkResult corpus_walk_documents(const char *index_path, CorpusDocumentVisitor visit, void *context,
                                       char **input_dir) {
    if (input_dir) {
        *input_dir = NULL;
    }
    char *index_text = NULL;
    common_file_read(index_path, &index_text, NULL, NULL);
    if (!index_text) {
        return CORPUS_WALK_ERR_IO;
    }
    cJSON *index = cJSON_Parse(index_text);
    free(index_text);
    const char *schema = common_json_string_or_null(index, "schema");
    const char *input = common_json_string_or_null(index, "input_dir");
    const cJSON *pieces = common_json_member(index, "pieces");
    if (!schema || strcmp(schema, "ursatz-corpus-index") != 0 || (input_dir && !input) || !cJSON_IsArray(pieces)) {
        cJSON_Delete(index);
        return CORPUS_WALK_ERR_FORMAT;
    }

    const char *slash = strrchr(index_path, '/');
    size_t directory = slash ? (size_t)(slash - index_path) : 0;
    CorpusWalkResult result = CORPUS_WALK_OK;
    const cJSON *piece;
    cJSON_ArrayForEach(piece, pieces) {
        const char *status = common_json_string_or_null(piece, "status");
        const char *document = common_json_string_or_null(piece, "document");
        const char *path = common_json_string_or_null(piece, "path");
        if (!status || strcmp(status, "analyzed") != 0 || !document || !path) {
            continue;
        }
        char *file = malloc(directory + strlen(document) + 2);
        if (!file) {
            result = CORPUS_WALK_ERR_ALLOC;
            break;
        }
        if (slash) {
            sprintf(file, "%.*s/%s", (int)directory, index_path, document);
        } else {
            strcpy(file, document);
        }
        char *json = NULL;
        common_file_read(file, &json, NULL, NULL);
        free(file);
        if (!json) {
            result = CORPUS_WALK_ERR_IO;
            break;
        }
        bool keep_going = visit(path, document, json, analyzer_document_status(json), context);
        free(json);
        if (!keep_going) {
            result = CORPUS_WALK_ERR_VISIT;
            break;
        }
    }
    if (result == CORPUS_WALK_OK && input_dir) {
        *input_dir = text_copy(input);
        if (!*input_dir) {
            result = CORPUS_WALK_ERR_ALLOC;
        }
    }
    cJSON_Delete(index);
    return result;
}

static bool count_stale_document(const char *piece_path, const char *document, const char *document_json,
                                 AnalyzerDocumentStatus status, void *context) {
    (void)piece_path;
    (void)document;
    (void)document_json;
    if (status != ANALYZER_DOCUMENT_CURRENT) {
        ++*(size_t *)context;
    }
    return true;
}

UrsatzStatus analyzer_corpus_stale_count(const char *index_path, size_t *stale) {
    *stale = 0;
    switch (corpus_walk_documents(index_path, count_stale_document, stale, NULL)) {
    case CORPUS_WALK_OK:
        return URSATZ_OK;
    case CORPUS_WALK_ERR_ALLOC:
        return URSATZ_ERR_ALLOC;
    case CORPUS_WALK_ERR_FORMAT:
        return URSATZ_ERR_FORMAT;
    default:
        return URSATZ_ERR_IO;
    }
}
