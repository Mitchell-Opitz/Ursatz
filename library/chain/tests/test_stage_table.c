#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "chain/analysis_chain.h"
#include "chain/analysis_chain_internal.h"
#include "chain_test_access.h"
#include "test_knowledge.h"

/*
 * The chain's stage table: each stage bit is in one row, every row needs only earlier rows, a row's error status is its bit's place
 * after the segments error, the names are unique, and the table's closure equals the closure the chain computed before it had a table.
 */

/* The closure as the chain wrote it before the table, copied here. */
static unsigned old_stage_closure(unsigned stages) {
    unsigned before;
    do {
        before = stages;
        if ((stages & ANALYSIS_CHAIN_STAGE_KEYS) != 0) {
            stages |= ANALYSIS_CHAIN_STAGE_SEGMENTS;
        }
        if ((stages & ANALYSIS_CHAIN_STAGE_CHORDS) != 0) {
            stages |= ANALYSIS_CHAIN_STAGE_KEYS;
        }
        if ((stages & ANALYSIS_CHAIN_STAGE_NUMERALS) != 0) {
            stages |= ANALYSIS_CHAIN_STAGE_CHORDS;
        }
        if ((stages & ANALYSIS_CHAIN_STAGE_OUTER_VOICES) != 0) {
            stages |= ANALYSIS_CHAIN_STAGE_SEGMENTS;
        }
        if ((stages & ANALYSIS_CHAIN_STAGE_CADENCES) != 0) {
            stages |= ANALYSIS_CHAIN_STAGE_NUMERALS | ANALYSIS_CHAIN_STAGE_OUTER_VOICES;
        }
        if ((stages & ANALYSIS_CHAIN_STAGE_FORM) != 0) {
            stages |= ANALYSIS_CHAIN_STAGE_CADENCES;
        }
        if ((stages & ANALYSIS_CHAIN_STAGE_METER) != 0) {
            stages |= ANALYSIS_CHAIN_STAGE_FORM | ANALYSIS_CHAIN_STAGE_CHORDS | ANALYSIS_CHAIN_STAGE_OUTER_VOICES;
        }
        if ((stages & ANALYSIS_CHAIN_STAGE_VOICE_LEADING) != 0) {
            stages |= ANALYSIS_CHAIN_STAGE_CADENCES | ANALYSIS_CHAIN_STAGE_SEGMENTS;
        }
        if ((stages & ANALYSIS_CHAIN_STAGE_MELODY) != 0) {
            stages |= ANALYSIS_CHAIN_STAGE_CHORDS | ANALYSIS_CHAIN_STAGE_OUTER_VOICES;
        }
        if ((stages & ANALYSIS_CHAIN_STAGE_SCHEMAS) != 0) {
            stages |= ANALYSIS_CHAIN_STAGE_NUMERALS | ANALYSIS_CHAIN_STAGE_OUTER_VOICES;
        }
    } while (stages != before);
    return stages;
}

/* The closure the table gives: a backward pass, since every row needs only earlier rows. */
static unsigned table_closure(unsigned stages) {
    for (size_t row = ANALYSIS_CHAIN_STAGE_COUNT; row-- > 0;) {
        if ((stages & analysis_chain_stages[row].bit) != 0) {
            stages |= analysis_chain_stages[row].needs;
        }
    }
    return stages;
}

/* The chain reads its pitch materials from the config's knowledge snapshot: a config without one (and the NULL config, which is the
   defaults) is refused before the notes are looked at, and a config with one gets as far as the notes. */
static void test_knowledge_refusal(void) {
    AnalysisNoteSet empty;
    memset(&empty, 0, sizeof(empty));
    AnalysisChain *chain = NULL;
    assert(analysis_chain_run(&empty, NULL, 0, NULL, &chain) == ANALYSIS_CHAIN_ERR_KNOWLEDGE && chain == NULL);
    AnalysisChainConfig config = analysis_chain_config_default();
    assert(config.knowledge == NULL);
    assert(analysis_chain_run(&empty, NULL, 0, &config, &chain) == ANALYSIS_CHAIN_ERR_KNOWLEDGE && chain == NULL);
    config.knowledge = test_knowledge();
    assert(analysis_chain_run(&empty, NULL, 0, &config, &chain) == ANALYSIS_CHAIN_ERR_NOTES && chain == NULL);
    /* The status is not a stage's, and comes before the stages' in the enumeration. */
    assert(analysis_chain_stage_name(ANALYSIS_CHAIN_ERR_KNOWLEDGE) == NULL);
    assert((unsigned)ANALYSIS_CHAIN_ERR_KNOWLEDGE < (unsigned)ANALYSIS_CHAIN_ERR_SEGMENTS);
}

int main(void) {
    assert(ANALYSIS_CHAIN_STAGE_COUNT == 12u);
    assert(ANALYSIS_CHAIN_STAGE_ALL == 0xFFFu);

    unsigned seen = 0;
    unsigned earlier = 0;
    for (size_t row = 0; row < ANALYSIS_CHAIN_STAGE_COUNT; row++) {
        const AnalysisChainStage *stage = &analysis_chain_stages[row];
        assert(stage->bit != 0 && (stage->bit & (stage->bit - 1)) == 0);
        assert((seen & stage->bit) == 0);
        seen |= stage->bit;
        assert((stage->needs & ~earlier) == 0);
        assert((stage->needs & stage->bit) == 0);
        unsigned index = 0;
        while ((1u << index) != stage->bit) {
            index++;
        }
        assert((unsigned)stage->error == (unsigned)ANALYSIS_CHAIN_ERR_SEGMENTS + index);
        assert(stage->name != NULL && stage->name[0] != '\0');
        assert(stage->prepare != NULL || stage->run_pass != NULL || stage->finish != NULL);
        assert(strcmp(analysis_chain_stage_name(stage->error), stage->name) == 0);
        for (size_t other = 0; other < row; other++) {
            assert(strcmp(analysis_chain_stages[other].name, stage->name) != 0);
        }
        earlier |= stage->bit;
    }
    assert(seen == ANALYSIS_CHAIN_STAGE_ALL);
    assert(analysis_chain_stage_name(ANALYSIS_CHAIN_ERR_ALLOC) == NULL);

    for (unsigned mask = 0; mask <= ANALYSIS_CHAIN_STAGE_ALL; mask++) {
        assert(table_closure(mask) == old_stage_closure(mask));
    }

    test_knowledge_refusal();

    puts("chain stage table: ok");
    return 0;
}
