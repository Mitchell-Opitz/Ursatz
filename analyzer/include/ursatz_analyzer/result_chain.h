#ifndef URSATZ_ANALYZER_RESULT_CHAIN_H
#define URSATZ_ANALYZER_RESULT_CHAIN_H

/*
 * The library chain behind an AnalyzerResult. This is the only Analyzer public header that includes a chain header; a
 * consumer that reads chain claims links ursatz_analyzer::chain (the ursatz_analyzer_chain target), and one that does not
 * links ursatz_analyzer alone and never sees the chain or the frameworks.
 */

#include <ursatz_analyzer/analysis_result.h>
#include <ursatz_chain/analysis_chain.h>

/* The library's results, through the accessors of ursatz_chain/analysis_chain.h. NULL for a piece with no notes. */
const AnalysisChain *analyzer_result_chain(const AnalyzerResult *result);

#endif
