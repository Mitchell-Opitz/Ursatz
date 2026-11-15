/* A consumer that links ursatz_analyzer::chain reads the chain through result_chain.h. */
#include <ursatz_analyzer/result_chain.h>

const AnalysisChain *analyzer_consumer_chain(const AnalyzerResult *result) { return analyzer_result_chain(result); }
