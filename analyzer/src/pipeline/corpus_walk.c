#include "pipeline/corpus_walk.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <cJSON.h>

#include "util/file_util.h"
#include "util/json_util.h"
#include "util/misc_util.h"

CorpusWalkResult corpus_walk_documents(const char *index_path, CorpusDocumentVisitor visit, void *context,
                                       char **input_dir) {
    if (input_dir) {
        *input_dir = NULL;
    }
    char *index_text = NULL;
    file_read_all(index_path, &index_text, NULL);
    if (!index_text) {
        return CORPUS_WALK_ERR_IO;
    }
    cJSON *index = cJSON_Parse(index_text);
    free(index_text);
    const char *schema = json_string_member(index, "schema");
    const char *input = json_string_member(index, "input_dir");
    const cJSON *pieces = json_member(index, "pieces");
    if (!schema || strcmp(schema, "ursatz-corpus-index") != 0 || (input_dir && !input) || !cJSON_IsArray(pieces)) {
        cJSON_Delete(index);
        return CORPUS_WALK_ERR_FORMAT;
    }

    const char *slash = strrchr(index_path, '/');
    size_t directory = slash ? (size_t)(slash - index_path) : 0;
    CorpusWalkResult result = CORPUS_WALK_OK;
    const cJSON *piece;
    cJSON_ArrayForEach(piece, pieces) {
        const char *status = json_string_member(piece, "status");
        const char *document = json_string_member(piece, "document");
        const char *path = json_string_member(piece, "path");
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
        file_read_all(file, &json, NULL);
        free(file);
        if (!json) {
            result = CORPUS_WALK_ERR_IO;
            break;
        }
        bool keep_going = visit(path, document, json, context);
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
