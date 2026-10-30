#include "pipeline/run_file.h"

RunFileResult analyzer_run_file(const char *path, InputFormat format, const NotationReading *reading,
                                ImportedNotes *notes, AnalysisReport *report, InputPipelineResult *import_status,
                                NotationReadingResult *reading_status) {
    InputPipelineResult imported = input_pipeline_import_file(path, format, notes);
    if (import_status) {
        *import_status = imported;
    }
    if (imported != INPUT_PIPELINE_OK) {
        return RUN_FILE_ERR_IMPORT;
    }
    if (reading) {
        NotationReadingResult applied = notation_reading_apply(notes, reading);
        if (reading_status) {
            *reading_status = applied;
        }
        if (applied != NOTATION_READING_OK) {
            imported_notes_destroy(notes);
            return RUN_FILE_ERR_READING;
        }
    }
    if (!run_analysis_pipeline(notes, report)) {
        imported_notes_destroy(notes);
        return RUN_FILE_ERR_ALLOC;
    }
    return RUN_FILE_OK;
}

AnalysisDocument *analyzer_run_file_document(const char *path, ImportedNotes *notes, AnalysisReport *report) {
    AnalysisDocument *document = analysis_document_build(path, notes, report);
    analysis_report_destroy(report);
    imported_notes_destroy(notes);
    return document;
}
