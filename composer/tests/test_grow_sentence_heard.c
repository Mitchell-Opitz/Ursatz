#include <assert.h>
#include <stdio.h>
#include <string.h>

#include <standard-form/phrase_level_form.h>

#include <ursatz_composer/form_hear.h>
#include <ursatz_composer/grow.h>
#include <ursatz_composer/plan_state.h>

/*
 * The grown sentences, heard. The same runs as test_grow_sentence (four keys and meters, 8 and 4 bars, seeds 1 to 10):
 * form_hear, the way standard-form's adapter hears real music, reads each plan's lanes alone, and the theme it finds
 * must be a sentence. The share heard is printed. Below 100% this test fails, and it is registered WILL_FAIL with the
 * share measured written in docs/grow.md: the share is not tuned, and the test is not skipped.
 */

typedef struct {
    const char *name;
    int tonic;
    KeyStructureMode mode;
    int beats;
    int unit;
    const char *motive;
} Scenario;

static const Scenario SCENARIOS[] = {
    {"C major 4/4", 0, KEY_STRUCTURE_MODE_MAJOR, 4, 4, "E4:q G4:e F4:e D4:q C4:q"},
    {"D minor 3/4", 2, KEY_STRUCTURE_MODE_MINOR, 3, 4, "F4:q A4:e G4:e E4:q"},
    {"G major 6/8", 7, KEY_STRUCTURE_MODE_MAJOR, 6, 8, "B4:q A4:e G4:e F#4:e D4:e"},
    {"F major 2/4", 5, KEY_STRUCTURE_MODE_MAJOR, 2, 4, "A4:q C5:e Bb4:e"},
};
#define SCENARIO_COUNT (sizeof(SCENARIOS) / sizeof(SCENARIOS[0]))

typedef struct {
    size_t runs;
    size_t sentences;
    size_t other_form;
    size_t nothing;
} Share;

static NodeId find_node(const PiecePlan *plan, const char *name) {
    for (NodeId id = 1; id <= plan_node_count(plan); id++) {
        const char *found = plan_node_name(plan, id);
        if (found && strcmp(found, name) == 0) {
            return id;
        }
    }
    return PLAN_NODE_NONE;
}

static void hear_one(const Scenario *scenario, size_t bars, unsigned long seed, Share *share) {
    Motive motive;
    MotiveOptions options;
    GrowRequest request;
    GrowConfig config;
    GrowOutcome outcome;
    PiecePlan *plan = NULL;
    FormHeard heard;
    memset(&options, 0, sizeof(options));
    options.has_meter = true;
    options.meter.beats_per_bar = scenario->beats;
    options.meter.beat_unit = scenario->unit;
    assert(motive_from_notes_text(scenario->motive, &options, &motive, NULL) == MOTIVE_OK);
    memset(&request, 0, sizeof(request));
    request.motive = &motive;
    request.key = plan_key_of_pitch_class(scenario->tonic, scenario->mode);
    request.meter = options.meter;
    request.bars = bars;
    request.pairing.beginning = PLAN_THEME_BEGINNING_PRESENTATION;
    request.pairing.ending = PLAN_THEME_ENDING_CONTINUATION;
    request.cadence = CADENCE_TYPE_PERFECT_AUTHENTIC;
    memset(&config, 0, sizeof(config));
    config.seed = seed;
    assert(grow_build_plan(&request, &plan) == GROW_OK);
    assert(grow_run(plan, &config, &outcome) == GROW_OK);

    PlanSpan theme = plan_node_span(plan, find_node(plan, "theme"));
    assert(form_hear(plan, theme, &heard) == FORM_HEAR_OK);
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

int main(void) {
    Share share[2];
    memset(share, 0, sizeof(share));
    for (size_t s = 0; s < SCENARIO_COUNT; s++) {
        for (unsigned long seed = 1; seed <= 10; seed++) {
            hear_one(&SCENARIOS[s], 8, seed, &share[0]);
            hear_one(&SCENARIOS[s], 4, seed, &share[1]);
        }
    }
    static const size_t BARS[2] = {8, 4};
    size_t sentences = 0;
    size_t runs = 0;
    for (int i = 0; i < 2; i++) {
        printf("%zu bars: heard as a sentence %zu of %zu (%zu%%), as another form %zu, no theme %zu\n", BARS[i],
               share[i].sentences, share[i].runs, share[i].sentences * 100 / share[i].runs, share[i].other_form,
               share[i].nothing);
        sentences += share[i].sentences;
        runs += share[i].runs;
    }
    printf("all: heard as a sentence %zu of %zu (%zu%%)\n", sentences, runs, sentences * 100 / runs);
    /* Exit with a status, not an abort, so that a registered WILL_FAIL sees an ordinary failure. */
    if (sentences != runs) {
        puts("test_grow_sentence_heard: not every grown sentence is heard as a sentence");
        return 1;
    }
    puts("test_grow_sentence_heard: ok");
    return 0;
}
