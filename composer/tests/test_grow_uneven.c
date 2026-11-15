#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <ursatz_composer/grow.h>
#include <ursatz_composer/plan_json.h>
#include <ursatz_composer/plan_state.h>
#include <ursatz_composer/theme_shape.h>

/*
 * Uneven themes (R-468, R-471, B2 of P47): a theme of 3 to 16 bars whose ideas are any whole bars, given by a
 * construction. From the motives of tests/test_grow_period.c (a 3-bar motive for the 5+5 period and the 6+5 sentence), in
 * C major 4/4 and A minor 3/4, seeds 1 to 10 each, six constructions are grown: a 3+3 period (ideas 2,1,2,1), a 5+5
 * period (3,2,3,2), a 4+6 period (2,2,2,4), a 4+5 sentence (2,2,1,1,3), a 5+4 sentence (2,3,1,1,2) and a 6+5 sentence
 * (3,3,1,1,3). Each plan's idea nodes must have exactly the spans the construction gives and the section the bars of all
 * of them; in the 5+5 period the contrasting ideas, shorter than the motive, are made from shortened copies of it.
 *
 * With the argument "fast" it runs seed 1 of each case, the refusals of the request and the plan round trip. With none it
 * runs every seed, prints how many of the ten compose for each case and asserts the counts recorded below.
 */

typedef struct {
    const char *name;
    int tonic;
    KeyStructureMode mode;
    int beats;
    int unit;
    const char *motive;       /* one bar */
    const char *long_motive;  /* three bars */
} Scenario;

static const Scenario SCENARIOS[] = {
    {"C major 4/4", 0, KEY_STRUCTURE_MODE_MAJOR, 4, 4, "E4:q G4:e F4:e D4:q C4:q",
     "E4:q G4:e F4:e D4:q C4:q E4:q G4:q A4:q G4:q F4:q D4:q E4:h"},
    {"A minor 3/4", 9, KEY_STRUCTURE_MODE_MINOR, 3, 4, "C5:q E5:e D5:e B4:q",
     "C5:q E5:e D5:e B4:q A4:q C5:q B4:q G#4:q A4:h"},
};
#define SCENARIO_COUNT (sizeof(SCENARIOS) / sizeof(SCENARIOS[0]))

typedef struct {
    const char *name;
    PlanThemeBeginning beginning;
    PlanThemeEnding ending;
    unsigned char ideas[GROW_IDEA_COUNT]; /* by GrowIdea */
    bool long_motive;                     /* the basic motive is three bars */
} Case;

#define PERIOD PLAN_THEME_BEGINNING_ANTECEDENT, PLAN_THEME_ENDING_CONSEQUENT
#define SENTENCE PLAN_THEME_BEGINNING_PRESENTATION, PLAN_THEME_ENDING_CONTINUATION

/* Ideas in GrowIdea order: basic, repetition, contrasting, consequent basic, consequent contrasting, fragment 1, fragment 2,
   cadential, cadential 1, cadential 2. */
static const Case CASES[] = {
    {"3+3 period", PERIOD, {2, 0, 1, 2, 1, 0, 0, 0, 0, 0}, false},
    {"5+5 period", PERIOD, {3, 0, 2, 3, 2, 0, 0, 0, 0, 0}, true},
    {"4+6 period", PERIOD, {2, 0, 2, 2, 4, 0, 0, 0, 0, 0}, false},
    {"4+5 sentence", SENTENCE, {2, 2, 0, 0, 0, 1, 1, 3, 0, 0}, false},
    {"5+4 sentence", SENTENCE, {2, 3, 0, 0, 0, 1, 1, 2, 0, 0}, false},
    {"6+5 sentence", SENTENCE, {3, 3, 0, 0, 0, 1, 1, 3, 0, 0}, true},
};
#define CASE_COUNT (sizeof(CASES) / sizeof(CASES[0]))

/* The seeds of ten that compose, per case and scenario, as measured (R-471): every one. */
#define COMPOSED_OF_TEN 10

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

static PlanConstruction construction_of(const Case *item) {
    PlanConstruction construction;
    memset(&construction, 0, sizeof(construction));
    memcpy(construction.idea_bars, item->ideas, sizeof(construction.idea_bars));
    construction.reason = PLAN_CONSTRUCTION_GIVEN;
    return construction;
}

