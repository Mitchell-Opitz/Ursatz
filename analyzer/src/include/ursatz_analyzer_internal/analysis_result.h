#ifndef URSATZ_ANALYZER_INTERNAL_ANALYSIS_RESULT_H
#define URSATZ_ANALYZER_INTERNAL_ANALYSIS_RESULT_H

#include <ursatz_analyzer/analyzer_api.h>

/* What of an AnalyzerResult only the Analyzer's own files read (the API header has the rest). */

/* The report view the document is exported from (R-252). Never NULL for a result. */
const ChainResult *analyzer_result_report(const AnalyzerResult *result);

#endif
