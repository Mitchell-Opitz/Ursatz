#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <ursatz_composer/grow.h>
#include <ursatz_composer/plan_state.h>
#include <ursatz_composer/theme_shape.h>

#include "grow/grow_internal.h"

/*
 * The construction chosen for a theme that is given none (R-469, B4 of P47): grow_construction_choose gives each row of
 * the rule's list exactly its ideas, its extensions and its reason, grow_construction_describe the sentence for it, 4 and
 * 8 bars with a motif of two bars or fewer give none, and grow_build_plan builds the chosen one into the plan and
 * grow_run reports its sentence. Fast: one seed.
 */

typedef struct {
    const char *name;
    int beats;
    int unit;
    const char *one;   /* one bar */
    const char *two;   /* two bars */
    const char *three; /* three bars */
} Scenario;

static const Scenario SCENARIOS[] = {
    {"C major 4/4", 4, 4, "E4:q G4:e F4:e D4:q C4:q", "E4:q G4:e F4:e D4:q C4:q E4:q G4:q A4:q G4:q",
     "E4:q G4:e F4:e D4:q C4:q E4:q G4:q A4:q G4:q F4:q D4:q E4:h"},
    {"A minor 3/4", 3, 4, "C5:q E5:e D5:e B4:q", "C5:q E5:e D5:e B4:q A4:q C5:q B4:q",
     "C5:q E5:e D5:e B4:q A4:q C5:q B4:q G#4:q A4:h"},
};
#define SCENARIO_COUNT (sizeof(SCENARIOS) / sizeof(SCENARIOS[0]))

typedef struct {
    PlanExtensionKind kind;
    GrowIdea idea;
    unsigned char bars;
    bool varied;
} Extension;

typedef enum { ONE, THREE } MotifLength;

typedef struct {
    const char *name;
    size_t bars;
    MotifLength motif;
    PlanThemeBeginning beginning;
    PlanThemeEnding ending;
    unsigned char ideas[GROW_IDEA_COUNT]; /* by GrowIdea */
    PlanConstructionReason reason;
    size_t extension_count;
    Extension extensions[PLAN_CONSTRUCTION_EXTENSIONS];
    const char *text;
} Row;

#define PERIOD PLAN_THEME_BEGINNING_ANTECEDENT, PLAN_THEME_ENDING_CONSEQUENT
#define SENTENCE PLAN_THEME_BEGINNING_PRESENTATION, PLAN_THEME_ENDING_CONTINUATION
#define CADENTIAL PLAN_THEME_BEGINNING_PRESENTATION, PLAN_THEME_ENDING_CADENTIAL

/* Ideas in GrowIdea order: basic, repetition, contrasting, consequent basic, consequent contrasting, fragment 1, fragment 2,
   cadential, cadential 1, cadential 2. */
