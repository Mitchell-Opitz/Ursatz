#include <assert.h>
#include <stdio.h>
#include <string.h>

#include <standard-harmony/harmony_phrase_rules.h>

#include <ursatz_composer/constraint.h>
#include <ursatz_composer/fill_loop.h>
#include <ursatz_composer/harmony_phrase.h>
#include <ursatz_composer/plan_state.h>

/*
 * The phrase-model harmony operator, run through the fill loop with its
 * checker. Every plan is one phrase over a meter lane, a key lane and one
 * harmony span asking for a cadence; everything but the harmony span is Given,
 * so it is the only thing the loop has to fill.
 */

#define MAX_CHORDS 16

typedef struct {
    int beats_per_bar;
    int beat_unit;
    PlanKeyMode mode;
    int tonic;
    int bars;
    PlanCadenceType cadence;
    unsigned long seed;
} Scenario;

typedef struct {
    PiecePlan *plan;
    NodeId owner;
    LaneSpanId target;
    PlanSpan span;
    Rational bar_length;
} Fixture;

static Rational fraction(long num, unsigned long den) {
    Rational time;
    assert(rational_make(num, den, &time) == RATIONAL_OK);
    return time;
}

static Rational time_sub(Rational a, Rational b) {
    return fraction(a.num * (long)b.den - b.num * (long)a.den, a.den * b.den);
}

static Rational time_plus(Rational a, Rational b) {
    Rational sum;
    assert(rational_add(a, b, &sum) == RATIONAL_OK);
    return sum;
}

/* The cadence goes on the harmony span, or on the owner node (which is then Given, and cannot take it later). */
typedef enum { CADENCE_NONE, CADENCE_ON_SPAN, CADENCE_ON_OWNER } CadencePlace;

static Fixture build(const Scenario *scenario, CadencePlace place) {
    Fixture fixture;
    fixture.plan = plan_create();
    fixture.bar_length = fraction(scenario->beats_per_bar, (unsigned long)scenario->beat_unit);
    fixture.span.start = fraction(0, 1);
    fixture.span.length = fraction(scenario->bars * (long)fixture.bar_length.num, fixture.bar_length.den);

    assert(plan_add_node(fixture.plan, PLAN_NODE_NONE, PLAN_NODE_FORM, fixture.span, NULL, &fixture.owner) == PLAN_OK);
    if (place == CADENCE_ON_OWNER) {
        PlanConstraint cadence = {PLAN_CONSTRAINT_CADENCE, {0}};
        cadence.params.cadence = scenario->cadence;
        assert(plan_node_add_constraint(fixture.plan, fixture.owner, &cadence) == PLAN_OK);
    }
    assert(plan_node_set_state(fixture.plan, fixture.owner, PLAN_STATE_GIVEN) == PLAN_OK);

    PlanLanePayload payload;
    LaneSpanId id;
    memset(&payload, 0, sizeof(payload));
    payload.meter.beats_per_bar = scenario->beats_per_bar;
    payload.meter.beat_unit = scenario->beat_unit;
    assert(plan_add_lane_span(fixture.plan, fixture.owner, PLAN_LANE_METER, 0, fixture.span, &payload, &id) == PLAN_OK);
    assert(plan_lane_span_set_state(fixture.plan, id, PLAN_STATE_GIVEN) == PLAN_OK);

    memset(&payload, 0, sizeof(payload));
    payload.key.tonic_pitch_class = scenario->tonic;
    payload.key.mode = scenario->mode;
    assert(plan_add_lane_span(fixture.plan, fixture.owner, PLAN_LANE_KEY, 0, fixture.span, &payload, &id) == PLAN_OK);
    assert(plan_lane_span_set_state(fixture.plan, id, PLAN_STATE_GIVEN) == PLAN_OK);

    memset(&payload, 0, sizeof(payload));
    strcpy(payload.harmony.roman_numeral, "I");
    payload.harmony.function = PLAN_FUNCTION_UNKNOWN;
    assert(plan_add_lane_span(fixture.plan, fixture.owner, PLAN_LANE_HARMONY, 0, fixture.span, &payload,
                              &fixture.target) == PLAN_OK);
    if (place == CADENCE_ON_SPAN) {
        PlanConstraint cadence = {PLAN_CONSTRAINT_CADENCE, {0}};
        cadence.params.cadence = scenario->cadence;
        assert(plan_lane_span_add_constraint(fixture.plan, fixture.target, &cadence) == PLAN_OK);
    }
    return fixture;
}

