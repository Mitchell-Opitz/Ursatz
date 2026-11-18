#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "analysis/notes/analysis_note_build.h"
#include "chain/analysis_chain.h"
#include "io/imported_score.h"
#include "test_knowledge.h"
#include "ursatz_test.h"

/*
 * The chain run on an imported score (R-615): for Bach 114 and Mozart 545 the collected result's counts equal what the
 * Analyzer's report held at 069e65c (measured with the Analyzer before the report moved into the chain); analysis_chain_run
 * collects nothing, a partial stage set collects nothing, and a score with no notes gives an empty result.
 */

typedef struct {
    const char *file;
    size_t entries;
    size_t cadences;
    size_t key_regions;
    size_t phrases;
    size_t themes;
    size_t motives;
    size_t sections;
    size_t densities;
} Counts;

static const Counts EXPECTED[] = {
    {"Bach 114.mid", 54, 0, 1, 2, 0, 36, 1, 8},
    {"Mozart 545.mid", 210, 16, 12, 16, 10, 116, 10, 64},
};

static AnalysisChainConfig config(void) {
    AnalysisChainConfig c = analysis_chain_config_default();
    c.knowledge = test_knowledge();
    c.stages = ANALYSIS_CHAIN_STAGE_ALL;
    c.refine_keys = true;
    return c;
}

static void import(const char *file, ImportedScore *score) {
    char path[512];
    snprintf(path, sizeof(path), "%s/%s", URSATZ_CORPUS_DIR, file);
    ImportedScoreError error = {{0}};
    assert(imported_score_import_file(path, IMPORTED_FORMAT_AUTO, score, &error) == IMPORTED_SCORE_OK);
}

static void test_counts_equal_the_analyzers(void) {
    for (size_t i = 0; i < sizeof EXPECTED / sizeof EXPECTED[0]; i++) {
        ImportedScore score;
        import(EXPECTED[i].file, &score);
        AnalysisChainConfig c = config();
        AnalysisChain *chain = NULL;
        assert(analysis_chain_run_score(&score, &c, &chain) == ANALYSIS_CHAIN_OK);
        const ChainResult *result = analysis_chain_result(chain);
        assert(analysis_chain_score(chain) == &score);
        const Counts *want = &EXPECTED[i];
        assert(result->entry_count == want->entries && result->cadence_count == want->cadences);
        assert(result->key_region_count == want->key_regions && result->phrase_count == want->phrases);
        assert(result->theme_count == want->themes && result->motive_count == want->motives);
        assert(result->section_count == want->sections && result->density_count == want->densities);
        analysis_chain_destroy(chain);
        imported_score_destroy(&score);
    }
}

/* analysis_chain_run collects nothing and keeps no score; a run of fewer than every chain stage collects nothing. */
static void test_only_a_full_run_collects(void) {
    ImportedScore score;
    import("Field 1.mid", &score);

    AnalysisNoteSource source = {.events = (const NoteEvent *const *)score.events,
                                 .pitch_values = score.pitch_values,
                                 .tracks = score.streams,
                                 .count = score.count,
                                 .use_spelling = score.source.spelling_notated,
                                 .map = score.meter.meter_map};
    AnalysisNoteSet set;
    size_t *order = NULL;
    assert(analysis_note_set_build(&source, &set, &order));
    AnalysisChainConfig c = config();
    AnalysisChain *plain = NULL;
    assert(analysis_chain_run(&set, NULL, 0, &c, &plain) == ANALYSIS_CHAIN_OK);
    assert(analysis_chain_chords(plain) != NULL);
    assert(analysis_chain_result(plain) != NULL && analysis_chain_result(plain)->entry_count == 0);
    assert(analysis_chain_score(plain) == NULL);
    analysis_chain_destroy(plain);
    analysis_note_set_built_destroy(&set, order);

    c.stages = ANALYSIS_CHAIN_STAGE_SEGMENTS | ANALYSIS_CHAIN_STAGE_KEYS | ANALYSIS_CHAIN_STAGE_CHORDS;
    AnalysisChain *partial = NULL;
    assert(analysis_chain_run_score(&score, &c, &partial) == ANALYSIS_CHAIN_OK);
    assert(analysis_chain_chords(partial) != NULL);
    assert(analysis_chain_result(partial)->entry_count == 0 && analysis_chain_result(partial)->key_region_count == 0);
    assert(analysis_chain_score(partial) == &score);
    analysis_chain_destroy(partial);

    /* Every chain stage without the result-only stages: the result is collected without them. */
    c.stages = ANALYSIS_CHAIN_STAGE_PERCEPTION;
    AnalysisChain *perception = NULL;
    assert(analysis_chain_run_score(&score, &c, &perception) == ANALYSIS_CHAIN_OK);
    assert(analysis_chain_result(perception)->entry_count > 0);
    assert(analysis_chain_result(perception)->density_count == 0 && analysis_chain_result(perception)->section_count == 0);
    analysis_chain_destroy(perception);

    /* No knowledge, no perception stage. */
    AnalysisChain *refused = NULL;
    AnalysisChainConfig none = analysis_chain_config_default();
    assert(analysis_chain_run_score(&score, &none, &refused) == ANALYSIS_CHAIN_ERR_KNOWLEDGE && refused == NULL);
    c.stages = ANALYSIS_CHAIN_STAGE_PROGRESSIONS;
    assert(analysis_chain_run_score(&score, &c, &refused) == ANALYSIS_CHAIN_ERR_BAD_CONFIG && refused == NULL);
    assert(analysis_chain_run_score(NULL, &c, &refused) == ANALYSIS_CHAIN_ERR_NULL_ARGUMENT);
    imported_score_destroy(&score);
}

/* A score with no notes: an empty result. */
static void test_a_score_with_no_notes(void) {
    ImportedScoreView view = {0};
    ImportedScore score;
    assert(imported_score_view(&view, &score) == IMPORTED_SCORE_OK && score.count == 0);
    AnalysisChainConfig c = config();
    c.stages = ANALYSIS_CHAIN_STAGE_ALL;
    AnalysisChain *chain = NULL;
    assert(analysis_chain_run_score(&score, &c, &chain) == ANALYSIS_CHAIN_OK);
    const ChainResult *result = analysis_chain_result(chain);
    assert(result->entry_count == 0 && result->key_region_count == 0 && result->home_duration.den == 1);
    assert(analysis_chain_segments(chain) == NULL && analysis_chain_segment_count(chain) == 0);
    ChainResult moved;
    chain_result_take(chain, &moved);
    assert(moved.entry_count == 0);
    chain_result_destroy(&moved);
    analysis_chain_destroy(chain);
    imported_score_destroy(&score);
    /* A NULL chain reads as an empty result too. */
    assert(analysis_chain_result(NULL) != NULL && analysis_chain_result(NULL)->entry_count == 0);
}

int main(void) {
    test_counts_equal_the_analyzers();
    test_only_a_full_run_collects();
    test_a_score_with_no_notes();
    test_knowledge_release();
    return 0;
}
