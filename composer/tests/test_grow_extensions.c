#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <ursatz_composer/grow.h>
#include <ursatz_composer/plan_state.h>
#include <ursatz_composer/theme_shape.h>

#include "grow/grow_internal.h"

/*
 * The four extensions of a theme (R-468, B3 of P47): internal repetition, a stretched cadence, an addition after the
 * cadence and an evaded cadence, on a construction. From the motives of tests/test_grow_uneven.c, in C major 4/4 and
 * A minor 3/4, seeds 1 to 10 each, five themes are grown: an 8-bar sentence with a repeat of fragment 2 (sequential, 9
 * bars), with a stretched cadential idea (9 bars), with an addition of 2 bars (10 bars) and with an evaded cadence (10
 * bars), and a 4+6 period by a varied repeat of the consequent contrasting idea (10 bars). Each plan's idea nodes lie
 * where the construction puts them; a repeat's melody is a step below the repeated idea's (sequential) or a different one
 * of the same contour (varied); an evaded cadence is a V then a vi at the end of the idea, and the last chord the theme's
 * cadence; an addition is a tonic from its first chord to its last.
 *
 * With the argument "fast" it runs seed 1 of each case, the layouts and the refusals. With none it runs every seed,
 * prints how many of the ten compose for each case and asserts the counts recorded below.
 */

typedef struct {
    const char *name;
    int tonic;
    KeyStructureMode mode;
    int beats;
    int unit;
    const char *motive; /* one bar */
} Scenario;

static const Scenario SCENARIOS[] = {
    {"C major 4/4", 0, KEY_STRUCTURE_MODE_MAJOR, 4, 4, "E4:q G4:e F4:e D4:q C4:q"},
    {"A minor 3/4", 9, KEY_STRUCTURE_MODE_MINOR, 3, 4, "C5:q E5:e D5:e B4:q"},
};
#define SCENARIO_COUNT (sizeof(SCENARIOS) / sizeof(SCENARIOS[0]))

typedef enum { CHECK_SEQUENCE, CHECK_STRETCH, CHECK_ADDITION, CHECK_EVADED, CHECK_VARIED } Check;

typedef struct {
    const char *name;
    int bar; /* the bar a node starts in, then the bars it has */
    int bars;
} Span;

typedef struct {
    const char *name;
    PlanThemeBeginning beginning;
    PlanThemeEnding ending;
    unsigned char ideas[GROW_IDEA_COUNT]; /* by GrowIdea */
    PlanExtension extension;
    size_t bars;     /* the theme */
    Check check;
    Span spans[12];  /* the nodes of the theme, 0 bars ends the list */
} Case;

#define PERIOD PLAN_THEME_BEGINNING_ANTECEDENT, PLAN_THEME_ENDING_CONSEQUENT
#define SENTENCE PLAN_THEME_BEGINNING_PRESENTATION, PLAN_THEME_ENDING_CONTINUATION

/* Ideas in GrowIdea order: basic, repetition, contrasting, consequent basic, consequent contrasting, fragment 1, fragment 2,
   cadential, cadential 1, cadential 2. */
