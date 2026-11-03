#include <assert.h>
#include <stdio.h>
#include <string.h>

#include <standard-harmony/harmony_applied.h>

#include <ursatz_composer/constraint.h>
#include <ursatz_composer/fill_loop.h>
#include <ursatz_composer/harmony_bass.h>
#include <ursatz_composer/harmony_embellish.h>
#include <ursatz_composer/harmony_octave.h>
#include <ursatz_composer/harmony_phrase.h>
#include <ursatz_composer/plan_state.h>

/*
 * The bass operators, run through the fill loop after the harmony operators. The first group writes a bass
 * for a progression made by the phrase-model operator and embellished by the embellishment operator; the
 * second harmonizes a Given stepwise bass. Results are judged from what is in the plan.
 */

#define MAX_CHORDS 64
#define SIGNATURE_SIZE 8192

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
    LaneSpanId given_bass; /* a Given bass span after the phrase, which nothing may change */
    PlanSpan span;
    PlanSpan form_span;
    PlanTime bar_length;
} Fixture;

static PlanTime fraction(long num, unsigned long den) {
    PlanTime time;
    assert(plan_time_make(num, den, &time) == PLAN_OK);
    return time;
}

static PlanTime time_plus(PlanTime a, PlanTime b) {
    PlanTime sum;
    assert(plan_time_add(a, b, &sum) == PLAN_OK);
    return sum;
}

static LaneSpanId add_bass(PiecePlan *plan, NodeId owner, PlanSpan span, const int *pitches, int count) {
    PlanNote notes[8];
    PlanLanePayload payload;
    LaneSpanId id;
    PlanTime length = fraction(span.length.num, span.length.den * (unsigned long)count);
    memset(&payload, 0, sizeof(payload));
    for (int i = 0; i < count; i++) {
        notes[i].midi_pitch = pitches[i];
        notes[i].offset = fraction(length.num * i, length.den);
        notes[i].duration = length;
    }
    payload.notes.notes = notes;
    payload.notes.note_count = (size_t)count;
    assert(plan_add_lane_span(plan, owner, PLAN_LANE_BASS, 0, span, &payload, &id) == PLAN_OK);
    return id;
}

/* One form node a bar longer than the phrase, a meter and a key over it, and the harmony span to fill. */
static Fixture build(const Scenario *scenario, bool cadence) {
    Fixture fixture;
    memset(&fixture, 0, sizeof(fixture));
    fixture.plan = plan_create();
    fixture.bar_length = fraction(scenario->beats_per_bar, (unsigned long)scenario->beat_unit);
    fixture.span.start = fraction(0, 1);
    fixture.span.length = fraction(scenario->bars * (long)fixture.bar_length.num, fixture.bar_length.den);
    fixture.form_span.start = fraction(0, 1);
    fixture.form_span.length = time_plus(fixture.span.length, fixture.bar_length);

    assert(plan_add_node(fixture.plan, PLAN_NODE_NONE, PLAN_NODE_FORM, fixture.form_span, NULL, &fixture.owner) ==
           PLAN_OK);
    assert(plan_node_set_state(fixture.plan, fixture.owner, PLAN_STATE_GIVEN) == PLAN_OK);

    PlanLanePayload payload;
    LaneSpanId id;
    memset(&payload, 0, sizeof(payload));
    payload.meter.beats_per_bar = scenario->beats_per_bar;
    payload.meter.beat_unit = scenario->beat_unit;
    assert(plan_add_lane_span(fixture.plan, fixture.owner, PLAN_LANE_METER, 0, fixture.form_span, &payload, &id) ==
           PLAN_OK);
    assert(plan_lane_span_set_state(fixture.plan, id, PLAN_STATE_GIVEN) == PLAN_OK);

    memset(&payload, 0, sizeof(payload));
    payload.key.tonic_pitch_class = scenario->tonic;
    payload.key.mode = scenario->mode;
    assert(plan_add_lane_span(fixture.plan, fixture.owner, PLAN_LANE_KEY, 0, fixture.form_span, &payload, &id) ==
           PLAN_OK);
    assert(plan_lane_span_set_state(fixture.plan, id, PLAN_STATE_GIVEN) == PLAN_OK);

    memset(&payload, 0, sizeof(payload));
    strcpy(payload.harmony.roman_numeral, "I");
    payload.harmony.function = PLAN_FUNCTION_UNKNOWN;
    assert(plan_add_lane_span(fixture.plan, fixture.owner, PLAN_LANE_HARMONY, 0, fixture.span, &payload,
                              &fixture.target) == PLAN_OK);
    if (cadence) {
        PlanConstraint constraint = {PLAN_CONSTRAINT_CADENCE, {0}};
        constraint.params.cadence = scenario->cadence;
        assert(plan_lane_span_add_constraint(fixture.plan, fixture.target, &constraint) == PLAN_OK);
    }

    /* A Given bass note in the last bar, outside the phrase. */
    PlanSpan after = {fixture.span.length, fixture.bar_length};
    int pitch = 43;
    fixture.given_bass = add_bass(fixture.plan, fixture.owner, after, &pitch, 1);
    assert(plan_lane_span_set_state(fixture.plan, fixture.given_bass, PLAN_STATE_GIVEN) == PLAN_OK);
    return fixture;
}