static const Row ROWS[] = {
    {"6-bar period", 6, ONE, PERIOD, {2, 0, 1, 2, 1, 0, 0, 0, 0, 0}, PLAN_CONSTRUCTION_FROM_LENGTH, 0, {{0}},
     "3+3 period: the section is 6 bars, so each phrase is 3."},
    {"10-bar period", 10, ONE, PERIOD, {2, 0, 2, 2, 2, 0, 0, 0, 0, 0}, PLAN_CONSTRUCTION_FROM_LENGTH, 1,
     {{PLAN_EXTENSION_REPEAT, GROW_IDEA_CONSEQUENT_CONTRASTING, 2, true}},
     "4+6 period: the section is 10 bars, so the consequent repeats its contrasting idea, varied (bars 9 and 10)."},
    {"10-bar period from a 3-bar motif", 10, THREE, PERIOD, {3, 0, 2, 3, 2, 0, 0, 0, 0, 0}, PLAN_CONSTRUCTION_FROM_MOTIVE, 0,
     {{0}}, "5+5 period: the motif is 3 bars, so the basic idea is 3 bars and each phrase 5."},
    {"12-bar period from a 3-bar motif", 12, THREE, PERIOD, {3, 0, 3, 3, 3, 0, 0, 0, 0, 0}, PLAN_CONSTRUCTION_FROM_MOTIVE, 0,
     {{0}}, "6+6 period: the motif is 3 bars, so the basic idea is 3 bars and each phrase 6."},
    {"9-bar sentence", 9, ONE, SENTENCE, {2, 2, 0, 0, 0, 1, 1, 2, 0, 0}, PLAN_CONSTRUCTION_FROM_LENGTH, 1,
     {{PLAN_EXTENSION_REPEAT, GROW_IDEA_FRAGMENT_2, 1, false}},
     "4+5 sentence: the section is 9 bars, so fragment 2 is repeated a step lower (bar 7)."},
    {"10-bar sentence", 10, ONE, SENTENCE, {2, 2, 0, 0, 0, 1, 1, 2, 0, 0}, PLAN_CONSTRUCTION_FROM_LENGTH, 1,
     {{PLAN_EXTENSION_EVADED, GROW_IDEA_CADENTIAL, 2, false}},
     "4+6 sentence: the section is 10 bars, so the cadence in bar 8 is evaded (V to vi) and the cadential idea returns to close "
     "in bar 10."},
    {"11-bar sentence", 11, ONE, SENTENCE, {2, 2, 0, 0, 0, 1, 1, 2, 0, 0}, PLAN_CONSTRUCTION_FROM_LENGTH, 2,
     {{PLAN_EXTENSION_REPEAT, GROW_IDEA_FRAGMENT_2, 1, false}, {PLAN_EXTENSION_EVADED, GROW_IDEA_CADENTIAL, 2, false}},
     "4+7 sentence: the section is 11 bars, so fragment 2 is repeated a step lower (bar 7) and the cadence in bar 9 is evaded "
     "(V to vi) and the cadential idea returns to close in bar 11."},
    {"12-bar sentence", 12, ONE, SENTENCE, {2, 2, 0, 0, 0, 1, 1, 2, 0, 0}, PLAN_CONSTRUCTION_FROM_LENGTH, 2,
     {{PLAN_EXTENSION_EVADED, GROW_IDEA_CADENTIAL, 2, false}, {PLAN_EXTENSION_ADDITION, GROW_IDEA_CADENTIAL, 2, false}},
     "4+6 sentence: the section is 12 bars, so the cadence in bar 8 is evaded (V to vi) and the cadential idea returns to close "
     "in bar 10 and a 2-bar addition follows (bars 11 and 12)."},
    {"7-bar period", 7, ONE, PERIOD, {2, 0, 1, 2, 1, 0, 0, 0, 0, 0}, PLAN_CONSTRUCTION_FROM_LENGTH, 1,
     {{PLAN_EXTENSION_STRETCH, GROW_IDEA_CONSEQUENT_CONTRASTING, 1, false}},
     "3+4 period: the section is 7 bars, so the consequent contrasting idea is stretched to 2 bars (bars 6 and 7)."},
    {"9-bar cadential ending", 9, ONE, CADENTIAL, {2, 2, 0, 0, 0, 0, 0, 0, 2, 2}, PLAN_CONSTRUCTION_FROM_LENGTH, 1,
     {{PLAN_EXTENSION_STRETCH, GROW_IDEA_CADENTIAL_2, 1, false}},
     "4+5 hybrid: the section is 9 bars, so the cadential 2 is stretched to 3 bars (bars 7 to 9)."},
};
#define ROW_COUNT (sizeof(ROWS) / sizeof(ROWS[0]))

static Motive make_motive(const Scenario *scenario, const char *text) {
    Motive motive;
    MotiveOptions options;
    memset(&options, 0, sizeof(options));
    options.has_meter = true;
    options.meter.beats_per_bar = scenario->beats;
    options.meter.beat_unit = scenario->unit;
    assert(motive_from_notes_text(text, &options, &motive, NULL) == MOTIVE_OK);
    return motive;
}

static PlanMeter meter_of(const Scenario *scenario) {
    PlanMeter meter;
    memset(&meter, 0, sizeof(meter));
    meter.beats_per_bar = scenario->beats;
    meter.beat_unit = scenario->unit;
    return meter;
}

static void expect_construction(const PlanConstruction *construction, const Row *row, const char *where) {
    bool same = memcmp(construction->idea_bars, row->ideas, sizeof(row->ideas)) == 0 && construction->reason == row->reason &&
                construction->extension_count == row->extension_count;
    for (size_t k = 0; same && k < row->extension_count; k++) {
        const PlanExtension *got = &construction->extensions[k];
        same = got->kind == row->extensions[k].kind && got->idea == row->extensions[k].idea &&
               got->bars == row->extensions[k].bars && got->varied == row->extensions[k].varied;
    }
    if (!same) {
        fprintf(stderr, "%s, %s: not the construction wanted (reason %d, %zu extensions)\n", where, row->name,
                (int)construction->reason, construction->extension_count);
    }
    assert(same);
}

