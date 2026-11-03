#include <assert.h>
#include <stdio.h>
#include <string.h>

#include <standard-harmony/harmony_phrase_rules.h>

#include <ursatz_composer/constraint.h>
#include <ursatz_composer/fill_loop.h>
#include <ursatz_composer/harmony_embellish.h>
#include <ursatz_composer/harmony_phrase.h>
#include <ursatz_composer/plan_state.h>

/*
 * The embellishment operator, run through the fill loop after the phrase-model operator wrote a plain
 * progression. Every plan is one phrase over a meter lane, a key lane and one harmony span asking for a
 * cadence; everything but the harmony span is Given. Results are judged from what is in the plan.
 */

#define MAX_CHORDS 64
#define SIGNATURE_SIZE 4096

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

static Fixture build(const Scenario *scenario) {
    Fixture fixture;
    fixture.plan = plan_create();
    fixture.bar_length = fraction(scenario->beats_per_bar, (unsigned long)scenario->beat_unit);
    fixture.span.start = fraction(0, 1);
    fixture.span.length = fraction(scenario->bars * (long)fixture.bar_length.num, fixture.bar_length.den);

    assert(plan_add_node(fixture.plan, PLAN_NODE_NONE, PLAN_NODE_FORM, fixture.span, NULL, &fixture.owner) == PLAN_OK);
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
    PlanConstraint cadence = {PLAN_CONSTRAINT_CADENCE, {0}};
    cadence.params.cadence = scenario->cadence;
    assert(plan_lane_span_add_constraint(fixture.plan, fixture.target, &cadence) == PLAN_OK);
    return fixture;
}

static FillOutcome run(const Fixture *fixture, unsigned long seed, const HarmonyEmbellishParams *params,
                       bool embellish) {
    FillOperatorRegistry *registry = fill_registry_create();
    FillConfig config;
    FillOutcome outcome;
    memset(&config, 0, sizeof(config));
    config.seed = seed;
    config.checker = harmony_phrase_checker;
    assert(harmony_phrase_register(registry) == FILL_OK);
    if (embellish) {
        assert(harmony_embellish_register(registry, params) == FILL_OK);
    }
    assert(fill_run(fixture->plan, registry, &config, &outcome, NULL) == FILL_OK);
    fill_registry_destroy(registry);
    return outcome;
}