static FillOutcome run(const Fixture *fixture, unsigned long seed, bool phrase, bool embellish, bool bass,
                       const HarmonyBassParams *params, bool octave) {
    FillOperatorRegistry *registry = fill_registry_create();
    FillConfig config;
    FillOutcome outcome;
    memset(&config, 0, sizeof(config));
    config.seed = seed;
    config.checker = harmony_phrase_checker;
    if (phrase) {
        assert(harmony_phrase_register(registry) == FILL_OK);
    }
    if (embellish) {
        assert(harmony_embellish_register(registry, NULL) == FILL_OK);
    }
    if (bass) {
        assert(harmony_bass_register(registry, params) == FILL_OK);
    }
    if (octave) {
        assert(harmony_octave_register(registry) == FILL_OK);
    }
    assert(fill_run(fixture->plan, registry, &config, &outcome, NULL) == FILL_OK);
    fill_registry_destroy(registry);
    return outcome;
}

/* Adds the bass span to fill: the phrase long, one placeholder note. */
static LaneSpanId add_bass_target(Fixture *fixture) {
    int placeholder = 40;
    return add_bass(fixture->plan, fixture->owner, fixture->span, &placeholder, 1);
}

static CadenceType unused_type(void) {
    return CADENCE_TYPE_UNKNOWN;
}

/* Writes the bass notes as text so two runs can be compared. */
static void signature(const Fixture *fixture, char *out, size_t size) {
    LaneSpanId ids[MAX_CHORDS];
    size_t count = plan_lane_find(fixture->plan, PLAN_LANE_BASS, 0, fixture->span, ids, MAX_CHORDS);
    size_t used = 0;
    (void)unused_type();
    out[0] = '\0';
    for (size_t i = 0; i < count; i++) {
        PlanSpan span = plan_lane_span_span(fixture->plan, ids[i]);
        const PlanNotes *notes = &plan_lane_span_payload(fixture->plan, ids[i])->notes;
        used += (size_t)snprintf(out + used, size - used, "@%ld/%lu+%ld/%lu:%d", span.start.num, span.start.den,
                                 span.length.num, span.length.den, (int)plan_lane_span_state(fixture->plan, ids[i]));
        for (size_t n = 0; n < notes->note_count; n++) {
            used += (size_t)snprintf(out + used, size - used, " %d/%ld/%lu", notes->notes[n].midi_pitch,
                                     notes->notes[n].duration.num, notes->notes[n].duration.den);
        }
        used += (size_t)snprintf(out + used, size - used, "\n");
        assert(used + 128 < size);
    }
}

/* The pitch class a chord's bass member has, worked out from standard-harmony's scale degrees. */
static bool bass_matches(PlanKeyMode mode, int tonic, const char *roman, int bass_pc) {
    HarmonyChord chord;
    HarmonyAppliedChord applied;
    HarmonyMode harmony_mode = mode == PLAN_MODE_MINOR ? HARMONY_MODE_MINOR : HARMONY_MODE_MAJOR;
    if (harmony_roman_parse_applied(mode, roman, &applied)) {
        return bass_pc == (tonic + harmony_applied_root_semitone(harmony_mode, &applied)) % 12;
    }
    assert(harmony_roman_parse(mode, roman, &chord));
    int degree = (chord.degree - 1 + 2 * chord.inversion) % 7 + 1;
    int scale = (tonic + harmony_applied_scale_semitone(harmony_mode, degree)) % 12;
    /* The leading tone of a minor key is raised wherever a dominant needs it. */
    int raised = (tonic + 11) % 12;
    return bass_pc == scale || (mode == PLAN_MODE_MINOR && degree == 7 && bass_pc == raised);
}