static size_t bars_of(const Case *item) {
    size_t bars = 0;
    for (int idea = 0; idea < GROW_IDEA_COUNT; idea++) {
        bars += item->ideas[idea];
    }
    return bars;
}

static GrowRequest make_request(const Scenario *scenario, const Motive *motive, const Case *item,
                                const PlanConstruction *construction) {
    GrowRequest request;
    memset(&request, 0, sizeof(request));
    request.motive = motive;
    request.key = plan_key_of_pitch_class(scenario->tonic, scenario->mode);
    request.meter.beats_per_bar = scenario->beats;
    request.meter.beat_unit = scenario->unit;
    request.bars = bars_of(item);
    request.pairing.beginning = item->beginning;
    request.pairing.ending = item->ending;
    request.cadence = CADENCE_TYPE_PERFECT_AUTHENTIC;
    request.construction = construction;
    return request;
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

/* The length of a span in bars of the scenario's meter; the span must be whole bars. */
static size_t whole_bars(Rational time, const Scenario *scenario) {
    Rational bar;
    Rational bars;
    assert(rational_make(scenario->beats, (long long)scenario->unit, &bar) == RATIONAL_OK);
    assert(rational_div(time, bar, &bars) == RATIONAL_OK && bars.den == 1 && bars.num >= 0);
    return (size_t)bars.num;
}

/* The section is as long as all the ideas and each idea node lies where the construction puts it, in whole bars. */
static void check_spans(const PiecePlan *plan, const Scenario *scenario, const Case *item) {
    NodeId theme = find_node(plan, "theme");
    assert(theme != PLAN_NODE_NONE && plan_node_state(plan, theme) == PLAN_STATE_REALIZED);
    assert(whole_bars(plan_node_span(plan, theme).length, scenario) == bars_of(item));
    size_t at = 0;
    size_t found = 0;
    for (int idea = 0; idea < GROW_IDEA_COUNT; idea++) {
        if (item->ideas[idea] == 0) {
            assert(find_node(plan, theme_shape_idea_name((GrowIdea)idea)) == PLAN_NODE_NONE);
            continue;
        }
        NodeId node = find_node(plan, theme_shape_idea_name((GrowIdea)idea));
        assert(node != PLAN_NODE_NONE && plan_node_kind(plan, node) == PLAN_NODE_IDEA);
        PlanSpan span = plan_node_span(plan, node);
        if (whole_bars(span.start, scenario) != at || whole_bars(span.length, scenario) != item->ideas[idea]) {
            fprintf(stderr, "%s, %s: %s at bar %zu for %zu, wanted %zu for %d\n", scenario->name, item->name,
                    theme_shape_idea_name((GrowIdea)idea), whole_bars(span.start, scenario),
                    whole_bars(span.length, scenario), at, (int)item->ideas[idea]);
        }
        assert(whole_bars(span.start, scenario) == at && whole_bars(span.length, scenario) == item->ideas[idea]);
        at += item->ideas[idea];
        found++;
    }
    assert(at == bars_of(item) && found > 0);
    /* A contrasting idea shorter than the motive is made from a shortened copy of it, at most as long as the idea. */
    static const GrowIdea CONTRASTS[2] = {GROW_IDEA_CONTRASTING, GROW_IDEA_CONSEQUENT_CONTRASTING};
    for (size_t k = 0; k < 2; k++) {
        GrowIdea idea = CONTRASTS[k];
        char name[32];
        snprintf(name, sizeof(name), "shortened for m%d", idea == GROW_IDEA_CONTRASTING ? 1 : 3);
        NodeId node = find_node(plan, name);
        if (item->long_motive && item->ideas[idea] != 0 && item->ideas[idea] < item->ideas[GROW_IDEA_BASIC]) {
            assert(node != PLAN_NODE_NONE);
            assert(whole_bars(plan_node_span(plan, node).length, scenario) <= item->ideas[idea]);
        } else {
            assert(node == PLAN_NODE_NONE);
        }
    }
}

/* The plan carries the construction as given. */
static void check_constraint(const PiecePlan *plan, const PlanConstruction *wanted) {
    const PlanConstraint *constraint = plan_node_find_constraint(plan, find_node(plan, "theme"), PLAN_CONSTRAINT_CONSTRUCTION);
    assert(constraint);
    assert(memcmp(constraint->params.construction.idea_bars, wanted->idea_bars, sizeof(wanted->idea_bars)) == 0);
    assert(constraint->params.construction.extension_count == 0);
    assert(constraint->params.construction.reason == PLAN_CONSTRUCTION_GIVEN);
}

/* Grows one case from one seed; whether it composed. With `check`, the plan's spans are held to the construction. */
static bool run_case(const Scenario *scenario, const Case *item, unsigned long seed, bool check) {
    Motive motive = make_motive(scenario, item->long_motive ? scenario->long_motive : scenario->motive);
    PlanConstruction construction = construction_of(item);
    GrowRequest request = make_request(scenario, &motive, item, &construction);
    GrowConfig config;
    PiecePlan *plan = NULL;
    GrowOutcome outcome;
    memset(&config, 0, sizeof(config));
    config.seed = seed;
    assert(grow_build_plan(&request, &plan) == GROW_OK);
    check_constraint(plan, &construction);
    GrowResult result = grow_run(plan, &config, &outcome);
    bool composed = result == GROW_OK && !outcome.stuck;
    if (composed && check) {
        check_spans(plan, scenario, item);
    }
    grow_outcome_destroy(&outcome);
    plan_destroy(plan);
    return composed;
}

static void test_fast(void) {
    for (size_t s = 0; s < SCENARIO_COUNT; s++) {
        for (size_t c = 0; c < CASE_COUNT; c++) {
            assert(run_case(&SCENARIOS[s], &CASES[c], 1, true));
        }
    }
}

static void test_rates(void) {
    for (size_t c = 0; c < CASE_COUNT; c++) {
        for (size_t s = 0; s < SCENARIO_COUNT; s++) {
            int composed = 0;
            for (unsigned long seed = 1; seed <= 10; seed++) {
                composed += run_case(&SCENARIOS[s], &CASES[c], seed, true) ? 1 : 0;
            }
            printf("%s, %s: %d of 10 compose\n", SCENARIOS[s].name, CASES[c].name, composed);
            assert(composed == COMPOSED_OF_TEN);
        }
    }
}

/* A request refused: its result, the field to blame and the message. */
static void expect_refusal(const GrowRequest *request, const char *path, const char *message) {
    PiecePlan *plan = NULL;
    GrowRefusal refusal;
    GrowResult result = grow_build_plan_checked(request, &plan, &refusal);
    if (result != GROW_ERR_BAD_REQUEST || strcmp(refusal.path, path) != 0 || strcmp(refusal.message, message) != 0) {
        fprintf(stderr, "wanted %s: %s, got %d %s: %s\n", path, message, (int)result, refusal.path, refusal.message);
    }
    assert(result == GROW_ERR_BAD_REQUEST && plan == NULL);
    assert(strcmp(refusal.path, path) == 0 && strcmp(refusal.message, message) == 0);
}

static void test_refusals(void) {
    const Scenario *scenario = &SCENARIOS[0];
    Motive motive = make_motive(scenario, scenario->motive);
    PlanConstruction construction = construction_of(&CASES[0]);
    GrowRequest request = make_request(scenario, &motive, &CASES[0], &construction);
    PiecePlan *plan = NULL;
    assert(grow_build_plan(&request, &plan) == GROW_OK);
    plan_destroy(plan);

    /* A theme without a construction has one chosen from its length (R-469), or none: then it is 4 or 8 bars. */
    GrowRequest plain = request;
    plain.construction = NULL;
    plain.bars = 6;
    assert(grow_build_plan(&plain, &plan) == GROW_OK);
    plan_destroy(plan);
    plain.bars = 2;
    expect_refusal(&plain, "/bars", "no construction makes a 2-bar period from this motif");
    GrowRequest changed = request;
    changed.bars = 2;
    expect_refusal(&changed, "/bars", "a theme has 4 or 8 bars, or 3 to 16 with a construction, not 2");
    changed.bars = 17;
    expect_refusal(&changed, "/bars", "a theme has 4 or 8 bars, or 3 to 16 with a construction, not 17");

    /* The ideas must add up to the theme, and fit the pairing. */
    changed = request;
    changed.bars = 7;
    expect_refusal(&changed, "/construction/ideas", "the ideas are 6 bars; the theme has 7");
    PlanConstruction other = construction;
    changed.construction = &other;
    changed.bars = 6;
    other.idea_bars[GROW_IDEA_FRAGMENT_1] = 1;
    expect_refusal(&changed, "/construction/ideas", "the antecedent+consequent has no fragment 1");
    other = construction;
    other.idea_bars[GROW_IDEA_CONTRASTING] = 0;
    other.idea_bars[GROW_IDEA_BASIC] = 3;
    expect_refusal(&changed, "/construction/ideas", "the contrasting idea needs a bar or more");

    /* A continuation's fragments are shorter than its basic idea: a 1-bar basic idea has no whole-bar fragment. */
    static const Case ONE_BAR = {"1+2+1+1+2 sentence", SENTENCE, {1, 2, 0, 0, 0, 1, 1, 2, 0, 0}, false};
    PlanConstruction one_bar = construction_of(&ONE_BAR);
    changed = make_request(scenario, &motive, &ONE_BAR, &one_bar);
    expect_refusal(&changed, "/construction/ideas", "the fragment 1 needs fewer bars than the basic idea");
    one_bar.idea_bars[GROW_IDEA_FRAGMENT_1] = 0;
    expect_refusal(&changed, "/construction/ideas", "the fragment 1 needs a bar or more");
    one_bar.idea_bars[GROW_IDEA_FRAGMENT_1] = 1;
    one_bar.idea_bars[GROW_IDEA_BASIC] = 2;
    one_bar.idea_bars[GROW_IDEA_CADENTIAL] = 1;
    one_bar.idea_bars[GROW_IDEA_FRAGMENT_2] = 2;
    changed.bars = 8;
    expect_refusal(&changed, "/construction/ideas", "the fragment 2 needs fewer bars than the basic idea");

    /* A motif given for an idea is held to the bars the construction gives that idea. */
    changed = request;
    Motive longer = make_motive(scenario, scenario->long_motive);
    changed.idea_motives[GROW_IDEA_CONTRASTING] = &longer;
    PiecePlan *refused = NULL;
    GrowRefusal refusal;
    assert(grow_build_plan_checked(&changed, &refused, &refusal) == GROW_ERR_BAD_REQUEST && refused == NULL);
    assert(strcmp(refusal.path, "/ideas/contrasting idea") == 0);
    assert(strstr(refusal.message, "is longer than the idea it fills"));
}

/* A plan with the constraint reads back unchanged, and the section is labelled by its halves. */
static void test_round_trip(void) {
    const Scenario *scenario = &SCENARIOS[0];
    for (size_t c = 0; c < CASE_COUNT; c++) {
        Motive motive = make_motive(scenario, CASES[c].long_motive ? scenario->long_motive : scenario->motive);
        PlanConstruction construction = construction_of(&CASES[c]);
        construction.reason = (PlanConstructionReason)(c % 3);
        GrowRequest request = make_request(scenario, &motive, &CASES[c], &construction);
        PiecePlan *plan = NULL;
        assert(grow_build_plan(&request, &plan) == GROW_OK);
        PlanJsonError error;
        char *text = plan_to_json(plan, &error);
        assert(text && strstr(text, "\"construction\""));
        PiecePlan *again = NULL;
        assert(plan_from_json(text, &again, &error) == PLAN_OK);
        char *text_again = plan_to_json(again, &error);
        assert(text_again && strcmp(text, text_again) == 0);
        const PlanConstraint *read = plan_node_find_constraint(again, find_node(again, "theme"), PLAN_CONSTRAINT_CONSTRUCTION);
        assert(read && memcmp(read->params.construction.idea_bars, construction.idea_bars, sizeof(construction.idea_bars)) == 0);
        assert(read->params.construction.reason == construction.reason);
        free(text);
        free(text_again);
        plan_destroy(plan);
        plan_destroy(again);
    }

    /* Extensions round trip too, and a constraint on anything but a section, or a refused idea, is not read. */
    PiecePlan *plan = NULL;
    Motive motive = make_motive(scenario, scenario->motive);
    PlanConstruction construction = construction_of(&CASES[3]);
    GrowRequest request = make_request(scenario, &motive, &CASES[3], &construction);
    assert(grow_build_plan(&request, &plan) == GROW_OK);
    NodeId theme = find_node(plan, "theme");
    PlanConstraint extended = {PLAN_CONSTRAINT_CONSTRUCTION, {0}};
    extended.params.construction = construction;
    extended.params.construction.extension_count = 2;
    extended.params.construction.extensions[0] = (PlanExtension){PLAN_EXTENSION_STRETCH, GROW_IDEA_CADENTIAL, 1, false};
    extended.params.construction.extensions[1] = (PlanExtension){PLAN_EXTENSION_REPEAT, GROW_IDEA_FRAGMENT_2, 2, true};
    extended.params.construction.reason = PLAN_CONSTRUCTION_FROM_LENGTH;
    PiecePlan *scratch = NULL;
    assert(grow_build_plan(&request, &scratch) == GROW_OK);
    NodeId scratch_theme = find_node(scratch, "theme");
    assert(plan_node_remove_constraint(scratch, scratch_theme, plan_node_constraint_count(scratch, scratch_theme) - 1) == PLAN_OK);
    assert(plan_node_add_constraint(scratch, scratch_theme, &extended) == PLAN_OK);
    PlanJsonError error;
    char *text = plan_to_json(scratch, &error);
    assert(text && strstr(text, "\"stretch\"") && strstr(text, "\"cadential\""));
    PiecePlan *again = NULL;
    assert(plan_from_json(text, &again, &error) == PLAN_OK);
    char *text_again = plan_to_json(again, &error);
    assert(text_again && strcmp(text, text_again) == 0);
    free(text);
    free(text_again);
    plan_destroy(again);
    plan_destroy(scratch);

    /* The constraint is only for a section node, with ideas to 16 bars and at most 3 extensions of 4 bars. */
    NodeId motive_node = PLAN_NODE_NONE;
    for (NodeId id = 1; id <= plan_node_count(plan); id++) {
        if (plan_node_kind(plan, id) == PLAN_NODE_PHRASE && plan_node_state(plan, id) != PLAN_STATE_GIVEN) {
            motive_node = id;
        }
    }
    assert(motive_node != PLAN_NODE_NONE);
    PlanConstraint bad = {PLAN_CONSTRAINT_CONSTRUCTION, {0}};
    bad.params.construction = construction;
    assert(plan_node_add_constraint(plan, motive_node, &bad) == PLAN_ERR_BAD_PAYLOAD);
    bad.params.construction.idea_bars[0] = 17;
    assert(plan_node_add_constraint(plan, theme, &bad) == PLAN_ERR_BAD_PAYLOAD);
    bad.params.construction = construction;
    bad.params.construction.extension_count = 4;
    assert(plan_node_add_constraint(plan, theme, &bad) == PLAN_ERR_BAD_PAYLOAD);
    bad.params.construction.extension_count = 1;
    bad.params.construction.extensions[0].bars = 5;
    assert(plan_node_add_constraint(plan, theme, &bad) == PLAN_ERR_BAD_PAYLOAD);
    bad.params.construction.extensions[0].bars = 1;
    bad.params.construction.extensions[0].idea = GROW_IDEA_COUNT;
    assert(plan_node_add_constraint(plan, theme, &bad) == PLAN_ERR_BAD_PAYLOAD);
    plan_destroy(plan);

    /* A plan with an unknown idea in a construction is refused with its path. */
    const char *marker = "\"basic idea\"";
    PiecePlan *good = NULL;
    assert(grow_build_plan(&request, &good) == GROW_OK);
    char *good_text = plan_to_json(good, &error);
    assert(good_text);
    char *at = strstr(good_text, "\"ideas\"");
    assert(at && strstr(at, marker));
    char *name = strstr(at, marker);
    memcpy(name + 1, "basic idxa", 10);
    PiecePlan *broken = NULL;
    assert(plan_from_json(good_text, &broken, &error) == PLAN_ERR_FORMAT && broken == NULL);
    assert(strstr(error.path, "ideas") != NULL);
    free(good_text);
    plan_destroy(good);
}

int main(int argc, char **argv) {
    if (argc == 2 && strcmp(argv[1], "fast") == 0) {
        test_refusals();
        test_round_trip();
        test_fast();
        puts("test_grow_uneven fast: ok");
    } else {
        test_rates();
        puts("test_grow_uneven: ok");
    }
    return 0;
}
