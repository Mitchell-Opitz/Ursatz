/* Must not compile: a target linking only ursatz_analyzer sees the chain's result header (chain/chain_result.h, R-615) but no
   chain internals. */
#include <ursatz_analyzer/analysis_result.h>
#include <chain/analysis_chain_internal.h>

int analyzer_consumer_leak_chain(void) { return 0; }