static int scale_pc(PlanKeyMode mode, int tonic, int degree) {
    return (tonic + harmony_applied_scale_semitone(mode == PLAN_MODE_MINOR ? HARMONY_MODE_MINOR : HARMONY_MODE_MAJOR,
                                                   degree)) % 12;
}

/* What the plan holds once the bass operator has run, judged from the plan alone. */
static void check_bass(const Fixture *fixture, const Scenario *scenario, const HarmonyBassParams *params) {
    LaneSpanId chord_ids[MAX_CHORDS];
    size_t count = plan_lane_find(fixture->plan, PLAN_LANE_HARMONY, 0, fixture->span, chord_ids, MAX_CHORDS);
    assert(count >= 2 && count <= MAX_CHORDS);
    int last_pc[MAX_CHORDS];

    for (size_t i = 0; i < count; i++) {
        /* One bass span for each chord, the same span, settled, its notes following one another to its end. */
        PlanSpan chord_span = plan_lane_span_span(fixture->plan, chord_ids[i]);
        LaneSpanId found[2];
        assert(plan_lane_find(fixture->plan, PLAN_LANE_BASS, 0, chord_span, found, 2) == 1);
        PlanSpan bass_span = plan_lane_span_span(fixture->plan, found[0]);
        assert(plan_time_compare(bass_span.start, chord_span.start) == 0);
        assert(plan_time_compare(bass_span.length, chord_span.length) == 0);
        assert(plan_lane_span_state(fixture->plan, found[0]) == PLAN_STATE_REALIZED);

        const PlanNotes *notes = &plan_lane_span_payload(fixture->plan, found[0])->notes;
        const char *roman = plan_lane_span_payload(fixture->plan, chord_ids[i])->harmony.roman_numeral;
        PlanTime at = fraction(0, 1);
        for (size_t n = 0; n < notes->note_count; n++) {
            assert(plan_time_compare(notes->notes[n].offset, at) == 0);
            at = time_plus(at, notes->notes[n].duration);
            assert(notes->notes[n].midi_pitch >= params->range_low && notes->notes[n].midi_pitch <= params->range_high);
            /* Every note is the chord's root or inversion. */
            int pc = notes->notes[n].midi_pitch % 12;
            assert(bass_matches(scenario->mode, scenario->tonic, roman, pc));
            last_pc[i] = pc;
        }
        assert(plan_time_compare(at, bass_span.length) == 0);
    }

    /* The bass passes standard-counterpoint's melodic rules, checked by the framework from the plan. */
    PlanKey key = {scenario->tonic, scenario->mode};
    const char *violation = "unread";
    assert(harmony_check_bass(fixture->plan, fixture->span, key, params, &violation));
    assert(violation == NULL);

    /* The bass outlines the cadence by standard-harmony's definitions of its chords. */
    int tonic = scale_pc(scenario->mode, scenario->tonic, 1);
    int dominant = scale_pc(scenario->mode, scenario->tonic, 5);
    switch (scenario->cadence) {
    case PLAN_CADENCE_PAC:
    case PLAN_CADENCE_IAC:
        assert(last_pc[count - 2] == dominant && last_pc[count - 1] == tonic);
        break;
    case PLAN_CADENCE_HC:
        assert(last_pc[count - 1] == dominant);
        break;
    case PLAN_CADENCE_DC:
        assert(last_pc[count - 2] == dominant && last_pc[count - 1] == scale_pc(scenario->mode, scenario->tonic, 6));
        break;
    case PLAN_CADENCE_PC:
        assert(last_pc[count - 2] == scale_pc(scenario->mode, scenario->tonic, 4) && last_pc[count - 1] == tonic);
        break;
    }
}

/*
 * Runs the harmony operators, then the bass operator. When the bass is written, checks it and returns it as
 * text. A progression whose chords no bass line can follow without a forbidden leap (a tritone from I to the
 * root of vii/V, say) gets the loop's "no valid candidate" result and the bass span stays open; that is checked
 * too, and false is returned.
 */
