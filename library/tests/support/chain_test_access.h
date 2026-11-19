#ifndef URSATZ_TESTS_SUPPORT_CHAIN_TEST_ACCESS_H
#define URSATZ_TESTS_SUPPORT_CHAIN_TEST_ACCESS_H

#include "chain/analysis_chain.h"
#include "chain/analysis_chain_internal.h"

/* What the tests read of an AnalysisChain beyond its public accessors: the stage table. */

/* The name of the stage whose failure a status reports, or NULL for a status that is not a stage's. */
static inline const char *analysis_chain_stage_name(AnalysisChainStatus status) {
    for (size_t row = 0; row < ANALYSIS_CHAIN_STAGE_COUNT; row++) {
        if (analysis_chain_stages[row].error == status) {
            return analysis_chain_stages[row].name;
        }
    }
    return NULL;
}

#endif
