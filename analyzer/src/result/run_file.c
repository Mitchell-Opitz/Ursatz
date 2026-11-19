#include "result/run_file.h"

#include <stdio.h>

RunFileResult analyzer_run_file(const char *path, ImportedFormat format, const ImportedReading *reading,
                                const KnowledgeSnapshot *knowledge, ImportedScore *notes, AnalyzerResult **result, ImportedScoreResult *import_status,
                                ImportedReadingResult *reading_status, UrsatzStatus *analyze_status) {
    ImportedScoreError error = {{0}};
    ImportedScoreResult imported = imported_score_import_file(path, format, notes, &error);
    if (import_status) {
        *import_status = imported;
    }
    if (imported != IMPORTED_SCORE_OK) {
        fprintf(stderr, "%s\n", error.message);
        return RUN_FILE_ERR_IMPORT;
    }
    for (size_t i = 0; i < notes->source.warning_line_count; i++) {
        fprintf(stderr, "%s\n", notes->source.warnings[i]);
    }
    if (reading) {
        ImportedReadingResult applied = imported_reading_apply(notes, reading);
        if (reading_status) {
            *reading_status = applied;
        }
        if (applied != IMPORTED_READING_OK) {
            imported_score_destroy(notes);
            return RUN_FILE_ERR_READING;
        }
    }
    AnalyzerOptions options = analyzer_options_default();
    options.knowledge = knowledge;
    UrsatzStatus analyzed = analyzer_analyze(notes, &options, result);
    if (analyzed != URSATZ_OK) {
        if (analyze_status) {
            *analyze_status = analyzed;
        }
        imported_score_destroy(notes);
        return RUN_FILE_ERR_ANALYZE;
    }
    return RUN_FILE_OK;
}

AnalysisDocument *analyzer_run_file_document(const char *path, ImportedScore *notes, AnalyzerResult *result) {
    AnalysisDocument *document = analysis_document_build_from_result(path, result);
    analyzer_result_destroy(result);
    imported_score_destroy(notes);
    return document;
}