static bool pipeline(const Scenario *scenario, const HarmonyBassParams *params, char *text) {
    Fixture fixture = build(scenario, true);
    FillOutcome outcome = run(&fixture, scenario->seed, true, false, false, NULL, false);
    assert(outcome.status == FILL_STATUS_COMPLETE);
    fill_outcome_destroy(&outcome);
    assert(harmony_embellish_open(fixture.plan, fixture.span) == PLAN_OK);
    outcome = run(&fixture, scenario->seed, true, true, false, NULL, false);
    assert(outcome.status == FILL_STATUS_COMPLETE);
    fill_outcome_destroy(&outcome);

    char given_before[SIGNATURE_SIZE];
    PlanSpan given_span = plan_lane_span_span(fixture.plan, fixture.given_bass);
    (void)given_before;
    LaneSpanId target = add_bass_target(&fixture);
    outcome = run(&fixture, scenario->seed, false, false, true, params, false);
    bool written = outcome.status == FILL_STATUS_COMPLETE;
    if (!written) {
        assert(outcome.status == FILL_STATUS_STUCK);
        assert(outcome.stuck_count == 1 && outcome.stuck[0].reason == FILL_STUCK_ALL_REJECTED);
        assert(outcome.stuck[0].target.id == target);
        assert(plan_lane_span_state(fixture.plan, target) == PLAN_STATE_CONSTRAINED);
        text[0] = '\0';
    }
    fill_outcome_destroy(&outcome);
    if (written) {
        check_bass(&fixture, scenario, params);
        signature(&fixture, text, SIGNATURE_SIZE);
    }

    /* The Given bass note after the phrase is untouched. */
    PlanSpan now = plan_lane_span_span(fixture.plan, fixture.given_bass);
    assert(plan_time_compare(now.start, given_span.start) == 0 && plan_time_compare(now.length, given_span.length) == 0);
    assert(plan_lane_span_state(fixture.plan, fixture.given_bass) == PLAN_STATE_GIVEN);
    assert(plan_lane_span_payload(fixture.plan, fixture.given_bass)->notes.notes[0].midi_pitch == 43);
    plan_destroy(fixture.plan);
    return written;
}

static bool check_scenario(const Scenario *scenario, const HarmonyBassParams *params) {
    static char first[SIGNATURE_SIZE];
    static char second[SIGNATURE_SIZE];
    bool written = pipeline(scenario, params, first);
    /* The same seed gives the same bass, or the same refusal. */
    assert(pipeline(scenario, params, second) == written);
    assert(strcmp(first, second) == 0);
    return written;
}

static void test_sweep(void) {
    static const int meters[4][2] = {{2, 4}, {3, 4}, {4, 4}, {6, 8}};
    static const int tonics[4] = {0, 1, 6, 10};
    static const int bars[2] = {5, 8};
    static const PlanCadenceType cadences[5] = {PLAN_CADENCE_PAC, PLAN_CADENCE_IAC, PLAN_CADENCE_HC, PLAN_CADENCE_DC,
                                                PLAN_CADENCE_PC};
    HarmonyBassParams plain = harmony_bass_params_default();
    HarmonyBassParams split = harmony_bass_params_default();
    plain.subdivision = 0;
    split.subdivision = 1000;
    int total = 0;
    int written = 0;
    for (int m = 0; m < 4; m++) {
        for (int mode = 0; mode < 2; mode++) {
            for (int t = 0; t < 4; t++) {
                for (int b = 0; b < 2; b++) {
                    for (int c = 0; c < 5; c++) {
                        for (unsigned long seed = 0; seed < 2; seed++) {
                            Scenario scenario = {meters[m][0], meters[m][1], mode ? PLAN_MODE_MINOR : PLAN_MODE_MAJOR,
                                                 tonics[t], bars[b], cadences[c], seed};
                            total++;
                            written += check_scenario(&scenario, seed ? &split : &plain);
                        }
                    }
                }
            }
        }
    }
    /* Most progressions have a bass; the rest are the ones no line follows without a forbidden leap. 512 of 640 were written
       until the shaping preferences (P16 B4): strong root progressions are preferred and the harmony operators offer them
       with a vii/V after the IV (a tritone leap in the bass), so the recorded baseline is 432. */
    assert(written >= 432);
}