static const Case CASES[] = {
    {"sentence, repeat of fragment 2", SENTENCE, {2, 2, 0, 0, 0, 1, 1, 2, 0, 0},
     {PLAN_EXTENSION_REPEAT, GROW_IDEA_FRAGMENT_2, 0, false}, 9, CHECK_SEQUENCE,
     {{"basic idea", 0, 2}, {"repetition", 2, 2}, {"fragment 1", 4, 1}, {"fragment 2", 5, 1},
      {"fragment 2 repeated", 6, 1}, {"cadential", 7, 2}, {NULL, 0, 0}}},
    {"sentence, stretched cadential idea", SENTENCE, {2, 2, 0, 0, 0, 1, 1, 2, 0, 0},
     {PLAN_EXTENSION_STRETCH, GROW_IDEA_CADENTIAL, 1, false}, 9, CHECK_STRETCH,
     {{"basic idea", 0, 2}, {"repetition", 2, 2}, {"fragment 1", 4, 1}, {"fragment 2", 5, 1}, {"cadential", 6, 3},
      {NULL, 0, 0}}},
    {"sentence, addition of 2 bars", SENTENCE, {2, 2, 0, 0, 0, 1, 1, 2, 0, 0},
     {PLAN_EXTENSION_ADDITION, 0, 2, false}, 10, CHECK_ADDITION,
     {{"basic idea", 0, 2}, {"repetition", 2, 2}, {"fragment 1", 4, 1}, {"fragment 2", 5, 1}, {"cadential", 6, 2},
      {"addition", 8, 2}, {"addition idea", 8, 2}, {NULL, 0, 0}}},
    {"sentence, evaded cadence", SENTENCE, {2, 2, 0, 0, 0, 1, 1, 2, 0, 0},
     {PLAN_EXTENSION_EVADED, 0, 0, false}, 10, CHECK_EVADED,
     {{"basic idea", 0, 2}, {"repetition", 2, 2}, {"fragment 1", 4, 1}, {"fragment 2", 5, 1}, {"cadential", 6, 2},
      {"cadential after the evaded cadence", 8, 2}, {NULL, 0, 0}}},
    {"4+6 period, varied repeat of the consequent contrasting idea", PERIOD, {2, 0, 2, 2, 2, 0, 0, 0, 0, 0},
     {PLAN_EXTENSION_REPEAT, GROW_IDEA_CONSEQUENT_CONTRASTING, 2, true}, 10, CHECK_VARIED,
     {{"basic idea", 0, 2}, {"contrasting idea", 2, 2}, {"consequent basic idea", 4, 2},
      {"consequent contrasting idea", 6, 2}, {"consequent contrasting idea repeated", 8, 2}, {NULL, 0, 0}}},
};
#define CASE_COUNT (sizeof(CASES) / sizeof(CASES[0]))

/* The seeds of ten that compose, per case and scenario, as measured: every one. */
#define COMPOSED_OF_TEN 10

static Motive make_motive(const Scenario *scenario) {
    Motive motive;
    MotiveOptions options;
    memset(&options, 0, sizeof(options));
    options.has_meter = true;
    options.meter.beats_per_bar = scenario->beats;
    options.meter.beat_unit = scenario->unit;
    assert(motive_from_notes_text(scenario->motive, &options, &motive, NULL) == MOTIVE_OK);
    return motive;
}

static PlanConstruction construction_of(const Case *item) {
    PlanConstruction construction;
    memset(&construction, 0, sizeof(construction));
    memcpy(construction.idea_bars, item->ideas, sizeof(construction.idea_bars));
    construction.extension_count = 1;
    construction.extensions[0] = item->extension;
    construction.reason = PLAN_CONSTRUCTION_GIVEN;
    return construction;
}

