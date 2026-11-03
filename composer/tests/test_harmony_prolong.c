#include <assert.h>
#include <stdio.h>
#include <string.h>

#include <ursatz_composer/constraint.h>
#include <ursatz_composer/fill_loop.h>
#include <ursatz_composer/harmony_phrase.h>
#include <ursatz_composer/harmony_prolong.h>
#include <ursatz_composer/plan_state.h>

#include "harmony/harmony_internal.h"

/*
 * The prolongation harmony operator, run through the fill loop with its checker. Every plan is one idea over a
 * Given meter lane and key lane, with one Constrained harmony span asking for a prolongation.
 */

#define MAX_CHORDS 16

typedef struct {
    int beats_per_bar;
    int beat_unit;
    PlanKeyMode mode;
    int bars;
    PlanHarmonyShape shape;
} Scenario;

typedef struct {
    PiecePlan *plan;
    LaneSpanId target;
    PlanSpan span;
} Fixture;

static Rational fraction(long num, unsigned long den) {
    Rational time;
    assert(rational_make(num, den, &time) == RATIONAL_OK);
    return time;
}

static Fixture build(const Scenario *scenario) {
    Fixture fixture;
    NodeId owner;
    PlanLanePayload payload;
    LaneSpanId id;
    fixture.plan = plan_create();
    fixture.span.start = fraction(0, 1);
    fixture.span.length = fraction(scenario->bars * (long)scenario->beats_per_bar, (unsigned long)scenario->beat_unit);
    assert(plan_add_node(fixture.plan, PLAN_NODE_NONE, PLAN_NODE_FORM, fixture.span, NULL, &owner) == PLAN_OK);
    assert(plan_node_set_state(fixture.plan, owner, PLAN_STATE_GIVEN) == PLAN_OK);

    memset(&payload, 0, sizeof(payload));
    payload.meter.beats_per_bar = scenario->beats_per_bar;
    payload.meter.beat_unit = scenario->beat_unit;
    assert(plan_add_lane_span(fixture.plan, owner, PLAN_LANE_METER, 0, fixture.span, &payload, &id) == PLAN_OK);
    assert(plan_lane_span_set_state(fixture.plan, id, PLAN_STATE_GIVEN) == PLAN_OK);
    memset(&payload, 0, sizeof(payload));
    payload.key.tonic_pitch_class = 0;
    payload.key.mode = scenario->mode;
    assert(plan_add_lane_span(fixture.plan, owner, PLAN_LANE_KEY, 0, fixture.span, &payload, &id) == PLAN_OK);
    assert(plan_lane_span_set_state(fixture.plan, id, PLAN_STATE_GIVEN) == PLAN_OK);

    memset(&payload, 0, sizeof(payload));
    strcpy(payload.harmony.roman_numeral, "I");
    payload.harmony.has_function = false;
    assert(plan_add_lane_span(fixture.plan, owner, PLAN_LANE_HARMONY, 0, fixture.span, &payload, &fixture.target) ==
           PLAN_OK);
    PlanConstraint shape = {PLAN_CONSTRAINT_HARMONY_SHAPE, {0}};
    shape.params.harmony_shape = scenario->shape;
    assert(plan_lane_span_add_constraint(fixture.plan, fixture.target, &shape) == PLAN_OK);
    return fixture;
}

static FillOutcome run(const Fixture *fixture, unsigned long seed) {
    FillOperatorRegistry *registry = fill_registry_create();
    FillConfig config;
    FillOutcome outcome;
    memset(&config, 0, sizeof(config));
    config.seed = seed;
    config.checker = harmony_prolong_checker;
    assert(harmony_prolong_register(registry) == FILL_OK);
    assert(harmony_phrase_register(registry) == FILL_OK);
    assert(fill_run(fixture->plan, registry, &config, &outcome, NULL) == FILL_OK);
    fill_registry_destroy(registry);
    return outcome;
}

static size_t cells_per_bar_of(int beats_per_bar, int beat_unit) {
    return (beat_unit == 4 && beats_per_bar == 4) || beat_unit == 8 ? 2 : 1;
}

/* The roman numeral of the first or last chord is of the anchor's chord. */
static bool starts_on_tonic(const char *roman) {
    return strcmp(roman, "I") == 0 || strcmp(roman, "i") == 0;
}

