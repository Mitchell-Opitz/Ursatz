#include <assert.h>
#include <stdio.h>
#include <string.h>

#include <theory/form/phrase_level_form.h>

#include <ursatz_composer/form_hear.h>
#include <ursatz_composer/grow.h>
#include <plan/plan_state.h>
#include "support/test_composer.h"
#include "support/test_knowledge.h"

/*
 * The grown sentences, heard. The same runs as test_grow_sentence (four keys and meters, 8 and 4 bars, seeds 1 to 10):
 * form_hear, the way standard-form's adapter hears real music, reads each plan's lanes alone, and the theme it finds
 * must be a sentence. The share heard is printed and held to the baseline table below: the test fails when the sentences
 * heard fall below the floor or the runs with no theme rise above the ceiling, and passes when a number holds or
 * improves. The target, every grown sentence heard as a sentence, is printed and does not gate (R-698).
 */

typedef struct {
    const char *name;
    int tonic;
    HarmonyMode mode;
    int beats;
    int unit;
    const char *motive;
} Scenario;

static const Scenario SCENARIOS[] = {
    {"C major 4/4", 0, HARMONY_MODE_MAJOR, 4, 4, "E4:q G4:e F4:e D4:q C4:q"},
    {"D minor 3/4", 2, HARMONY_MODE_MINOR, 3, 4, "F4:q A4:e G4:e E4:q"},
    {"G major 6/8", 7, HARMONY_MODE_MAJOR, 6, 8, "B4:q A4:e G4:e F#4:e D4:e"},
    {"F major 2/4", 5, HARMONY_MODE_MAJOR, 2, 4, "A4:q C5:e Bb4:e"},
};
#define SCENARIO_COUNT (sizeof(SCENARIOS) / sizeof(SCENARIOS[0]))

typedef struct {
    size_t runs;
    size_t sentences;
    size_t other_form;
    size_t nothing;
} Share;

static void hear_one(const Scenario *scenario, size_t bars, unsigned long seed, Share *share) {
    Motive motive;
    MotiveOptions options;
    GrowRequest request;
    GrowConfig config;
    GrowOutcome outcome;
    Plan *plan = NULL;
    FormHeard heard;
    memset(&options, 0, sizeof(options));
    options.knowledge = test_knowledge();
    options.has_meter = true;
    options.meter.beats_per_bar = scenario->beats;
    options.meter.beat_unit = scenario->unit;
    assert(motive_from_notes_text(scenario->motive, &options, &motive, NULL) == MOTIVE_OK);
    memset(&request, 0, sizeof(request));
    request.motive = &motive;
    request.key = test_key_mode(scenario->tonic, scenario->mode);
    request.meter = options.meter;
    request.bars = bars;
    request.pairing.beginning = SECTION_BEGINNING_PRESENTATION;
    request.pairing.ending = SECTION_ENDING_CONTINUATION;
    request.cadence = CADENCE_TYPE_PERFECT_AUTHENTIC;
    memset(&config, 0, sizeof(config));
    config.knowledge = test_knowledge();
    config.seed = seed;
    assert(grow_build_plan_checked(&request, &plan, NULL) == GROW_OK);
    assert(grow_run(plan, &config, &outcome) == GROW_OK);

    PlanSpan theme = plan_node_span(plan, test_find_node(plan, "theme"));
    assert(test_form_hear(plan, theme, &heard) == FORM_HEAR_OK);
    share->runs++;
    if (heard.count == 0) {
        share->nothing++;
    } else if (heard.themes[0].form == PHRASE_LEVEL_FORM_SENTENCE) {
        share->sentences++;
    } else {
        share->other_form++;
    }
    /* grow_run heard the same plan, and says the same. */
    assert(outcome.heard_result == FORM_HEAR_OK);
    assert(outcome.heard.count == heard.count);
    assert(outcome.heard_as_requested == (heard.count > 0 && heard.themes[0].form == PHRASE_LEVEL_FORM_SENTENCE));
    form_heard_destroy(&heard);
    grow_outcome_destroy(&outcome);
    plan_destroy(plan);
}

/*
 * What the runs heard at 243bd316 (seeds 1 to 10, four scenarios, so 40 runs per size; the runs are deterministic).
 * 8 bars: 16 sentences, 17 another form, 7 no theme. 4 bars: 0, 0, 40. Raise a baseline when a change lifts it.
 */
typedef struct {
    size_t bars;
    size_t floor_sentences;
    size_t ceiling_nothing;
} Baseline;

static const Baseline BASELINES[2] = {{8, 16, 7}, {4, 0, 40}};

int main(void) {
    Share share[2];
    memset(share, 0, sizeof(share));
    for (size_t s = 0; s < SCENARIO_COUNT; s++) {
        for (unsigned long seed = 1; seed <= 10; seed++) {
            hear_one(&SCENARIOS[s], 8, seed, &share[0]);
            hear_one(&SCENARIOS[s], 4, seed, &share[1]);
        }
    }
    size_t sentences = 0;
    size_t runs = 0;
    int failed = 0;
    for (int i = 0; i < 2; i++) {
        printf("%zu bars: heard as a sentence %zu of %zu (%zu%%), as another form %zu, no theme %zu\n",
               BASELINES[i].bars, share[i].sentences, share[i].runs, share[i].sentences * 100 / share[i].runs,
               share[i].other_form, share[i].nothing);
        if (share[i].sentences < BASELINES[i].floor_sentences) {
            printf("%zu bars: heard as a sentence %zu, below the baseline %zu\n", BASELINES[i].bars,
                   share[i].sentences, BASELINES[i].floor_sentences);
            failed = 1;
        }
        if (share[i].nothing > BASELINES[i].ceiling_nothing) {
            printf("%zu bars: no theme %zu, above the baseline %zu\n", BASELINES[i].bars, share[i].nothing,
                   BASELINES[i].ceiling_nothing);
            failed = 1;
        }
        sentences += share[i].sentences;
        runs += share[i].runs;
    }
    printf("all: heard as a sentence %zu of %zu (%zu%%)\n", sentences, runs, sentences * 100 / runs);
    puts("target: every grown sentence heard as a sentence (not gating)");
    if (failed) {
        puts("test_grow_sentence_heard: a number is worse than its baseline");
        return 1;
    }
    puts("test_grow_sentence_heard: ok");
    return 0;
}