static void test_rows(void) {
    for (size_t s = 0; s < SCENARIO_COUNT; s++) {
        const Scenario *scenario = &SCENARIOS[s];
        PlanMeter meter = meter_of(scenario);
        Motive one = make_motive(scenario, scenario->one);
        Motive three = make_motive(scenario, scenario->three);
        for (size_t r = 0; r < ROW_COUNT; r++) {
            const Row *row = &ROWS[r];
            PlanThemePairing pairing = {row->beginning, row->ending};
            PlanConstruction construction;
            char text[192];
            memset(&construction, 0xAA, sizeof(construction));
            assert(grow_construction_choose(row->motif == THREE ? &three : &one, &meter, row->bars, &pairing,
                                            CADENCE_TYPE_PERFECT_AUTHENTIC, &construction));
            expect_construction(&construction, row, scenario->name);
            size_t written = grow_construction_describe(&construction, &pairing, text, sizeof(text));
            if (strcmp(text, row->text) != 0) {
                fprintf(stderr, "%s, %s:\n  got    %s\n  wanted %s\n", scenario->name, row->name, text, row->text);
            }
            assert(strcmp(text, row->text) == 0 && written == strlen(row->text));
            /* No draw: asking again gives the same. */
            PlanConstruction again;
            assert(grow_construction_choose(row->motif == THREE ? &three : &one, &meter, row->bars, &pairing,
                                            CADENCE_TYPE_PERFECT_AUTHENTIC, &again));
            assert(memcmp(&construction, &again, sizeof(construction)) == 0);
        }
    }
}

static bool is_zero(const PlanConstruction *construction) {
    PlanConstruction zero;
    memset(&zero, 0, sizeof(zero));
    return memcmp(construction, &zero, sizeof(zero)) == 0;
}

static void test_none(void) {
    for (size_t s = 0; s < SCENARIO_COUNT; s++) {
        const Scenario *scenario = &SCENARIOS[s];
        PlanMeter meter = meter_of(scenario);
        Motive one = make_motive(scenario, scenario->one);
        Motive two = make_motive(scenario, scenario->two);
        PlanConstruction construction;
        PlanThemePairing period = {PERIOD};
        PlanThemePairing sentence = {SENTENCE};
        /* 4 and 8 bars with a motif of 2 bars or fewer are built as they always were. */
        static const size_t PLAIN[2] = {4, 8};
        for (size_t k = 0; k < 2; k++) {
            memset(&construction, 0xAA, sizeof(construction));
            assert(!grow_construction_choose(&one, &meter, PLAIN[k], &period, CADENCE_TYPE_PERFECT_AUTHENTIC, &construction));
            assert(is_zero(&construction));
            memset(&construction, 0xAA, sizeof(construction));
            assert(!grow_construction_choose(&two, &meter, PLAIN[k], &sentence, CADENCE_TYPE_PERFECT_AUTHENTIC, &construction));
            assert(is_zero(&construction));
        }
        /* A motif longer than the basic idea the length gives, a theme too short or too long, an excess an HC cannot take. */
        memset(&construction, 0xAA, sizeof(construction));
        assert(!grow_construction_choose(&two, &meter, 5, &period, CADENCE_TYPE_PERFECT_AUTHENTIC, &construction));
        assert(is_zero(&construction));
        assert(!grow_construction_choose(&one, &meter, 2, &period, CADENCE_TYPE_PERFECT_AUTHENTIC, &construction));
        assert(!grow_construction_choose(&one, &meter, 17, &period, CADENCE_TYPE_PERFECT_AUTHENTIC, &construction));
        assert(is_zero(&construction));
        assert(!grow_construction_choose(&one, &meter, 10, &sentence, CADENCE_TYPE_HALF, &construction));
        assert(is_zero(&construction));
        /* A continuation needs a basic idea of 2 bars: 6 bars give 2,2 | and no room for its three ideas. */
        assert(!grow_construction_choose(&one, &meter, 6, &sentence, CADENCE_TYPE_PERFECT_AUTHENTIC, &construction));
        assert(is_zero(&construction));
    }
}

static NodeId find_node(const PiecePlan *plan, const char *name) {
    for (NodeId id = 1; id <= plan_node_count(plan); id++) {
        const char *found = plan_node_name(plan, id);
        if (found && strcmp(found, name) == 0) {
            return id;
        }
    }
    return PLAN_NODE_NONE;
}

static GrowRequest request_of(const Scenario *scenario, const Motive *motive, size_t bars, PlanThemeBeginning beginning,
                              PlanThemeEnding ending) {
    GrowRequest request;
    memset(&request, 0, sizeof(request));
    request.motive = motive;
    request.key = plan_key_of_pitch_class(0, KEY_STRUCTURE_MODE_MAJOR);
    request.meter = meter_of(scenario);
    request.bars = bars;
    request.pairing.beginning = beginning;
    request.pairing.ending = ending;
    request.cadence = CADENCE_TYPE_PERFECT_AUTHENTIC;
    return request;
}