static FillOutcome run(const Fixture *fixture, unsigned long seed) {
    FillOperatorRegistry *registry = fill_registry_create();
    FillConfig config;
    FillOutcome outcome;
    memset(&config, 0, sizeof(config));
    config.seed = seed;
    config.checker = harmony_phrase_checker;
    assert(harmony_phrase_register(registry) == FILL_OK);
    assert(fill_run(fixture->plan, registry, &config, &outcome, NULL) == FILL_OK);
    fill_registry_destroy(registry);
    return outcome;
}

/* The fewest bars a cadence fits in, counting the chords it needs (see harmony_phrase.h): a tonic, a predominant
   is optional, then the cadence, whose last chord takes the last bar. */
static int fewest_bars(PlanCadenceType cadence, size_t cells_per_bar) {
    size_t before_last = 2; /* Tb and D */
    if (cadence == PLAN_CADENCE_HC) {
        before_last = 1;
    } else if (cadence == PLAN_CADENCE_PC) {
        before_last = 4; /* Tb, D, Te, IV */
    }
    return (int)((before_last + cells_per_bar + cells_per_bar - 1) / cells_per_bar);
}

static size_t cells_per_bar(int beats_per_bar, int beat_unit) {
    return (beat_unit == 4 && beats_per_bar == 4) || beat_unit == 8 ? 2 : 1;
}

static CadenceType expected_type(PlanCadenceType cadence) {
    switch (cadence) {
    case PLAN_CADENCE_PAC:
        return CADENCE_TYPE_PERFECT_AUTHENTIC;
    case PLAN_CADENCE_IAC:
        return CADENCE_TYPE_IMPERFECT_AUTHENTIC;
    case PLAN_CADENCE_HC:
        return CADENCE_TYPE_HALF;
    case PLAN_CADENCE_DC:
        return CADENCE_TYPE_DECEPTIVE;
    case PLAN_CADENCE_PC:
        return CADENCE_TYPE_PLAGAL;
    }
    return CADENCE_TYPE_UNKNOWN;
}

/* Writes the chords as text, one "numeral@start" each, so two runs can be compared. */
static void signature(const Fixture *fixture, char *out, size_t size) {
    LaneSpanId ids[MAX_CHORDS];
    size_t count = plan_lane_find(fixture->plan, PLAN_LANE_HARMONY, 0, fixture->span, ids, MAX_CHORDS);
    size_t used = 0;
    out[0] = '\0';
    for (size_t i = 0; i < count && i < MAX_CHORDS; i++) {
        PlanSpan span = plan_lane_span_span(fixture->plan, ids[i]);
        int written = snprintf(out + used, size - used, "%s@%lld/%lld+%lld/%lld ",
                               plan_lane_span_payload(fixture->plan, ids[i])->harmony.roman_numeral, span.start.num,
                               span.start.den, span.length.num, span.length.den);
        assert(written > 0 && (size_t)written < size - used);
        used += (size_t)written;
    }
}