/* Subdivision splits chords into several notes of the same bass member; none keeps one note a chord. */
static void test_subdivision_and_stepwise(void) {
    HarmonyBassParams plain = harmony_bass_params_default();
    HarmonyBassParams split = harmony_bass_params_default();
    plain.subdivision = 0;
    split.subdivision = 1000;
    size_t plain_notes = 0;
    size_t split_notes = 0;
    for (unsigned long seed = 0; seed < 6; seed++) {
        Scenario scenario = {4, 4, PLAN_MODE_MAJOR, 0, 8, PLAN_CADENCE_PAC, seed};
        for (int which = 0; which < 2; which++) {
            Fixture fixture = build(&scenario, true);
            FillOutcome outcome = run(&fixture, seed, true, false, false, NULL, false);
            assert(outcome.status == FILL_STATUS_COMPLETE);
            fill_outcome_destroy(&outcome);
            add_bass_target(&fixture);
            outcome = run(&fixture, seed, false, false, true, which ? &split : &plain, false);
            assert(outcome.status == FILL_STATUS_COMPLETE);
            fill_outcome_destroy(&outcome);
            LaneSpanId ids[MAX_CHORDS];
            size_t count = plan_lane_find(fixture.plan, PLAN_LANE_BASS, 0, fixture.span, ids, MAX_CHORDS);
            size_t notes = 0;
            for (size_t i = 0; i < count; i++) {
                notes += plan_lane_span_payload(fixture.plan, ids[i])->notes.note_count;
            }
            if (which) {
                split_notes += notes - count;
            } else {
                assert(notes == count);
                plain_notes += notes - count;
            }
            plan_destroy(fixture.plan);
        }
    }
    assert(plain_notes == 0);
    assert(split_notes > 0);
}

/* A range no octave of some chord's bass member fits leaves the span open: no valid candidate. */
static void test_unsatisfiable_range(void) {
    Scenario scenario = {4, 4, PLAN_MODE_MAJOR, 0, 8, PLAN_CADENCE_PAC, 1};
    HarmonyBassParams narrow = harmony_bass_params_default();
    narrow.range_low = 40;
    narrow.range_high = 41;
    Fixture fixture = build(&scenario, true);
    FillOutcome outcome = run(&fixture, 1, true, false, false, NULL, false);
    assert(outcome.status == FILL_STATUS_COMPLETE);
    fill_outcome_destroy(&outcome);
    LaneSpanId target = add_bass_target(&fixture);
    outcome = run(&fixture, 1, false, false, true, &narrow, false);
    assert(outcome.status == FILL_STATUS_STUCK);
    assert(outcome.stuck_count == 1 && outcome.stuck[0].reason == FILL_STUCK_ALL_REJECTED);
    assert(outcome.stuck[0].target.kind == FILL_TARGET_LANE_SPAN && outcome.stuck[0].target.id == target);
    assert(plan_lane_span_state(fixture.plan, target) == PLAN_STATE_CONSTRAINED);
    assert(plan_lane_span_span(fixture.plan, target).length.num == fixture.span.length.num);
    fill_outcome_destroy(&outcome);
    plan_destroy(fixture.plan);
}

/* A bass with no random octave choice (stepwise 0) either passes the melodic rules or is not written. */
static void test_random_octaves(void) {
    HarmonyBassParams loose = harmony_bass_params_default();
    loose.stepwise = 0;
    for (unsigned long seed = 0; seed < 10; seed++) {
        Scenario scenario = {4, 4, PLAN_MODE_MINOR, 3, 8, PLAN_CADENCE_HC, seed};
        Fixture fixture = build(&scenario, true);
        FillOutcome outcome = run(&fixture, seed, true, false, false, NULL, false);
        assert(outcome.status == FILL_STATUS_COMPLETE);
        fill_outcome_destroy(&outcome);
        add_bass_target(&fixture);
        outcome = run(&fixture, seed, false, false, true, &loose, false);
        if (outcome.status == FILL_STATUS_COMPLETE) {
            check_bass(&fixture, &scenario, &loose);
        } else {
            assert(outcome.stuck_count == 1 && outcome.stuck[0].reason == FILL_STUCK_ALL_REJECTED);
        }
        fill_outcome_destroy(&outcome);
        plan_destroy(fixture.plan);
    }
}

/* --- a Given stepwise bass, harmonized ---------------------------------- */

/* Builds a plan with a Given bass of one note a bar over an open harmony span, runs the operators. */
static Fixture octave_fixture(PlanKeyMode mode, const int *pitches, int count, bool cadence_constraint) {
    Scenario scenario = {4, 4, mode, 0, count, PLAN_CADENCE_PAC, 0};
    Fixture fixture = build(&scenario, cadence_constraint);
    /* The note after the phrase is not wanted here. */
    for (int i = 0; i < count; i++) {
        PlanSpan bar = {fraction(i * (long)fixture.bar_length.num, fixture.bar_length.den), fixture.bar_length};
        LaneSpanId id = add_bass(fixture.plan, fixture.owner, bar, &pitches[i], 1);
        assert(plan_lane_span_set_state(fixture.plan, id, PLAN_STATE_GIVEN) == PLAN_OK);
    }
    return fixture;
}

