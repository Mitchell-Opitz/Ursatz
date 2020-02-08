#ifndef URSATZ_ANALYZER_ANALYSIS_DOCUMENT_H
#define URSATZ_ANALYZER_ANALYSIS_DOCUMENT_H

#include <ursatz_analyzer/analysis_report.h>
#include <ursatz_analyzer/midi_pipeline.h>

/*
 * The analysis document: the structured, versioned output described by
 * schema/analysis-document.v1.schema.json.
 *
 * Value of the document's top-level "schema_version". Breaking changes
 * bump it; additive fields do not.
 */
#define URSATZ_ANALYZER_DOCUMENT_SCHEMA_VERSION 1

/*
 * Renders the analysis document for one piece from the imported notes
 * and the report run_analysis_pipeline produced for them. Fills
 * metadata, keys and chords; every other section is an empty array, or
 * null for meter and form. Output is pretty-printed JSON with a 2-space
 * indent and a fixed key order, ending in a newline.
 *
 * Returns a heap string the caller frees, or NULL on allocation failure.
 */
char *analysis_document_render(const char *input_path, const ImportedNotes *notes, const AnalysisReport *report);

/*
 * The analysis document path for input_path: "<stem>.analysis.json"
 * next to it, using the same stem rule as derive_report_path. Caller
 * frees; NULL on allocation failure.
 */
char *derive_analysis_document_path(const char *input_path);

#endif
