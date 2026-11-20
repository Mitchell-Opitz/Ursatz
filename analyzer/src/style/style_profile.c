#include <ursatz_analyzer_internal/style_profile.h>

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <cJSON.h>

#include "store/corpus_walk.h"
#include "style/style_profile_internal.h"

/* Counts one document tree (schema version 13) into the profile. */
static StyleProfileResult add_document_tree(StyleProfile *profile, const cJSON *document) {
    StylePiece piece = {0};
    StyleProfileResult result = style_piece_from_document(document, &piece);
    if (result == STYLE_PROFILE_OK) {
        result = style_profile_add_piece(profile, &piece);
    }
    style_piece_destroy(&piece);
    return result;
}

StyleProfileResult analyzer_style_add_document_json(StyleProfile *profile, const char *json) {
    if (!profile || !json) {
        return STYLE_PROFILE_ERR_ARGUMENT;
    }
    cJSON *document = cJSON_Parse(json);
    StyleProfileResult result = add_document_tree(profile, document);
    cJSON_Delete(document);
    return result;
}

/* ---- corpus ---- */

typedef struct {
    StyleProfile *profile;
    StyleProfileResult result;
} CorpusAdd;

static bool add_corpus_document(const char *piece_path, const char *document, const char *json,
                                AnalyzerDocumentStatus status, void *context) {
    (void)piece_path;
    (void)document;
    (void)status;
    CorpusAdd *add = context;
    add->result = analyzer_style_add_document_json(add->profile, json);
    return add->result == STYLE_PROFILE_OK;
}

StyleProfileResult analyzer_style_profile_add_corpus(StyleProfile *profile, const char *output_dir) {
    if (!profile || !output_dir) {
        return STYLE_PROFILE_ERR_ARGUMENT;
    }
    char *index_path = malloc(strlen(output_dir) + 24);
    if (!index_path) {
        return STYLE_PROFILE_ERR_ALLOC;
    }
    sprintf(index_path, "%s/corpus-index.json", output_dir);
    CorpusAdd add = {profile, STYLE_PROFILE_OK};
    char *input_dir = NULL;
    CorpusWalkResult walked = corpus_walk_documents(index_path, add_corpus_document, &add, &input_dir);
    free(index_path);
    StyleProfileResult result = STYLE_PROFILE_OK;
    switch (walked) {
    case CORPUS_WALK_OK:
        result = style_profile_add_training_corpus(profile, input_dir);
        break;
    case CORPUS_WALK_ERR_ALLOC:
        result = STYLE_PROFILE_ERR_ALLOC;
        break;
    case CORPUS_WALK_ERR_IO:
        result = STYLE_PROFILE_ERR_IO;
        break;
    case CORPUS_WALK_ERR_FORMAT:
        result = STYLE_PROFILE_ERR_FORMAT;
        break;
    case CORPUS_WALK_ERR_VISIT:
        result = add.result;
        break;
    }
    free(input_dir);
    return result;
}
