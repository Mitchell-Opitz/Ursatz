#ifndef URSATZ_TESTS_SUPPORT_CHAIN_TEST_ACCESS_H
#define URSATZ_TESTS_SUPPORT_CHAIN_TEST_ACCESS_H

#include "chain/analysis_chain.h"
#include "chain/analysis_chain_internal.h"

/* What the tests read of an AnalysisChain: its meter results. No application reads them, so they are not in ursatz_chain/analysis_chain.h. */

static inline const AnalysisChainMeter *analysis_chain_meter(const AnalysisChain *chain) {
    return chain != NULL && chain->have_densities ? &chain->meter : NULL;
}

#endif
