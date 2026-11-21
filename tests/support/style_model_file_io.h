#ifndef URSATZ_TESTS_SUPPORT_STYLE_MODEL_FILE_IO_H
#define URSATZ_TESTS_SUPPORT_STYLE_MODEL_FILE_IO_H

#include <stdlib.h>
#include <string.h>

#include <common/files.h>
#include <theory/style/style_model_file.h>

/*
 * What the tests do with a style model file on disk: the library parses and prints the text (style_model_file.h) and holds no file
 * IO, so a test reads and writes the file itself, as the workshop and the Composer do (R-691).
 */

static inline StyleModelFileResult test_style_model_read_without(const char *path, const char *const *categories, size_t count, StyleModel **out,
                                                                 size_t *pieces) {
    if (!path || !out) {
        return STYLE_MODEL_FILE_ERR_ARGUMENT;
    }
    *out = NULL;
    char *text = NULL;
    if (common_file_read(path, &text, NULL, NULL) != URSATZ_OK || !text) {
        free(text);
        return STYLE_MODEL_FILE_ERR_IO;
    }
    StyleModelFileResult result = style_model_file_parse(text, categories, count, out, pieces);
    free(text);
    return result;
}

static inline StyleModelFileResult test_style_model_read(const char *path, StyleModel **out, size_t *pieces) {
    return test_style_model_read_without(path, NULL, 0, out, pieces);
}

static inline StyleModelFileResult test_style_model_write(const StyleProfile *profile, const char *name, const char *version, const char *path) {
    char *text = NULL;
    StyleModelFileResult result = path ? style_model_file_text(profile, name, version, &text) : STYLE_MODEL_FILE_ERR_ARGUMENT;
    if (result != STYLE_MODEL_FILE_OK) {
        return result;
    }
    UrsatzStatus written = common_file_write_atomic(path, text, strlen(text), NULL);
    free(text);
    return written == URSATZ_OK ? STYLE_MODEL_FILE_OK : STYLE_MODEL_FILE_ERR_IO;
}

#endif
