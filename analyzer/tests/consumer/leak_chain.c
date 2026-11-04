/* Must not compile: a target linking only ursatz_analyzer cannot see a chain header. */
#include <ursatz_analyzer/analysis_result.h>
#include <ursatz_chain/analysis_chain.h>

int analyzer_consumer_leak_chain(void) { return 0; }
