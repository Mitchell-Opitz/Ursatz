#ifndef URSATZ_ANALYZER_INTERNAL_ANALYSIS_DOCUMENT_H
#define URSATZ_ANALYZER_INTERNAL_ANALYSIS_DOCUMENT_H

#include <ursatz_analyzer/analyzer_api.h>
#include <ursatz_analyzer_internal/analysis_result.h>

/* What of the analysis document only the Analyzer's own files use (the API header has the document and its build from a result). */

/*
 * Builds the analysis document for one piece as an export of a reading (R-631): `reading` is the Analyzer's reading of `score`
 * (analyzer_result_reading) and its detail is the chain the document's other areas are read from. The keys, chords, cadences, phrases,
 * themes, form and motives sections walk the reading's claims of that kind and print each claim's element of the chain's result; every
 * other area reads the chain's result itself. Only the basename of path is recorded. knowledge is the snapshot the chain ran with: the
 * labels of a notated key signature are spelled with its major and natural minor materials.
 *
 * URSATZ_ERR_REQUEST for a NULL argument, a reading that holds no detail (one read from a file) or one whose chain did not run every
 * stage (its report view is empty), URSATZ_ERR_ALLOC on allocation failure. On URSATZ_OK *out is a document the caller releases
 * with analyzer_document_destroy; it references none of the arguments after returning, and is NULL on any other status.
 */
UrsatzStatus analysis_document_build_from_reading(const char *path, const Reading *reading, const ImportedScore *score,
                                                    const KnowledgeSnapshot *knowledge, AnalysisDocument **out);

/* Convenience: analyzer_document_build, then analyzer_document_to_json. NULL as for the build. */
char *analysis_document_render_result(const char *input_path, const AnalyzerResult *result);

/*
 * The analysis document path for input_path: "<stem>.analysis.json"
 * next to it. Caller frees; NULL on allocation failure.
 */
char *derive_analysis_document_path(const char *input_path);

#endif
