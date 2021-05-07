#ifndef URSATZ_ANALYZER_ANALYSIS_DOCUMENT_H
#define URSATZ_ANALYZER_ANALYSIS_DOCUMENT_H

#include <ursatz_analyzer/analysis_report.h>
#include <ursatz_analyzer/midi_pipeline.h>

/*
 * The analysis document: the structured, versioned output described by
 * schema/analysis-document.v5.schema.json.
 *
 * Value of the document's top-level "schema_version". Breaking changes
 * bump it; additive fields do not.
 */
#define URSATZ_ANALYZER_DOCUMENT_SCHEMA_VERSION 5

/* An in-memory analysis document. It is the single source of truth for
   every output: the JSON file and the Markdown report are both rendered
   from it. */
typedef struct AnalysisDocument AnalysisDocument;

/*
 * Builds the analysis document for one piece from the imported notes
 * and the report run_analysis_pipeline produced for them. Fills
 * metadata, meter, keys, chords, outer voices and notes; every other section is an empty array, or
 * null for form. Only the basename of input_path is recorded.
 *
 * Returns a document the caller releases with analysis_document_destroy,
 * or NULL on allocation failure. It does not reference notes or report
 * after returning.
 */
AnalysisDocument *analysis_document_build(const char *input_path, const ImportedNotes *notes,
                                          const AnalysisReport *report);

/*
 * Serializes document as pretty-printed JSON with a 2-space indent and a
 * fixed key order, ending in a newline. Returns a heap string the caller
 * frees, or NULL on allocation failure.
 */
char *analysis_document_to_json(const AnalysisDocument *document);

void analysis_document_destroy(AnalysisDocument *document);

/* Convenience: analysis_document_build, then analysis_document_to_json.
   Returns a heap string the caller frees, or NULL on allocation failure. */
char *analysis_document_render(const char *input_path, const ImportedNotes *notes, const AnalysisReport *report);

/*
 * The analysis document path for input_path: "<stem>.analysis.json"
 * next to it, using the same stem rule as derive_report_path. Caller
 * frees; NULL on allocation failure.
 */
char *derive_analysis_document_path(const char *input_path);

#endif
