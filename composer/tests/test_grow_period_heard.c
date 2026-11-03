#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <standard-form/phrase_beginning.h>
#include <standard-form/phrase_ending.h>
#include <standard-form/phrase_level_form.h>

#include <ursatz_composer/form_hear.h>
#include <ursatz_composer/grow.h>
#include <ursatz_composer/plan_state.h>

/*
 * The grown periods and hybrids, heard. The same runs as test_grow_period (four keys and meters, seeds 1 to 10) for
 * one pairing of a beginning and an ending: form_hear, the way standard-form's adapter hears real music, reads each
 * plan's lanes alone, and the theme it finds must be the form the pairing makes: a period for an antecedent and a
 * consequent, a hybrid with the requested beginning and ending for the rest. The shares are printed.
 *
 * Only themes of 8 bars are held to it. The adapter cuts a phrase into ideas of two measures, so a theme of 4 bars is
 * two ideas, which it reads as one part, never as a theme: those are printed, and documented in docs/grow.md.
 *
 * Below 100% a pairing fails, and it is registered WILL_FAIL with the share measured written in docs/grow.md: the
 * share is not tuned, and the test is not skipped.
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
    size_t as_requested;
    size_t right_form_wrong_parts; /* a hybrid heard with another beginning or ending */
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

static PhraseBeginning beginning_of(PlanThemeBeginning beginning) {
    switch (beginning) {
    case PLAN_THEME_BEGINNING_ANTECEDENT:
        return PHRASE_BEGINNING_ANTECEDENT;
    case PLAN_THEME_BEGINNING_COMPOUND_BASIC_IDEA:
        return PHRASE_BEGINNING_COMPOUND_BASIC_IDEA;
    case PLAN_THEME_BEGINNING_PRESENTATION:
        return PHRASE_BEGINNING_PRESENTATION;
    }
    assert(0);
    return PHRASE_BEGINNING_COUNT;
}

static PhraseEnding ending_of(PlanThemeEnding ending) {
    switch (ending) {
    case PLAN_THEME_ENDING_CONTINUATION:
        return PHRASE_ENDING_CONTINUATION;
    case PLAN_THEME_ENDING_CONSEQUENT:
        return PHRASE_ENDING_CONSEQUENT;
    case PLAN_THEME_ENDING_CADENTIAL:
        return PHRASE_ENDING_CADENTIAL;
    }
    assert(0);
    return PHRASE_ENDING_COUNT;
}

static void hear_one(const Scenario *scenario, size_t bars, unsigned long seed, PlanThemeBeginning beginning,
                     PlanThemeEnding ending, Share *share) {
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
    request.pairing.beginning = beginning;
    request.pairing.ending = ending;
    request.cadence = CADENCE_TYPE_PERFECT_AUTHENTIC;
    memset(&config, 0, sizeof(config));
    config.seed = seed;
    assert(grow_build_plan(&request, &plan) == GROW_OK);
    assert(grow_run(plan, &config, &outcome) == GROW_OK);

    PhraseLevelForm wanted;
    bool period = beginning == PLAN_THEME_BEGINNING_ANTECEDENT && ending == PLAN_THEME_ENDING_CONSEQUENT;
    bool sentence = beginning == PLAN_THEME_BEGINNING_PRESENTATION && ending == PLAN_THEME_ENDING_CONTINUATION;
    wanted = period ? PHRASE_LEVEL_FORM_PERIOD : (sentence ? PHRASE_LEVEL_FORM_SENTENCE : PHRASE_LEVEL_FORM_HYBRID);

    PlanSpan theme = plan_node_span(plan, find_node(plan, "theme"));
    assert(form_hear(plan, theme, &heard) == FORM_HEAR_OK);
    share->runs++;
    bool as_requested = false;
    if (heard.count == 0) {
        share->nothing++;
    } else if (heard.themes[0].form != (int)wanted) {
        share->other_form++;
    } else if (wanted == PHRASE_LEVEL_FORM_HYBRID && (heard.themes[0].beginning != (int)beginning_of(beginning) ||
                                                       heard.themes[0].ending != (int)ending_of(ending))) {
        share->right_form_wrong_parts++;
    } else {
        share->as_requested++;
        as_requested = true;
    }
    /* grow_run heard the same plan, and says the same. */
    assert(outcome.heard_result == FORM_HEAR_OK);
    assert(outcome.heard.count == heard.count);
    assert(outcome.requested_form == (int)wanted);
    assert(outcome.heard_as_requested == as_requested);
    form_heard_destroy(&heard);
    grow_outcome_destroy(&outcome);
    plan_destroy(plan);
}

int main(int argc, char **argv) {
    if (argc != 3) {
        fputs("usage: test_grow_period_heard <beginning 0-2> <ending 0-2>\n", stderr);
        return 2;
    }
    PlanThemeBeginning beginning = (PlanThemeBeginning)atoi(argv[1]);
    PlanThemeEnding ending = (PlanThemeEnding)atoi(argv[2]);
    assert(beginning >= PLAN_THEME_BEGINNING_PRESENTATION && beginning <= PLAN_THEME_BEGINNING_COMPOUND_BASIC_IDEA);
    assert(ending >= PLAN_THEME_ENDING_CONTINUATION && ending <= PLAN_THEME_ENDING_CADENTIAL);

    Share share[2];
    memset(share, 0, sizeof(share));
    for (size_t s = 0; s < SCENARIO_COUNT; s++) {
        for (unsigned long seed = 1; seed <= 10; seed++) {
            hear_one(&SCENARIOS[s], 8, seed, beginning, ending, &share[0]);
            hear_one(&SCENARIOS[s], 4, seed, beginning, ending, &share[1]);
        }
    }
    static const size_t BARS[2] = {8, 4};
    for (int i = 0; i < 2; i++) {
        printf("%zu bars: heard as requested %zu of %zu (%zu%%), a hybrid with other parts %zu, another form %zu, no "
               "theme %zu\n",
               BARS[i], share[i].as_requested, share[i].runs, share[i].as_requested * 100 / share[i].runs,
               share[i].right_form_wrong_parts, share[i].other_form, share[i].nothing);
    }
    /* Exit with a status, not an abort, so that a registered WILL_FAIL sees an ordinary failure. */
    if (share[0].as_requested != share[0].runs) {
        puts("test_grow_period_heard: not every grown theme of 8 bars is heard as the form it was grown as");
        return 1;
    }
    puts("test_grow_period_heard: ok");
    return 0;
}