static bool is_dominant(const char *roman) {
    return roman[0] == 'V' && roman[1] != 'I' && roman[1] != '\0' ? true : strcmp(roman, "V") == 0;
}

static void check_filled(const Scenario *scenario, const Fixture *fixture) {
    LaneSpanId ids[MAX_CHORDS];
    int soprano[MAX_CHORDS] = {0};
    PhraseChord chords[MAX_CHORDS];
    PhraseMetre metre;
    size_t read;
    size_t count = plan_lane_find(fixture->plan, PLAN_LANE_HARMONY, 0, fixture->span, ids, MAX_CHORDS);
    assert(count >= 1);

    const char *first = plan_lane_span_payload(fixture->plan, ids[0])->harmony.roman_numeral;
    const char *last = plan_lane_span_payload(fixture->plan, ids[count - 1])->harmony.roman_numeral;
    if (scenario->shape.starts_on == PLAN_HARMONY_ANCHOR_DOMINANT) {
        assert(is_dominant(first));
    } else {
        assert(starts_on_tonic(first));
    }
    if (scenario->shape.ends_on == PLAN_HARMONY_ANCHOR_DOMINANT) {
        assert(is_dominant(last));
    } else if (scenario->shape.ends_on == PLAN_HARMONY_ANCHOR_TONIC) {
        assert(starts_on_tonic(last) || strcmp(last, "I6") == 0 || strcmp(last, "i6") == 0);
    }
    /* A span that ends on a root-position V keeps it off the bar's first cell. */
    if (strcmp(last, "V") == 0) {
        PlanSpan span = plan_lane_span_span(fixture->plan, ids[count - 1]);
        Rational offset = fraction(span.start.num * (long)(scenario->beat_unit), span.start.den);
        assert(offset.num % scenario->beats_per_bar != 0 || offset.den != 1);
    }
    assert(harmony_read_lane(fixture->plan, fixture->span, scenario->mode, chords, &read, &metre));
    assert(read == count);

    /* The chords tile the span on cells, and the shape holds. */
    size_t cells[MAX_CHORDS];
    for (size_t i = 0; i < count; i++) {
        cells[i] = chords[i].cells;
    }
    assert(harmony_shape_keeps(&scenario->shape, cells, count, metre.cells_per_bar, false));
    assert(metre.cells_per_bar == cells_per_bar_of(scenario->beats_per_bar, scenario->beat_unit));

    /* No cadence is heard inside or at the end, with no soprano given. */
    HarmonyCadences found = {NULL, 0};
    assert(harmony_hear_lane_cadences(fixture->plan, fixture->span, scenario->mode, soprano, count, NULL, 0, &found));
    assert(found.count == 0);
    harmony_cadences_destroy(&found);
}

/* Whether a request can be met: one cell holds one chord, which can only be the tonic. */
static bool feasible(const Scenario *scenario) {
    size_t cells = (size_t)scenario->bars * cells_per_bar_of(scenario->beats_per_bar, scenario->beat_unit);
    bool same = scenario->shape.starts_on != PLAN_HARMONY_ANCHOR_DOMINANT &&
                scenario->shape.ends_on != PLAN_HARMONY_ANCHOR_DOMINANT;
    return cells >= 2 || same;
}

static void run_scenario(const Scenario *scenario, unsigned long seed) {
    Fixture fixture = build(scenario);
    FillOutcome outcome = run(&fixture, seed);
    if (feasible(scenario)) {
        if (outcome.status != FILL_STATUS_COMPLETE) {
            fprintf(stderr, "stuck: %d/%d mode %d bars %d %d->%d seed %lu cap %d split %d\n", scenario->beats_per_bar,
                    scenario->beat_unit, scenario->mode, scenario->bars, scenario->shape.starts_on,
                    scenario->shape.ends_on, seed, scenario->shape.max_cells_per_chord, scenario->shape.split_at_bars);
        }
        assert(outcome.status == FILL_STATUS_COMPLETE);
        check_filled(scenario, &fixture);
    } else {
        assert(outcome.status == FILL_STATUS_STUCK);
        assert(outcome.stuck_count == 1 && outcome.stuck[0].reason == FILL_STUCK_ALL_REJECTED);
    }
    fill_outcome_destroy(&outcome);
    plan_destroy(fixture.plan);
}

