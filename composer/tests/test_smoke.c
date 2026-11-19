#include <assert.h>
#include <string.h>

#include <ursatz_composer/composer.h>
#include <chain/analysis_chain.h>

/*
 * The library links and reports its version, and the library chain resolves through it (the Composer links no Analyzer, R-620).
 */

int main(void) {
    assert(strlen(composer_version()) > 0);

    AnalysisChainConfig config = analysis_chain_config_default();
    assert(config.harmonic_spans && config.stages == ANALYSIS_CHAIN_STAGE_PERCEPTION);
    ChainResult report = {0};
    chain_result_destroy(&report);
    assert(strlen(analysis_chain_producer_key()) == 16);
    return 0;
}