static GrowRequest make_request(const Scenario *scenario, const Motive *motive, const Case *item,
                                const PlanConstruction *construction) {
    GrowRequest request;
    memset(&request, 0, sizeof(request));
    request.motive = motive;
    request.key = plan_key_of_pitch_class(scenario->tonic, scenario->mode);
    request.meter.beats_per_bar = scenario->beats;
    request.meter.beat_unit = scenario->unit;
    request.bars = item->bars;
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

/* The section is as long as all the bars and each node lies where the case puts it, in whole bars. */
static void check_spans(const PiecePlan *plan, const Scenario *scenario, const Case *item) {
    NodeId theme = find_node(plan, "theme");
    assert(theme != PLAN_NODE_NONE && plan_node_state(plan, theme) == PLAN_STATE_REALIZED);
    assert(whole_bars(plan_node_span(plan, theme).length, scenario) == item->bars);
    for (const Span *want = item->spans; want->name; want++) {
        NodeId node = find_node(plan, want->name);
        if (node == PLAN_NODE_NONE) {
            fprintf(stderr, "%s, %s: no node %s\n", scenario->name, item->name, want->name);
        }
        assert(node != PLAN_NODE_NONE);
        PlanSpan span = plan_node_span(plan, node);
        if (whole_bars(span.start, scenario) != (size_t)want->bar || whole_bars(span.length, scenario) != (size_t)want->bars) {
            fprintf(stderr, "%s, %s: %s at bar %zu for %zu, wanted %d for %d\n", scenario->name, item->name, want->name,
                    whole_bars(span.start, scenario), whole_bars(span.length, scenario), want->bar, want->bars);
        }
        assert(whole_bars(span.start, scenario) == (size_t)want->bar && whole_bars(span.length, scenario) == (size_t)want->bars);
    }
}

/* The notes of the motive node's own melody span. */
static size_t node_notes(const PiecePlan *plan, NodeId node, const PlanNote **notes) {
    LaneSpanId ids[4];
    size_t found = plan_node_lane_spans(plan, node, ids, 4);
    for (size_t i = 0; i < found && i < 4; i++) {
        if (plan_lane_span_lane(plan, ids[i]) == PLAN_LANE_MELODY) {
            const PlanNotes *payload = &plan_lane_span_payload(plan, ids[i])->notes;
            *notes = payload->notes;
            return payload->note_count;
        }
    }
    return 0;
}

/* The motive node the melody of the idea asks for. */
static NodeId asked_motive(const PiecePlan *plan, NodeId idea) {
    LaneSpanId ids[4];
    size_t found = plan_node_lane_spans(plan, idea, ids, 4);
    for (size_t i = 0; i < found && i < 4; i++) {
        if (plan_lane_span_lane(plan, ids[i]) == PLAN_LANE_MELODY) {
            const PlanConstraint *constraint = plan_lane_span_find_constraint(plan, ids[i], PLAN_CONSTRAINT_MOTIVE_REF);
            return constraint ? constraint->params.motive.motive : PLAN_NODE_NONE;
        }
    }
    return PLAN_NODE_NONE;
}

/* The last note of the melody of the idea, as a pitch class. */
static int last_pitch_class(const PiecePlan *plan, NodeId idea) {
    LaneSpanId ids[4];
    size_t found = plan_node_lane_spans(plan, idea, ids, 4);
    for (size_t i = 0; i < found && i < 4; i++) {
        if (plan_lane_span_lane(plan, ids[i]) == PLAN_LANE_MELODY) {
            const PlanNotes *payload = &plan_lane_span_payload(plan, ids[i])->notes;
            assert(payload->note_count > 0);
            return ((payload->notes[payload->note_count - 1].midi % 12) + 12) % 12;
        }
    }
    return -1;
}

/* A repeat's motive against the one of the idea it repeats: a step lower, note for note (sequential), or another of the same contour. */
static void check_repeat(const PiecePlan *plan, const char *idea_name, const char *repeat_name, bool varied) {
    NodeId idea = find_node(plan, idea_name);
    NodeId repeat = find_node(plan, repeat_name);
    NodeId extension = find_node(plan, "extension 1");
    assert(idea != PLAN_NODE_NONE && repeat != PLAN_NODE_NONE && extension != PLAN_NODE_NONE);
    /* The repeat asks for the extension's motive; the idea for the motive node of its own. */
    assert(asked_motive(plan, repeat) == extension);
    const PlanNote *original = NULL;
    const PlanNote *made = NULL;
    size_t original_count = node_notes(plan, asked_motive(plan, idea), &original);
    size_t made_count = node_notes(plan, extension, &made);
    assert(original_count > 0 && made_count == original_count);
    bool differs = false;
    for (size_t i = 0; i < made_count; i++) {
        assert(rational_cmp(made[i].offset, original[i].offset) == 0 && rational_cmp(made[i].duration, original[i].duration) == 0);
        int down = original[i].midi - made[i].midi;
        differs = differs || down != 0;
        if (!varied) {
            assert(down >= 1 && down <= 2);
        }
        if (i > 0 && varied) {
            /* The same contour: each interval goes the way it did. */
            int before = original[i].midi - original[i - 1].midi;
            int after = made[i].midi - made[i - 1].midi;
            assert((before > 0) == (after > 0) && (before < 0) == (after < 0));
        }
    }
    assert(differs);
}

/* Whether the character is one of the set (the end of the numeral is none). */
static bool one_of(const char *set, char c) {
    return c != '\0' && strchr(set, c) != NULL;
}

/* Whether a Roman numeral is a tonic, the dominant, or the submediant (vi). */
static bool is_tonic(const char *numeral) {
    return (numeral[0] == 'I' || numeral[0] == 'i') && !one_of("IiVv", numeral[1]);
}

static bool is_dominant(const char *numeral) {
    return (numeral[0] == 'V' || numeral[0] == 'v') && !one_of("Ii", numeral[1]);
}

static bool is_submediant(const char *numeral) {
    return (numeral[0] == 'V' || numeral[0] == 'v') && (numeral[1] == 'I' || numeral[1] == 'i') &&
           !one_of("Ii", numeral[2]);
}

/* The chords that sound within the span, in order. */
static size_t chords_in(const PiecePlan *plan, PlanSpan span, const PlanHarmony **chords, size_t capacity) {
    LaneSpanId ids[32];
    size_t found = plan_lane_find(plan, PLAN_LANE_HARMONY, 0, span, ids, 32);
    size_t count = 0;
    for (size_t i = 0; i < found && i < 32 && count < capacity; i++) {
        chords[count++] = &plan_lane_span_payload(plan, ids[i])->harmony;
    }
    return count;
}

/* A V then a vi at the end of the idea before the evaded cadence, the theme's cadence at the end of the idea after it. */
static void check_evaded(const PiecePlan *plan, const Scenario *scenario) {
    NodeId idea = find_node(plan, "cadential");
    NodeId after = find_node(plan, "cadential after the evaded cadence");
    const PlanHarmony *chords[32];
    PlanSpan span = plan_node_span(plan, idea);
    size_t count = chords_in(plan, span, chords, 32);
    assert(count >= 2 && is_submediant(chords[count - 1]->roman_numeral) && is_dominant(chords[count - 2]->roman_numeral));
    span = plan_node_span(plan, after);
    count = chords_in(plan, span, chords, 32);
    assert(count >= 2 && is_tonic(chords[count - 1]->roman_numeral) && is_dominant(chords[count - 2]->roman_numeral));
    /* The melody of the evaded idea ends on the tonic; that of the idea before it on a tone of vi (the tonic or the third). */
    assert(last_pitch_class(plan, after) == scenario->tonic);
    int third = (scenario->tonic + (scenario->mode == KEY_STRUCTURE_MODE_MINOR ? 3 : 4)) % 12;
    int before = last_pitch_class(plan, idea);
    assert(before == scenario->tonic || before == third);
}

/* The addition's chords go from a tonic to a tonic. */
static void check_addition(const PiecePlan *plan, const Scenario *scenario) {
    NodeId addition = find_node(plan, "addition");
    const PlanHarmony *chords[32];
    size_t count = chords_in(plan, plan_node_span(plan, addition), chords, 32);
    assert(count >= 1 && is_tonic(chords[0]->roman_numeral) && is_tonic(chords[count - 1]->roman_numeral));
    /* Its melody ends on the cadence's tone, the tonic. */
    assert(last_pitch_class(plan, find_node(plan, "addition idea")) == scenario->tonic);
}

static void check_extension(const PiecePlan *plan, const Scenario *scenario, const Case *item) {
    switch (item->check) {
    case CHECK_SEQUENCE:
        check_repeat(plan, "fragment 2", "fragment 2 repeated", false);
        break;
    case CHECK_VARIED:
        check_repeat(plan, "consequent contrasting idea", "consequent contrasting idea repeated", true);
        break;
    case CHECK_STRETCH:
        /* No idea of its own: the cadential idea is a bar longer (check_spans). */
        assert(find_node(plan, "extension 1") == PLAN_NODE_NONE);
        break;
    case CHECK_ADDITION:
        check_addition(plan, scenario);
        break;
    case CHECK_EVADED:
        check_evaded(plan, scenario);
        break;
    }
}

/* The plan carries the construction as given, extension included. */
static void check_constraint(const PiecePlan *plan, const PlanConstruction *wanted) {
    const PlanConstraint *constraint = plan_node_find_constraint(plan, find_node(plan, "theme"), PLAN_CONSTRAINT_CONSTRUCTION);
    assert(constraint);
    assert(memcmp(constraint->params.construction.idea_bars, wanted->idea_bars, sizeof(wanted->idea_bars)) == 0);
    assert(constraint->params.construction.extension_count == wanted->extension_count);
    assert(constraint->params.construction.extensions[0].kind == wanted->extensions[0].kind);
}

/* Grows one case from one seed; whether it composed. With `check`, the plan is held to the case. */
static bool run_case(const Scenario *scenario, const Case *item, unsigned long seed, bool check) {
    Motive motive = make_motive(scenario);
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
        check_extension(plan, scenario, item);
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

/* The layout of an 8-bar sentence with each extension: where the ideas and the extensions lie, in bars of a 4/4 bar. */
static void check_layout_of(const Case *item, const size_t *starts, const size_t *lengths, size_t beginning, size_t ending) {
    GrowThemeRequest request;
    GrowLayout layout;
    PlanConstruction construction = construction_of(item);
    memset(&request, 0, sizeof(request));
    request.bar = rational_ratio(1, 1);
    request.bars = item->bars;
    request.pairing.beginning = item->beginning;
    request.pairing.ending = item->ending;
    request.cadence = CADENCE_TYPE_PERFECT_AUTHENTIC;
    request.has_construction = true;
    request.construction = construction;
    assert(grow_layout_of(&request, &layout));
    for (size_t place = GROW_PLACE_BASIC; place <= GROW_PLACE_CADENTIAL; place++) {
        assert(rational_cmp(layout.start[place], rational_ratio((long long)starts[place], 1)) == 0);
        assert(rational_cmp(layout.length[place], rational_ratio((long long)lengths[place], 1)) == 0);
    }
    assert(layout.beginning_bars == beginning && layout.ending_bars == ending);
    /* The soft rules are asked on the construction's ideas alone. */
    assert(layout.idea_beginning_bars == 4 && layout.idea_ending_bars == 4);
    assert(layout.extension_count == 1 && layout.extensions[0].kind == item->extension.kind);
}

static void test_layouts(void) {
    /* Places: basic, repetition, fragment 1, fragment 2, cadential. */
    static const size_t REPEAT_STARTS[5] = {0, 2, 4, 5, 7};
    static const size_t REPEAT_LENGTHS[5] = {2, 2, 1, 1, 2};
    check_layout_of(&CASES[0], REPEAT_STARTS, REPEAT_LENGTHS, 4, 5);
    static const size_t STRETCH_STARTS[5] = {0, 2, 4, 5, 6};
    static const size_t STRETCH_LENGTHS[5] = {2, 2, 1, 1, 3};
    check_layout_of(&CASES[1], STRETCH_STARTS, STRETCH_LENGTHS, 4, 5);
    static const size_t PLAIN_LENGTHS[5] = {2, 2, 1, 1, 2};
    check_layout_of(&CASES[2], STRETCH_STARTS, PLAIN_LENGTHS, 4, 4);
    check_layout_of(&CASES[3], STRETCH_STARTS, PLAIN_LENGTHS, 4, 6);

    /* The extensions as laid out: the repeat follows fragment 2, the addition is after the ending, the evaded idea in it. */
    GrowThemeRequest request;
    GrowLayout layout;
    memset(&request, 0, sizeof(request));
    request.bar = rational_ratio(1, 1);
    request.bars = 10;
    request.pairing.beginning = PLAN_THEME_BEGINNING_PRESENTATION;
    request.pairing.ending = PLAN_THEME_ENDING_CONTINUATION;
    request.cadence = CADENCE_TYPE_PERFECT_AUTHENTIC;
    request.has_construction = true;
    request.construction = construction_of(&CASES[2]);
    request.construction.extension_count = 2;
    request.construction.extensions[0] = (PlanExtension){PLAN_EXTENSION_ADDITION, 0, 0, false};
    request.construction.extensions[1] = (PlanExtension){PLAN_EXTENSION_EVADED, 0, 0, false};
    request.bars = 11;
    assert(grow_layout_of(&request, &layout));
    /* The evaded idea (the cadential idea's 2 bars) is before the 1-bar addition, whatever their order in the construction. */
    assert(layout.ending_bars == 6 && layout.main_ending_bars == 4);
    assert(rational_cmp(layout.extensions[0].start, rational_ratio(10, 1)) == 0);
    assert(rational_cmp(layout.extensions[0].length, rational_ratio(1, 1)) == 0);
    assert(rational_cmp(layout.extensions[1].start, rational_ratio(8, 1)) == 0);
    assert(rational_cmp(layout.extensions[1].length, rational_ratio(2, 1)) == 0);
    /* A stretch and a repeat before it: the repeat takes the stretched idea's bars. */
    request.construction.extension_count = 2;
    request.construction.extensions[0] = (PlanExtension){PLAN_EXTENSION_STRETCH, GROW_IDEA_FRAGMENT_2, 0, false};
    request.construction.extensions[1] = (PlanExtension){PLAN_EXTENSION_REPEAT, GROW_IDEA_FRAGMENT_2, 0, false};
    request.bars = 8 + 1 + 2;
    assert(!grow_layout_of(&request, &layout)); /* a stretch cannot lengthen fragment 2 */
    request.construction.extensions[0] = (PlanExtension){PLAN_EXTENSION_STRETCH, GROW_IDEA_CADENTIAL, 1, false};
    request.construction.extensions[1] = (PlanExtension){PLAN_EXTENSION_REPEAT, GROW_IDEA_FRAGMENT_2, 0, false};
    request.bars = 8 + 1 + 1;
    assert(grow_layout_of(&request, &layout));
    assert(rational_cmp(layout.start[GROW_PLACE_CADENTIAL], rational_ratio(7, 1)) == 0);
    assert(rational_cmp(layout.length[GROW_PLACE_CADENTIAL], rational_ratio(3, 1)) == 0);
    assert(rational_cmp(layout.extensions[1].start, rational_ratio(6, 1)) == 0);
    /* A second evaded cadence, an addition of 3 bars and an HC to evade have no layout. */
    request.construction.extension_count = 2;
    request.construction.extensions[0] = (PlanExtension){PLAN_EXTENSION_EVADED, 0, 0, false};
    request.construction.extensions[1] = (PlanExtension){PLAN_EXTENSION_EVADED, 0, 0, false};
    request.bars = 12;
    assert(!grow_layout_of(&request, &layout));
    request.construction.extension_count = 1;
    request.construction.extensions[0] = (PlanExtension){PLAN_EXTENSION_ADDITION, 0, 3, false};
    request.bars = 11;
    assert(!grow_layout_of(&request, &layout));
    request.construction.extensions[0] = (PlanExtension){PLAN_EXTENSION_EVADED, 0, 0, false};
    request.bars = 10;
    request.cadence = CADENCE_TYPE_HALF;
    assert(!grow_layout_of(&request, &layout));
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
    Motive motive = make_motive(scenario);
    PlanConstruction construction = construction_of(&CASES[0]);
    GrowRequest request = make_request(scenario, &motive, &CASES[0], &construction);
    PiecePlan *plan = NULL;
    assert(grow_build_plan(&request, &plan) == GROW_OK);
    plan_destroy(plan);

    /* An extension after an idea it cannot follow, or one the pairing does not have. */
    PlanConstruction other = construction;
    GrowRequest changed = request;
    changed.construction = &other;
    other.extensions[0] = (PlanExtension){PLAN_EXTENSION_REPEAT, GROW_IDEA_BASIC, 0, false};
    expect_refusal(&changed, "/construction/extensions/0", "a repeat cannot follow the basic idea");
    other.extensions[0] = (PlanExtension){PLAN_EXTENSION_REPEAT, GROW_IDEA_CADENTIAL, 0, false};
    expect_refusal(&changed, "/construction/extensions/0", "a repeat cannot follow the cadential");
    other.extensions[0] = (PlanExtension){PLAN_EXTENSION_REPEAT, GROW_IDEA_CONTRASTING, 0, false};
    expect_refusal(&changed, "/construction/extensions/0", "the presentation+continuation has no contrasting idea");
    other.extensions[0] = (PlanExtension){PLAN_EXTENSION_STRETCH, GROW_IDEA_FRAGMENT_1, 1, false};
    expect_refusal(&changed, "/construction/extensions/0", "a stretch cannot follow the fragment 1");
    other.extensions[0] = (PlanExtension){PLAN_EXTENSION_REPEAT, GROW_IDEA_FRAGMENT_2, 0, false};
    changed.bars = 10;
    expect_refusal(&changed, "/construction/ideas", "the ideas are 9 bars; the theme has 10");

    /* An addition or an evaded cadence follows a PAC or an IAC, there is one of each, and an addition has 2 bars at most. */
    changed = request;
    changed.construction = &other;
    changed.cadence = CADENCE_TYPE_HALF;
    other.extensions[0] = (PlanExtension){PLAN_EXTENSION_EVADED, 0, 0, false};
    changed.bars = 10;
    expect_refusal(&changed, "/construction/extensions/0", "an HC cannot be evaded");
    other.extensions[0] = (PlanExtension){PLAN_EXTENSION_ADDITION, 0, 1, false};
    changed.bars = 9;
    expect_refusal(&changed, "/construction/extensions/0", "an addition follows a PAC or an IAC");
    changed.cadence = CADENCE_TYPE_PERFECT_AUTHENTIC;
    other.extension_count = 2;
    other.extensions[0] = (PlanExtension){PLAN_EXTENSION_EVADED, 0, 0, false};
    other.extensions[1] = (PlanExtension){PLAN_EXTENSION_EVADED, 0, 0, false};
    changed.bars = 12;
    expect_refusal(&changed, "/construction/extensions/1", "a theme has one evaded cadence");
    other.extensions[0] = (PlanExtension){PLAN_EXTENSION_ADDITION, 0, 1, false};
    other.extensions[1] = (PlanExtension){PLAN_EXTENSION_ADDITION, 0, 1, false};
    changed.bars = 10;
    expect_refusal(&changed, "/construction/extensions/1", "a theme has one addition");
    other.extension_count = 1;
    other.extensions[0] = (PlanExtension){PLAN_EXTENSION_ADDITION, 0, 3, false};
    changed.bars = 11;
    expect_refusal(&changed, "/construction/extensions/0", "an addition is 2 bars at most");
    /* An evaded idea of one bar has no room for the cadence. */
    other.extensions[0] = (PlanExtension){PLAN_EXTENSION_EVADED, 0, 1, false};
    changed.bars = 9;
    expect_refusal(&changed, "/bars", "9 bars leave no room for the cadence");
}

int main(int argc, char **argv) {
    if (argc == 2 && strcmp(argv[1], "fast") == 0) {
        test_layouts();
        test_refusals();
        test_fast();
        puts("test_grow_extensions fast: ok");
    } else {
        test_rates();
        puts("test_grow_extensions: ok");
    }
    return 0;
}