/* Writes the chords as text, one "numeral@start+length state" each, so two runs can be compared. */
static void signature(const Fixture *fixture, char *out, size_t size) {
    LaneSpanId ids[MAX_CHORDS];
    size_t count = plan_lane_find(fixture->plan, PLAN_LANE_HARMONY, 0, fixture->span, ids, MAX_CHORDS);
    size_t used = 0;
    assert(count <= MAX_CHORDS);
    out[0] = '\0';
    for (size_t i = 0; i < count; i++) {
        PlanSpan span = plan_lane_span_span(fixture->plan, ids[i]);
        int written = snprintf(out + used, size - used, "%s@%lld/%lld+%lld/%lld:%d ",
                               plan_lane_span_payload(fixture->plan, ids[i])->harmony.roman_numeral, span.start.num,
                               span.start.den, span.length.num, span.length.den,
                               (int)plan_lane_span_state(fixture->plan, ids[i]));
        assert(written > 0 && (size_t)written < size - used);
        used += (size_t)written;
    }
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

/* An applied numeral has a slash before a letter (V7/V); a plain figure has none (I64). */
static bool is_applied(const char *roman) {
    const char *slash = strrchr(roman, '/');
    return slash && slash[1] != '\0' && (slash[1] < '0' || slash[1] > '9');
}

/* What the plan holds after the embellishment pass, judged from the plan alone. */
static void check_embellished(const Fixture *fixture, const Scenario *scenario) {
    LaneSpanId ids[MAX_CHORDS];
    size_t count = plan_lane_find(fixture->plan, PLAN_LANE_HARMONY, 0, fixture->span, ids, MAX_CHORDS);
    assert(count >= 2 && count <= MAX_CHORDS);

    /* The chords tile the phrase exactly, are settled, and change only on strong beats. */
    Rational position = fixture->span.start;
    Rational strong_beats[2] = {fraction(0, 1), fraction(0, 1)};
    size_t strong_count = 1;
    if (scenario->beats_per_bar == 4 && scenario->beat_unit == 4) {
        strong_beats[strong_count++] = fraction(1, 2);
    } else if (scenario->beat_unit == 8) {
        strong_beats[strong_count++] = fraction(3, 8);
    }
    HarmonyChord chords[MAX_CHORDS];
    bool applied[MAX_CHORDS];
    HarmonyAppliedChord applied_chords[MAX_CHORDS];
    for (size_t i = 0; i < count; i++) {
        PlanSpan span = plan_lane_span_span(fixture->plan, ids[i]);
        const char *roman = plan_lane_span_payload(fixture->plan, ids[i])->harmony.roman_numeral;
        assert(rational_cmp(span.start, position) == 0);
        assert(plan_lane_span_state(fixture->plan, ids[i]) != PLAN_STATE_CONSTRAINED);
        for (size_t k = 0; k < plan_lane_span_constraint_count(fixture->plan, ids[i]); k++) {
            const PlanConstraint *constraint = plan_lane_span_constraint_at(fixture->plan, ids[i], k);
            assert(constraint->kind != PLAN_CONSTRAINT_EMBELLISH);
            /* The cadence constraint is on the last chord and nowhere else. */
            assert(constraint->kind != PLAN_CONSTRAINT_CADENCE || i == count - 1);
        }
        applied[i] = is_applied(roman);
        if (applied[i]) {
            assert(harmony_roman_parse_applied(scenario->mode, roman, &applied_chords[i]));
        } else {
            assert(harmony_roman_parse(scenario->mode, roman, &chords[i]));
        }

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
        position = time_plus(span.start, span.length);
    }
    assert(rational_cmp(position, time_plus(fixture->span.start, fixture->span.length)) == 0);
    assert(plan_lane_span_constraint_count(fixture->plan, ids[count - 1]) == 1);
    assert(plan_lane_span_constraint_at(fixture->plan, ids[count - 1], 0)->params.cadence == scenario->cadence);

    /* An applied chord leads into the chord it tonicizes (or the cadential 6/4 that embellishes V). */
    for (size_t i = 0; i < count; i++) {
        if (applied[i]) {
            assert(i + 1 < count && !applied[i + 1]);
            assert(chords[i + 1].degree == applied_chords[i].target ||
                   (applied_chords[i].target == 5 && chords[i + 1].degree == 1 && chords[i + 1].inversion == 2));
        }
    }

    /* The root-position chords pass the phrase-model rules of standard-harmony. A deceptive or plagal ending is
       read up to its dominant or authentic cadence. */
    HarmonyChord model[MAX_CHORDS];
    size_t model_count = 0;
    for (size_t i = 0; i < count; i++) {
        if (!applied[i] && chords[i].inversion == 0) {
            model[model_count++] = chords[i];
        }
    }
    size_t tail = scenario->cadence == PLAN_CADENCE_DC ? 1 : (scenario->cadence == PLAN_CADENCE_PC ? 2 : 0);
    assert(model_count > tail);
    HarmonyPhrase phrase = {scenario->mode == PLAN_MODE_MINOR ? HARMONY_MODE_MINOR : HARMONY_MODE_MAJOR, model,
                            model_count - tail};
    unsigned hard = 0;
    unsigned soft = 0;
    assert(harmony_phrase_check(&phrase, &hard, &soft));
    assert(hard == 0);

    /* The whole lane passes the checker, and the cadence the recognizer hears is the requested one. */
    const char *violation = "unread";
    assert(harmony_check_lane(fixture->plan, fixture->span, scenario->mode, scenario->cadence, &violation));
    assert(violation == NULL);
    CadenceType heard;
    bool at_end;
    assert(harmony_hear_cadence(fixture->plan, fixture->span, scenario->mode, scenario->cadence, &heard, &at_end));
    assert(heard == expected_type(scenario->cadence));
    assert(at_end);
}

typedef struct {
    bool changed;
    bool six_four;
    bool applied;
} Result;

/*
 * Runs the phrase-model operator, then the embellishment pass. With given set, one chord is Given before
 * the pass and must come out untouched. Checks the result and returns what changed.
 */
static Result pipeline(const Scenario *scenario, const HarmonyEmbellishParams *params, bool given,
                       char *plain_text, char *result_text) {
    Result result = {false, false, false};
    Fixture fixture = build(scenario);
    FillOutcome outcome = run(&fixture, scenario->seed, NULL, false);
    assert(outcome.status == FILL_STATUS_COMPLETE);
    fill_outcome_destroy(&outcome);
    signature(&fixture, plain_text, SIGNATURE_SIZE);

    LaneSpanId ids[MAX_CHORDS];
    size_t count = plan_lane_find(fixture.plan, PLAN_LANE_HARMONY, 0, fixture.span, ids, MAX_CHORDS);
    PlanSpan given_span = {{0, 1}, {0, 1}};
    char given_roman[PLAN_ROMAN_NUMERAL_MAX] = "";
    LaneSpanId given_id = PLAN_LANE_SPAN_NONE;
    if (given && count >= 3) {
        given_id = ids[1];
        given_span = plan_lane_span_span(fixture.plan, given_id);
        strcpy(given_roman, plan_lane_span_payload(fixture.plan, given_id)->harmony.roman_numeral);
        assert(plan_lane_span_set_state(fixture.plan, given_id, PLAN_STATE_GIVEN) == PLAN_OK);
    }

    assert(harmony_embellish_open(fixture.plan, fixture.span) == PLAN_OK);
    outcome = run(&fixture, scenario->seed, params, true);
    assert(outcome.status == FILL_STATUS_COMPLETE);
    fill_outcome_destroy(&outcome);
    signature(&fixture, result_text, SIGNATURE_SIZE);
    check_embellished(&fixture, scenario);

    if (given_id != PLAN_LANE_SPAN_NONE) {
        PlanSpan now = plan_lane_span_span(fixture.plan, given_id);
        assert(rational_cmp(now.start, given_span.start) == 0);
        assert(rational_cmp(now.length, given_span.length) == 0);
        assert(plan_lane_span_state(fixture.plan, given_id) == PLAN_STATE_GIVEN);
        assert(strcmp(plan_lane_span_payload(fixture.plan, given_id)->harmony.roman_numeral, given_roman) == 0);
    }

    count = plan_lane_find(fixture.plan, PLAN_LANE_HARMONY, 0, fixture.span, ids, MAX_CHORDS);
    for (size_t i = 0; i < count; i++) {
        const char *roman = plan_lane_span_payload(fixture.plan, ids[i])->harmony.roman_numeral;
        result.six_four = result.six_four || (strstr(roman, "64") != NULL && !is_applied(roman));
        result.applied = result.applied || is_applied(roman);
    }
    result.changed = strcmp(plain_text, result_text) != 0;
    plan_destroy(fixture.plan);
    return result;
}

static void check_scenario(const Scenario *scenario, const HarmonyEmbellishParams *params, bool given) {
    static char plain[SIGNATURE_SIZE];
    static char first[SIGNATURE_SIZE];
    static char second[SIGNATURE_SIZE];
    static char unused[SIGNATURE_SIZE];
    pipeline(scenario, params, given, plain, first);

    /* The same seed gives the same result. */
    pipeline(scenario, params, given, unused, second);
    assert(strcmp(first, second) == 0);

    /* No embellishment level, or no family on, returns the plain progression unchanged. */
    if (params->density == 0 || params->families == 0) {
        assert(strcmp(plain, first) == 0);
    }
}

static void test_sweep(void) {
    static const int meters[4][2] = {{2, 4}, {3, 4}, {4, 4}, {6, 8}};
    static const int tonics[2] = {0, 6};
    static const int bars[2] = {5, 8};
    static const PlanCadenceType cadences[5] = {PLAN_CADENCE_PAC, PLAN_CADENCE_IAC, PLAN_CADENCE_HC, PLAN_CADENCE_DC,
                                                PLAN_CADENCE_PC};
    HarmonyEmbellishParams params = harmony_embellish_params_default();
    params.density = 1000;
    for (int m = 0; m < 4; m++) {
        for (int mode = 0; mode < 2; mode++) {
            for (int t = 0; t < 2; t++) {
                for (int b = 0; b < 2; b++) {
                    for (int c = 0; c < 5; c++) {
                        for (unsigned long seed = 0; seed < 3; seed++) {
                            Scenario scenario = {meters[m][0], meters[m][1], mode ? PLAN_MODE_MINOR : PLAN_MODE_MAJOR,
                                                 tonics[t], bars[b], cadences[c], seed};
                            check_scenario(&scenario, &params, seed == 1);
                        }
                    }
                }
            }
        }
    }
}

/* Density 0, and every family off, leave the progression as it was. */
static void test_nothing_asked_changes_nothing(void) {
    HarmonyEmbellishParams off = harmony_embellish_params_default();
    HarmonyEmbellishParams none = harmony_embellish_params_default();
    off.density = 0;
    none.density = 1000;
    none.families = 0;
    for (unsigned long seed = 0; seed < 8; seed++) {
        Scenario scenario = {4, 4, PLAN_MODE_MAJOR, 0, 8, PLAN_CADENCE_PAC, seed};
        char plain[SIGNATURE_SIZE];
        char result[SIGNATURE_SIZE];
        assert(!pipeline(&scenario, &off, false, plain, result).changed);
        assert(strcmp(plain, result) == 0);
        assert(!pipeline(&scenario, &none, false, plain, result).changed);
        assert(strcmp(plain, result) == 0);
    }
}

/* Each family changes progressions when it is the only one on, and the results stay valid. */
static void test_each_family_acts(void) {
    static const unsigned families[3] = {HARMONY_EMBELLISH_PROLONGATION, HARMONY_EMBELLISH_PREDOMINANT,
                                         HARMONY_EMBELLISH_APPLIED};
    static const int meters[3][2] = {{4, 4}, {6, 8}, {3, 4}};
    static const PlanCadenceType cadences[3] = {PLAN_CADENCE_PAC, PLAN_CADENCE_HC, PLAN_CADENCE_DC};
    for (int f = 0; f < 3; f++) {
        HarmonyEmbellishParams params = harmony_embellish_params_default();
        params.density = 1000;
        params.families = families[f];
        int changed = 0;
        bool six_four = false;
        bool applied = false;
        for (int m = 0; m < 3; m++) {
            for (int mode = 0; mode < 2; mode++) {
                for (int c = 0; c < 3; c++) {
                    for (unsigned long seed = 0; seed < 6; seed++) {
                        Scenario scenario = {meters[m][0], meters[m][1], mode ? PLAN_MODE_MINOR : PLAN_MODE_MAJOR, 0,
                                             8, cadences[c], seed};
                        char plain[SIGNATURE_SIZE];
                        char result[SIGNATURE_SIZE];
                        Result r = pipeline(&scenario, &params, false, plain, result);
                        changed += r.changed;
                        six_four = six_four || r.six_four;
                        applied = applied || r.applied;
                    }
                }
            }
        }
        assert(changed > 0);
        assert(six_four == (families[f] == HARMONY_EMBELLISH_PROLONGATION));
        assert(applied == (families[f] == HARMONY_EMBELLISH_APPLIED));
    }
}

/* Without the marks, the embellishment operator finds nothing to do; with NULL parameters it uses the defaults. */
static void test_marks_and_defaults(void) {
    Scenario scenario = {4, 4, PLAN_MODE_MAJOR, 0, 6, PLAN_CADENCE_PAC, 2};
    Fixture fixture = build(&scenario);
    FillOutcome outcome = run(&fixture, 2, NULL, false);
    assert(outcome.status == FILL_STATUS_COMPLETE);
    fill_outcome_destroy(&outcome);
    char before[SIGNATURE_SIZE];
    char after[SIGNATURE_SIZE];
    signature(&fixture, before, sizeof(before));
    outcome = run(&fixture, 2, NULL, true);
    assert(outcome.status == FILL_STATUS_COMPLETE);
    fill_outcome_destroy(&outcome);
    signature(&fixture, after, sizeof(after));
    assert(strcmp(before, after) == 0);

    assert(harmony_embellish_open(fixture.plan, fixture.span) == PLAN_OK);
    outcome = run(&fixture, 2, NULL, true);
    assert(outcome.status == FILL_STATUS_COMPLETE);
    fill_outcome_destroy(&outcome);
    check_embellished(&fixture, &scenario);
    plan_destroy(fixture.plan);

    HarmonyEmbellishParams params = harmony_embellish_params_default();
    assert(params.density == 500 && params.families == HARMONY_EMBELLISH_ALL);
}

int main(void) {
    test_sweep();
    test_nothing_asked_changes_nothing();
    test_each_family_acts();
    test_marks_and_defaults();
    return 0;
}