static void test_every_meter_and_shape(void) {
    static const int METERS[][2] = {{4, 4}, {3, 4}, {6, 8}, {2, 4}};
    static const PlanHarmonyAnchor ANCHORS[][2] = {
        {PLAN_HARMONY_ANCHOR_TONIC, PLAN_HARMONY_ANCHOR_TONIC},
        {PLAN_HARMONY_ANCHOR_TONIC, PLAN_HARMONY_ANCHOR_DOMINANT},
        {PLAN_HARMONY_ANCHOR_DOMINANT, PLAN_HARMONY_ANCHOR_TONIC},
        {PLAN_HARMONY_ANCHOR_ANY, PLAN_HARMONY_ANCHOR_ANY},
    };
    for (size_t m = 0; m < 4; m++) {
        for (int mode = 0; mode < 2; mode++) {
            for (int bars = 1; bars <= 4; bars++) {
                for (size_t a = 0; a < 4; a++) {
                    for (unsigned long seed = 1; seed <= 6; seed++) {
                        Scenario scenario;
                        memset(&scenario, 0, sizeof(scenario));
                        scenario.beats_per_bar = METERS[m][0];
                        scenario.beat_unit = METERS[m][1];
                        scenario.mode = (PlanKeyMode)mode;
                        scenario.bars = bars;
                        scenario.shape.kind = PLAN_HARMONY_SHAPE_PROLONGATION;
                        scenario.shape.starts_on = ANCHORS[a][0];
                        scenario.shape.ends_on = ANCHORS[a][1];
                        run_scenario(&scenario, seed);
                    }
                }
            }
        }
    }
}

static void test_cap_and_split(void) {
    for (unsigned long seed = 1; seed <= 12; seed++) {
        Scenario scenario;
        memset(&scenario, 0, sizeof(scenario));
        scenario.beats_per_bar = 4;
        scenario.beat_unit = 4;
        scenario.bars = 3;
        scenario.shape.kind = PLAN_HARMONY_SHAPE_PROLONGATION;
        scenario.shape.starts_on = PLAN_HARMONY_ANCHOR_TONIC;
        scenario.shape.ends_on = PLAN_HARMONY_ANCHOR_TONIC;
        scenario.shape.max_cells_per_chord = 2;
        scenario.shape.split_at_bars = true;
        run_scenario(&scenario, seed);
    }
}

static void test_same_seed_same_chords(void) {
    Scenario scenario;
    memset(&scenario, 0, sizeof(scenario));
    scenario.beats_per_bar = 4;
    scenario.beat_unit = 4;
    scenario.bars = 4;
    scenario.shape.kind = PLAN_HARMONY_SHAPE_PROLONGATION;
    char first[256] = "";
    for (int round = 0; round < 2; round++) {
        Fixture fixture = build(&scenario);
        FillOutcome outcome = run(&fixture, 5);
        assert(outcome.status == FILL_STATUS_COMPLETE);
        LaneSpanId ids[MAX_CHORDS];
        size_t count = plan_lane_find(fixture.plan, PLAN_LANE_HARMONY, 0, fixture.span, ids, MAX_CHORDS);
        char text[256] = "";
        for (size_t i = 0; i < count; i++) {
            strcat(text, plan_lane_span_payload(fixture.plan, ids[i])->harmony.roman_numeral);
            strcat(text, " ");
        }
        if (round == 0) {
            strcpy(first, text);
        } else {
            assert(strcmp(first, text) == 0);
        }
        fill_outcome_destroy(&outcome);
        plan_destroy(fixture.plan);
    }
}

/* A span with no shape is not a prolongation, and a prolongation shape keeps the phrase-model operator away. */
static void test_shape_decides_the_operator(void) {
    Scenario scenario;
    memset(&scenario, 0, sizeof(scenario));
    scenario.beats_per_bar = 4;
    scenario.beat_unit = 4;
    scenario.bars = 2;
    scenario.shape.kind = PLAN_HARMONY_SHAPE_PROLONGATION;
    Fixture fixture = build(&scenario);
    PhraseRequest request;
    assert(harmony_prolong_read_request(fixture.plan, fixture.target, &request));
    assert(!harmony_phrase_read_request(fixture.plan, fixture.target, &request));
    plan_destroy(fixture.plan);
}

int main(void) {
    test_every_meter_and_shape();
    test_cap_and_split();
    test_same_seed_same_chords();
    test_shape_decides_the_operator();
    printf("test_harmony_prolong: ok\n");
    return 0;
}
