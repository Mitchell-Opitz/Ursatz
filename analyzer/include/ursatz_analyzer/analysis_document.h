#ifndef URSATZ_ANALYZER_ANALYSIS_DOCUMENT_H
#define URSATZ_ANALYZER_ANALYSIS_DOCUMENT_H

#include <chain/chain_result.h>
#include <ursatz_analyzer/analysis_result.h>
#include <io/imported_score.h>

/*
 * The analysis document: the structured, versioned output described by
 * schema/analysis-document.v13.schema.json.
 *
 * Value of the document's top-level "schema_version". Breaking changes
 * bump it; additive fields do not.
 */
#define URSATZ_ANALYZER_DOCUMENT_SCHEMA_VERSION 13

/* An in-memory analysis document: an export of an AnalyzerResult. The JSON file is rendered from it. */
typedef struct AnalysisDocument AnalysisDocument;

/*
 * Builds the analysis document for one piece as an export of a reading (R-631): `reading` is the Analyzer's reading of `score`
 * (analyzer_result_reading) and its detail is the chain the document's other areas are read from. The keys, chords, cadences, phrases,
 * themes, form and motives sections walk the reading's claims of that kind and print each claim's element of the chain's result; every
 * other area reads the chain's result itself. Only the basename of path is recorded. knowledge is the snapshot the chain ran with: the
 * labels of a notated key signature are spelled with its major and natural minor materials.
 *
 * ANALYZER_ERR_ARGUMENT for a NULL argument, a reading that holds no detail (one read from a file) or one whose chain did not run every
 * stage (its report view is empty), ANALYZER_ERR_ALLOC on allocation failure. On ANALYZER_OK *out is a document the caller releases
 * with analysis_document_destroy; it references none of the arguments after returning, and is NULL on any other status.
 */
AnalyzerStatus analysis_document_build_from_reading(const char *path, const Reading *reading, const ImportedScore *score,
                                                    const KnowledgeSnapshot *knowledge, AnalysisDocument **out);

/*
 * Builds the document from an analysis result: analysis_document_build_from_reading over the result's reading, notes and knowledge.
 * NULL for a NULL result, a result that did not run every chain stage, or on allocation failure. The document does not reference the
 * result after returning.
 */
AnalysisDocument *analysis_document_build_from_result(const char *input_path, const AnalyzerResult *result);

/*
 * Serializes document as pretty-printed JSON with a 2-space indent and a
 * fixed key order, ending in a newline. Returns a heap string the caller
 * frees, or NULL on allocation failure.
 */
char *analysis_document_to_json(const AnalysisDocument *document);

void analysis_document_destroy(AnalysisDocument *document);

/* Convenience: analysis_document_build_from_result, then analysis_document_to_json. NULL as for the build. */
char *analysis_document_render_result(const char *input_path, const AnalyzerResult *result);

/*
 * The analysis document path for input_path: "<stem>.analysis.json"
 * next to it. Caller frees; NULL on allocation failure.
 */
char *derive_analysis_document_path(const char *input_path);

#endif