/* Everything a chosen progression must satisfy. */
static void check_progression(const Fixture *fixture, const Scenario *scenario) {
    LaneSpanId ids[MAX_CHORDS];
    size_t count = plan_lane_find(fixture->plan, PLAN_LANE_HARMONY, 0, fixture->span, ids, MAX_CHORDS);
    assert(count >= 2 && count <= MAX_CHORDS);

    /* The chords tile the phrase exactly, are Realized, and change only on strong beats. */
    Rational position = fixture->span.start;
    Rational strong_beats[2] = {fraction(0, 1), fraction(0, 1)};
    size_t strong_count = 1;
    if (scenario->beats_per_bar == 4 && scenario->beat_unit == 4) {
        strong_beats[strong_count++] = fraction(1, 2);
    } else if (scenario->beat_unit == 8) {
        strong_beats[strong_count++] = fraction(3, 8);
    }
    HarmonyChord chords[MAX_CHORDS];
    for (size_t i = 0; i < count; i++) {
        PlanSpan span = plan_lane_span_span(fixture->plan, ids[i]);
        const PlanHarmony *harmony = &plan_lane_span_payload(fixture->plan, ids[i])->harmony;
        assert(rational_cmp(span.start, position) == 0);
        assert(plan_lane_span_state(fixture->plan, ids[i]) == PLAN_STATE_REALIZED);
        assert(harmony->function != PLAN_FUNCTION_UNKNOWN);
        assert(harmony_roman_parse(scenario->mode, harmony->roman_numeral, &chords[i]));

        Rational bar_start = fixture->span.start;
        Rational next_bar = time_plus(bar_start, fixture->bar_length);
        while (rational_cmp(next_bar, span.start) <= 0) {
            bar_start = next_bar;
            next_bar = time_plus(bar_start, fixture->bar_length);
        }
        Rational offset = time_sub(span.start, bar_start);
        bool strong = false;
        for (size_t b = 0; b < strong_count; b++) {
            strong = strong || rational_cmp(offset, strong_beats[b]) == 0;
        }
        assert(strong);
        if (i == count - 1) {
            assert(rational_cmp(offset, fraction(0, 1)) == 0); /* the last chord arrives on a downbeat */
        }
        position = time_plus(span.start, span.length);
    }
    assert(rational_cmp(position, time_plus(fixture->span.start, fixture->span.length)) == 0);

    /* The cadence constraint moved to the final chord. */
    assert(plan_lane_span_constraint_count(fixture->plan, ids[count - 1]) == 1);
    assert(plan_lane_span_constraint_at(fixture->plan, ids[count - 1], 0)->params.cadence == scenario->cadence);

    /* The phrase passes standard-harmony: no hard rule broken. A deceptive or plagal ending is read up to its
       dominant or authentic cadence, as the checker does. */
    size_t model = count;
    model -= scenario->cadence == PLAN_CADENCE_DC ? 1 : (scenario->cadence == PLAN_CADENCE_PC ? 2 : 0);
    HarmonyPhrase phrase = {scenario->mode == PLAN_MODE_MINOR ? HARMONY_MODE_MINOR : HARMONY_MODE_MAJOR, chords,
                            model};
    unsigned hard = 0;
    unsigned soft = 0;
    assert(harmony_phrase_check(&phrase, &hard, &soft));
    assert(hard == 0);

    /* In minor the dominant is major (the raised leading tone): V, never v. */
    for (size_t i = 0; i < count; i++) {
        if (chords[i].degree == 5) {
            assert(plan_lane_span_payload(fixture->plan, ids[i])->harmony.roman_numeral[0] == 'V');
            assert(plan_lane_span_payload(fixture->plan, ids[i])->harmony.function == PLAN_FUNCTION_DOMINANT);
        }
    }

    /* The cadence standard-harmony's recognizer hears is the requested one, and it ends the phrase. */
    CadenceType heard;
    bool at_end;
    assert(harmony_hear_cadence(fixture->plan, fixture->span, scenario->mode, scenario->cadence, &heard, &at_end));
    assert(heard == expected_type(scenario->cadence));
    assert(at_end);
}

static void check_scenario(const Scenario *scenario) {
    Fixture fixture = build(scenario, CADENCE_ON_SPAN);
    FillOutcome outcome = run(&fixture, scenario->seed);
    int fewest = fewest_bars(scenario->cadence, cells_per_bar(scenario->beats_per_bar, scenario->beat_unit));
    if (scenario->bars < fewest) {
        /* Unsatisfiable: the loop leaves the span open, rejected, and the plan unchanged. */
        assert(outcome.status == FILL_STATUS_STUCK);
        assert(outcome.stuck_count == 1);
        assert(outcome.stuck[0].reason == FILL_STUCK_ALL_REJECTED);
        assert(outcome.stuck[0].target.kind == FILL_TARGET_LANE_SPAN && outcome.stuck[0].target.id == fixture.target);
        assert(plan_lane_span_state(fixture.plan, fixture.target) == PLAN_STATE_CONSTRAINED);
        assert(plan_lane_span_count(fixture.plan) == 3);
    } else {
        assert(outcome.status == FILL_STATUS_COMPLETE);
        check_progression(&fixture, scenario);

        /* The same seed gives the same progression. */
        char first[512];
        char second[512];
        signature(&fixture, first, sizeof(first));
        Fixture again = build(scenario, CADENCE_ON_SPAN);
        FillOutcome again_outcome = run(&again, scenario->seed);
        assert(again_outcome.status == FILL_STATUS_COMPLETE);
        signature(&again, second, sizeof(second));
        assert(strcmp(first, second) == 0);
        fill_outcome_destroy(&again_outcome);
        plan_destroy(again.plan);
    }
    fill_outcome_destroy(&outcome);
    plan_destroy(fixture.plan);
}