static void check_octave(const Fixture *fixture, PlanKeyMode mode, const int *pitches, int count) {
    LaneSpanId ids[MAX_CHORDS];
    size_t chords = plan_lane_find(fixture->plan, PLAN_LANE_HARMONY, 0, fixture->span, ids, MAX_CHORDS);
    assert(chords == (size_t)count);
    const char *violation = "unread";
    assert(harmony_check_lane_without_cadence(fixture->plan, fixture->span, mode, &violation));
    assert(violation == NULL);
    for (int i = 0; i < count; i++) {
        PlanSpan span = plan_lane_span_span(fixture->plan, ids[i]);
        assert(plan_time_compare(span.start, fraction(i * (long)fixture->bar_length.num, fixture->bar_length.den)) == 0);
        assert(plan_time_compare(span.length, fixture->bar_length) == 0);
        assert(plan_lane_span_state(fixture->plan, ids[i]) == PLAN_STATE_REALIZED);
        /* Each chord has the given bass note as its bass member. */
        assert(bass_matches(mode, 0, plan_lane_span_payload(fixture->plan, ids[i])->harmony.roman_numeral,
                            pitches[i] % 12));
    }
    /* The first and last chords are the tonic in root position. */
    const char *tonic = mode == PLAN_MODE_MINOR ? "i" : "I";
    assert(strcmp(plan_lane_span_payload(fixture->plan, ids[0])->harmony.roman_numeral, tonic) == 0);
    assert(strcmp(plan_lane_span_payload(fixture->plan, ids[count - 1])->harmony.roman_numeral, tonic) == 0);
}

static void test_rule_of_the_octave(void) {
    static const int major_up[8] = {48, 50, 52, 53, 55, 57, 59, 60};
    static const int major_down[8] = {60, 59, 57, 55, 53, 52, 50, 48};
    static const int minor_up[8] = {48, 50, 51, 53, 55, 56, 59, 60};
    static const int minor_down[8] = {60, 58, 56, 55, 53, 51, 50, 48};
    const int *lines[4] = {major_up, major_down, minor_up, minor_down};
    for (int line = 0; line < 4; line++) {
        PlanKeyMode mode = line < 2 ? PLAN_MODE_MAJOR : PLAN_MODE_MINOR;
        for (unsigned long seed = 0; seed < 8; seed++) {
            /* With a cadence constraint on the span too: the phrase-model operator must leave it alone. */
            Fixture fixture = octave_fixture(mode, lines[line], 8, seed % 2 == 1);
            FillOutcome outcome = run(&fixture, seed, true, false, false, NULL, true);
            assert(outcome.status == FILL_STATUS_COMPLETE);
            fill_outcome_destroy(&outcome);
            check_octave(&fixture, mode, lines[line], 8);
            plan_destroy(fixture.plan);
        }
    }
}

/* A bass that is not stepwise, or that does not start on the tonic, is not one the operator applies to. */
static void test_octave_needs_a_scale(void) {
    static const int leaping[8] = {48, 52, 55, 53, 55, 57, 59, 60};
    static const int off_tonic[8] = {50, 52, 53, 55, 57, 59, 60, 62};
    const int *lines[2] = {leaping, off_tonic};
    for (int line = 0; line < 2; line++) {
        Fixture fixture = octave_fixture(PLAN_MODE_MAJOR, lines[line], 8, false);
        FillOutcome outcome = run(&fixture, 1, false, false, false, NULL, true);
        assert(outcome.status == FILL_STATUS_STUCK);
        assert(outcome.stuck_count == 1 && outcome.stuck[0].reason == FILL_STUCK_NO_OPERATOR);
        assert(plan_lane_span_state(fixture.plan, fixture.target) == PLAN_STATE_CONSTRAINED);
        fill_outcome_destroy(&outcome);
        plan_destroy(fixture.plan);
    }
}

int main(void) {
    test_sweep();
    test_subdivision_and_stepwise();
    test_unsatisfiable_range();
    test_random_octaves();
    test_rule_of_the_octave();
    test_octave_needs_a_scale();
    return 0;
}