/* A 9-bar sentence with no construction builds the 4+5 one, with its reason, and grow_run reports why. */
static void test_build(void) {
    const Scenario *scenario = &SCENARIOS[0];
    Motive one = make_motive(scenario, scenario->one);
    GrowRequest request = request_of(scenario, &one, 9, SENTENCE);
    PiecePlan *plan = NULL;
    assert(grow_build_plan(&request, &plan) == GROW_OK);
    const PlanConstraint *constraint = plan_node_find_constraint(plan, find_node(plan, "theme"), PLAN_CONSTRAINT_CONSTRUCTION);
    assert(constraint);
    expect_construction(&constraint->params.construction, &ROWS[4], "plan");
    GrowConfig config;
    GrowOutcome outcome;
    memset(&config, 0, sizeof(config));
    config.seed = 1;
    GrowResult result = grow_run(plan, &config, &outcome);
    assert(result == GROW_OK && !outcome.stuck);
    assert(strcmp(outcome.construction_text, ROWS[4].text) == 0);
    grow_outcome_destroy(&outcome);
    plan_destroy(plan);

    /* A given construction keeps its reason and is described as given. */
    PlanConstruction given;
    memset(&given, 0, sizeof(given));
    memcpy(given.idea_bars, ROWS[4].ideas, sizeof(given.idea_bars));
    given.extension_count = 1;
    given.extensions[0].kind = PLAN_EXTENSION_REPEAT;
    given.extensions[0].idea = GROW_IDEA_FRAGMENT_2;
    given.extensions[0].bars = 1;
    given.reason = PLAN_CONSTRUCTION_GIVEN;
    request.construction = &given;
    assert(grow_build_plan(&request, &plan) == GROW_OK);
    NodeId theme = find_node(plan, "theme");
    constraint = plan_node_find_constraint(plan, theme, PLAN_CONSTRAINT_CONSTRUCTION);
    assert(constraint && constraint->params.construction.reason == PLAN_CONSTRUCTION_GIVEN);
    char text[192];
    assert(grow_section_describe(plan, theme, text, sizeof(text)) > 0);
    assert(strcmp(text, "4+5 sentence as given: fragment 2 is repeated a step lower (bar 7).") == 0);
    plan_destroy(plan);
}

static void expect_refusal(const GrowRequest *request, const char *message) {
    PiecePlan *plan = NULL;
    GrowRefusal refusal;
    GrowResult result = grow_build_plan_checked(request, &plan, &refusal);
    if (result != GROW_ERR_BAD_REQUEST || strcmp(refusal.path, "/bars") != 0 || strcmp(refusal.message, message) != 0) {
        fprintf(stderr, "wanted /bars: %s, got %d %s: %s\n", message, (int)result, refusal.path, refusal.message);
    }
    assert(result == GROW_ERR_BAD_REQUEST && plan == NULL);
    assert(strcmp(refusal.path, "/bars") == 0 && strcmp(refusal.message, message) == 0);
}

static void test_refusals(void) {
    const Scenario *scenario = &SCENARIOS[0];
    Motive one = make_motive(scenario, scenario->one);
    Motive two = make_motive(scenario, scenario->two);
    GrowRequest request = request_of(scenario, &one, 2, PERIOD);
    expect_refusal(&request, "no construction makes a 2-bar period from this motif");
    request = request_of(scenario, &one, 17, SENTENCE);
    expect_refusal(&request, "no construction makes a 17-bar sentence from this motif");
    request = request_of(scenario, &two, 5, PERIOD);
    expect_refusal(&request, "no construction makes a 5-bar period from this motif");
    request = request_of(scenario, &one, 6, PLAN_THEME_BEGINNING_COMPOUND_BASIC_IDEA, PLAN_THEME_ENDING_CONTINUATION);
    expect_refusal(&request, "no construction makes a 6-bar hybrid from this motif");
    /* 4 and 8 bars with no construction are as before: no refusal of the length. */
    request = request_of(scenario, &one, 8, SENTENCE);
    PiecePlan *plan = NULL;
    assert(grow_build_plan(&request, &plan) == GROW_OK);
    assert(plan_node_find_constraint(plan, find_node(plan, "theme"), PLAN_CONSTRAINT_CONSTRUCTION) == NULL);
    plan_destroy(plan);
}

int main(void) {
    test_rows();
    test_none();
    test_build();
    test_refusals();
    puts("ok");
    return 0;
}
