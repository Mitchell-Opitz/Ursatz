#include <ursatz_analyzer_internal/input_pipeline.h>

static UrsatzStatus status_of(ImportedScoreResult result, const ImportedScoreError *detail, const char *path, UrsatzError *error) {
    UrsatzStatus status;
    switch (result) {
        case IMPORTED_SCORE_OK:
            return URSATZ_OK;
        case IMPORTED_SCORE_ERR_ARGUMENT:
            status = URSATZ_ERR_REQUEST;
            break;
        case IMPORTED_SCORE_ERR_ALLOC:
            status = URSATZ_ERR_ALLOC;
            break;
        case IMPORTED_SCORE_ERR_IO:
            status = URSATZ_ERR_NOT_FOUND;
            break;
        default:
            status = URSATZ_ERR_FORMAT;
            break;
    }
    return ursatz_error_set(error, status, path, "%s", detail->message);
}

UrsatzStatus analyzer_import_file(const char *path, ImportedFormat format, ImportedScore *out, UrsatzError *error) {
    ImportedScoreError detail = {{0}};
    ImportedScoreResult result = imported_score_import_file(path, format, out, &detail);
    return status_of(result, &detail, path, error);
}

UrsatzStatus analyzer_import_buffer(const unsigned char *bytes, size_t length, ImportedFormat format, ImportedScore *out,
                                    UrsatzError *error) {
    ImportedScoreError detail = {{0}};
    ImportedScoreResult result = imported_score_import_buffer(bytes, length, format, out, &detail);
    return status_of(result, &detail, "", error);
}
