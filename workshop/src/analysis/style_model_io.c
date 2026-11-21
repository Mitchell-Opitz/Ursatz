#include "analysis/style_model_io.h"

#include <stdlib.h>
#include <string.h>

#include <common/files.h>

StyleModelFileResult workshop_style_model_read(const char *path, StyleModel **out, size_t *pieces) {
    if (!path || !out) {
        return STYLE_MODEL_FILE_ERR_ARGUMENT;
    }
    *out = NULL;
    char *text = NULL;
    if (common_file_read(path, &text, NULL, NULL) != URSATZ_OK || !text) {
        free(text);
        return STYLE_MODEL_FILE_ERR_IO;
    }
    StyleModelFileResult result = style_model_file_parse(text, NULL, 0, out, pieces);
    free(text);
    return result;
}

StyleModelFileResult workshop_style_model_write(const StyleProfile *profile, const char *name, const char *model_version, const char *path) {
    if (!path) {
        return STYLE_MODEL_FILE_ERR_ARGUMENT;
    }
    char *text = NULL;
    StyleModelFileResult result = style_model_file_text(profile, name, model_version, &text);
    if (result != STYLE_MODEL_FILE_OK) {
        return result;
    }
    UrsatzStatus written = common_file_write_atomic(path, text, strlen(text), NULL);
    free(text);
    return written == URSATZ_OK ? STYLE_MODEL_FILE_OK : STYLE_MODEL_FILE_ERR_IO;
}
