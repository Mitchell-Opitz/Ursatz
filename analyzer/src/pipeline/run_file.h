#ifndef URSATZ_ANALYZER_RUN_FILE_H
#define URSATZ_ANALYZER_RUN_FILE_H

#include <ursatz_analyzer/analysis_document.h>
#include <ursatz_analyzer/analysis_result.h>
#include <io/imported_score.h>

typedef enum {
    RUN_FILE_OK = 0,
    RUN_FILE_ERR_IMPORT,  /* *import_status says why */
    RUN_FILE_ERR_READING, /* *reading_status says why */
    RUN_FILE_ERR_ALLOC
} RunFileResult;

/* Imports path, applies reading when it is not NULL, and analyzes the notes (analyzer_analyze, all stages) with the pitch materials of
   `knowledge`, which must outlive the result. On
   RUN_FILE_OK the caller owns *notes and *result (the result borrows the notes); on any other result both are already
   released. import_status and reading_status may be NULL; they receive the importer's and the reading's own result when
   that step fails. */
RunFileResult analyzer_run_file(const char *path, ImportedFormat format, const ImportedReading *reading,
                                const KnowledgeSnapshot *knowledge, ImportedScore *notes, AnalyzerResult **result, ImportedScoreResult *import_status,
                                ImportedReadingResult *reading_status);

/* Builds the analysis document of a run and releases *result and *notes. NULL on allocation failure. */
AnalysisDocument *analyzer_run_file_document(const char *path, ImportedScore *notes, AnalyzerResult *result);

#endif