static void test_sweep(void) {
    static const int meters[4][2] = {{2, 4}, {3, 4}, {4, 4}, {6, 8}};
    static const int tonics[3] = {0, 6, 10};
    static const PlanCadenceType cadences[5] = {PLAN_CADENCE_PAC, PLAN_CADENCE_IAC, PLAN_CADENCE_HC, PLAN_CADENCE_DC,
                                                PLAN_CADENCE_PC};
    for (int m = 0; m < 4; m++) {
        for (int mode = 0; mode < 2; mode++) {
            for (int t = 0; t < 3; t++) {
                for (int bars = 1; bars <= 10; bars++) {
                    for (int c = 0; c < 5; c++) {
                        for (unsigned long seed = 0; seed < 6; seed++) {
                            Scenario scenario = {meters[m][0], meters[m][1], mode ? PLAN_MODE_MINOR : PLAN_MODE_MAJOR,
                                                 tonics[t], bars, cadences[c], seed};
                            check_scenario(&scenario);
                        }
                    }
                }
            }
        }
    }
}

/* Different seeds give different progressions for a phrase with room to vary. */
static void test_seeds_vary(void) {
    char seen[16][512];
    int distinct = 0;
    for (unsigned long seed = 0; seed < 16; seed++) {
        Scenario scenario = {4, 4, PLAN_MODE_MAJOR, 0, 8, PLAN_CADENCE_PAC, seed};
        Fixture fixture = build(&scenario, CADENCE_ON_SPAN);
        FillOutcome outcome = run(&fixture, seed);
        assert(outcome.status == FILL_STATUS_COMPLETE);
        char text[512];
        signature(&fixture, text, sizeof(text));
        bool known = false;
        for (int i = 0; i < distinct; i++) {
            known = known || strcmp(seen[i], text) == 0;
        }
        if (!known) {
            strcpy(seen[distinct++], text);
        }
        fill_outcome_destroy(&outcome);
        plan_destroy(fixture.plan);
    }
    assert(distinct > 1);
}

/* A span with no cadence to ask for is not one the operator applies to. */
static void test_no_cadence_is_not_applicable(void) {
    Scenario scenario = {4, 4, PLAN_MODE_MAJOR, 0, 4, PLAN_CADENCE_PAC, 0};
    Fixture fixture = build(&scenario, CADENCE_NONE);
    FillOutcome outcome = run(&fixture, 0);
    assert(outcome.status == FILL_STATUS_STUCK);
    assert(outcome.stuck_count == 1 && outcome.stuck[0].reason == FILL_STUCK_NO_OPERATOR);
    fill_outcome_destroy(&outcome);
    plan_destroy(fixture.plan);
}

/* A cadence on the owner node does as well as one on the span. */
static void test_cadence_on_owner(void) {
    Scenario scenario = {3, 4, PLAN_MODE_MINOR, 9, 4, PLAN_CADENCE_HC, 3};
    Fixture fixture = build(&scenario, CADENCE_ON_OWNER);
    FillOutcome outcome = run(&fixture, 3);
    assert(outcome.status == FILL_STATUS_COMPLETE);
    CadenceType heard;
    bool at_end;
    assert(harmony_hear_cadence(fixture.plan, fixture.span, PLAN_MODE_MINOR, PLAN_CADENCE_HC, &heard, &at_end));
    assert(heard == CADENCE_TYPE_HALF && at_end);
    fill_outcome_destroy(&outcome);
    plan_destroy(fixture.plan);
}

/* Adds a harmony shape to the target and runs the loop. */
static FillOutcome run_shaped(const Fixture *fixture, PlanHarmonyShape shape, unsigned long seed) {
    PlanConstraint constraint = {PLAN_CONSTRAINT_HARMONY_SHAPE, {0}};
    constraint.params.harmony_shape = shape;
    assert(plan_lane_span_add_constraint(fixture->plan, fixture->target, &constraint) == PLAN_OK);
    return run(fixture, seed);
}

