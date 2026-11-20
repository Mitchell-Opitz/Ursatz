#ifndef URSATZ_ANALYZER_RUN_FILE_H
#define URSATZ_ANALYZER_RUN_FILE_H

#include <ursatz_analyzer_internal/analysis_document.h>
#include <ursatz_analyzer_internal/analysis_result.h>
#include <io/imported_score.h>

typedef enum {
    RUN_FILE_OK = 0,
    RUN_FILE_ERR_IMPORT,  /* *import_status says why */
    RUN_FILE_ERR_READING, /* *reading_status says why */
    RUN_FILE_ERR_ANALYZE  /* *analyze_status says why */
} RunFileResult;

/* Imports path, applies reading when it is not NULL, and analyzes the notes (analyzer_analyze, all stages) with the pitch materials of
   `knowledge`, which must outlive the result. On
   RUN_FILE_OK the caller owns *notes and *result (the result borrows the notes); on any other result both are already
   released. import_status, reading_status and analyze_status may be NULL; they receive the importer's, the reading's and the analysis's
   own result when that step fails, and import_error (NULL for none) the importer's message of a failed import. Nothing is printed: the
   warnings of an import that worked are in notes->source.warnings, for the caller to say. */
RunFileResult analyzer_run_file(const char *path, ImportedFormat format, const ImportedReading *reading,
                                const KnowledgeSnapshot *knowledge, ImportedScore *notes, AnalyzerResult **result, ImportedScoreResult *import_status,
                                ImportedReadingResult *reading_status, UrsatzStatus *analyze_status, UrsatzError *import_error);

/* Builds the analysis document of a run and releases *result and *notes. NULL on allocation failure. */
AnalysisDocument *analyzer_run_file_document(const char *path, ImportedScore *notes, AnalyzerResult *result);

#endif
