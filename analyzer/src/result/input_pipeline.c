#include <ursatz_analyzer/input_pipeline.h>

#include <string.h>

static AnalyzerStatus status_of(ImportedScoreResult result, const ImportedScoreError *error, char *message,
                                size_t size) {
    if (result != IMPORTED_SCORE_OK && message && size > 0) {
        strncpy(message, error->message, size - 1);
        message[size - 1] = '\0';
    }
    switch (result) {
        case IMPORTED_SCORE_OK:
            return ANALYZER_OK;
        case IMPORTED_SCORE_ERR_ARGUMENT:
            return ANALYZER_ERR_ARGUMENT;
        case IMPORTED_SCORE_ERR_ALLOC:
            return ANALYZER_ERR_ALLOC;
        default:
            return ANALYZER_ERR_LIBRARY;
    }
}

AnalyzerStatus analyzer_import_file(const char *path, ImportedFormat format, ImportedScore *out, char *message,
                                    size_t size) {
    ImportedScoreError error = {{0}};
    ImportedScoreResult result = imported_score_import_file(path, format, out, &error);
    return status_of(result, &error, message, size);
}

AnalyzerStatus analyzer_import_buffer(const unsigned char *bytes, size_t length, ImportedFormat format,
                                      ImportedScore *out, char *message, size_t size) {
    ImportedScoreError error = {{0}};
    ImportedScoreResult result = imported_score_import_buffer(bytes, length, format, out, &error);
    return status_of(result, &error, message, size);
}