/* max_cells_per_chord caps every chord but the last, which takes the whole last bar; split_at_bars keeps chords off barlines. */
static void test_phrase_model_shape(void) {
    static const int meters[3][2] = {{4, 4}, {6, 8}, {3, 4}};
    static const PlanCadenceType cadences[3] = {PLAN_CADENCE_PAC, PLAN_CADENCE_HC, PLAN_CADENCE_IAC};
    for (int m = 0; m < 3; m++) {
        for (int c = 0; c < 3; c++) {
            for (int cap = 1; cap <= 3; cap++) {
                for (unsigned long seed = 0; seed < 8; seed++) {
                    Scenario scenario = {meters[m][0], meters[m][1], PLAN_MODE_MAJOR, 0, 6, cadences[c], seed};
                    Fixture fixture = build(&scenario, CADENCE_ON_SPAN);
                    PlanHarmonyShape shape;
                    memset(&shape, 0, sizeof(shape));
                    shape.kind = PLAN_HARMONY_SHAPE_PHRASE_MODEL;
                    shape.max_cells_per_chord = cap;
                    shape.split_at_bars = true;
                    FillOutcome outcome = run_shaped(&fixture, shape, seed);
                    size_t per_bar = cells_per_bar(meters[m][0], meters[m][1]);
                    if (outcome.status == FILL_STATUS_COMPLETE) {
                        check_progression(&fixture, &scenario);
                        LaneSpanId ids[MAX_CHORDS];
                        size_t count = plan_lane_find(fixture.plan, PLAN_LANE_HARMONY, 0, fixture.span, ids, MAX_CHORDS);
                        Rational cell = fraction(fixture.bar_length.num, fixture.bar_length.den * (unsigned long)per_bar);
                        for (size_t i = 0; i < count; i++) {
                            PlanSpan span = plan_lane_span_span(fixture.plan, ids[i]);
                            /* Length in cells, and the cell it starts on in its bar. */
                            long length = span.length.num * (long)cell.den / ((long)span.length.den * cell.num);
                            long start = span.start.num * (long)cell.den / ((long)span.start.den * cell.num);
                            if (i + 1 < count) {
                                assert(length <= cap);
                            }
                            assert((size_t)(start % (long)per_bar) + (size_t)length <= per_bar);
                        }
                    } else {
                        /* Only a phrase too tightly capped to be dealt is left open. */
                        assert(outcome.status == FILL_STATUS_STUCK && outcome.stuck_count == 1 &&
                               outcome.stuck[0].reason == FILL_STUCK_ALL_REJECTED);
                    }
                    fill_outcome_destroy(&outcome);
                    plan_destroy(fixture.plan);
                }
            }
        }
    }
}

/* A cadential progression has no opening tonic: it is the predominant, the dominant and the arrival, over the whole span. */
static void test_cadential_shape(void) {
    static const int meters[4][2] = {{4, 4}, {6, 8}, {3, 4}, {2, 4}};
    for (int m = 0; m < 4; m++) {
        for (int bars = 2; bars <= 4; bars++) {
            for (unsigned long seed = 0; seed < 8; seed++) {
                Scenario scenario = {meters[m][0], meters[m][1], PLAN_MODE_MAJOR, 0, bars, PLAN_CADENCE_PAC, seed};
                Fixture fixture = build(&scenario, CADENCE_ON_SPAN);
                PlanHarmonyShape shape;
                memset(&shape, 0, sizeof(shape));
                shape.kind = PLAN_HARMONY_SHAPE_CADENTIAL;
                FillOutcome outcome = run_shaped(&fixture, shape, seed);
                assert(outcome.status == FILL_STATUS_COMPLETE);
                LaneSpanId ids[MAX_CHORDS];
                size_t count = plan_lane_find(fixture.plan, PLAN_LANE_HARMONY, 0, fixture.span, ids, MAX_CHORDS);
                assert(count >= 2);
                assert(plan_lane_span_payload(fixture.plan, ids[0])->harmony.function != PLAN_FUNCTION_TONIC);
                CadenceType heard;
                bool at_end;
                assert(harmony_hear_cadence(fixture.plan, fixture.span, PLAN_MODE_MAJOR, PLAN_CADENCE_PAC, &heard,
                                            &at_end));
                assert(heard == CADENCE_TYPE_PERFECT_AUTHENTIC && at_end);
                fill_outcome_destroy(&outcome);
                plan_destroy(fixture.plan);
            }
        }
    }
}

int main(void) {
    test_sweep();
    test_phrase_model_shape();
    test_cadential_shape();
    test_seeds_vary();
    test_no_cadence_is_not_applicable();
    test_cadence_on_owner();
    return 0;
}
