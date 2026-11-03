#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <galant-schemas/galant_degree.h>
#include <galant-schemas/galant_prototypes.h>
#include <standard-harmony/harmony_applied.h>
#include <scale-harmonization/scale_patterns.h>

#include <ursatz_composer/constraint.h>
#include <ursatz_composer/fill_loop.h>
#include <ursatz_composer/fill_record.h>
#include <ursatz_composer/fill_suggest.h>
#include <ursatz_composer/harmony_bass.h>
#include <ursatz_composer/harmony_phrase.h>
#include <ursatz_composer/harmony_schema.h>
#include <ursatz_composer/harmony_sequence.h>
#include <ursatz_composer/plan_state.h>

/*
 * The schema and sequence operators, run through the fill loop. Each plan is one phrase over a meter lane and a key
 * lane, with the harmony span to fill and a bass placeholder over it. What the operators wrote is judged from the
 * plan: the harmony and bass against the framework's own prototype of the schema, transposed to the key, and the
 * lanes against standard-harmony and standard-counterpoint.
 */

#define MAX_CHORDS 64
#define SIGNATURE_SIZE 16384

typedef struct {
    int beats_per_bar;
    int beat_unit;
    PlanKeyMode mode;
    int tonic;
    int bars;        /* the phrase, in bars */
    int before_bars; /* bars at its start held by a Given tonic chord, so the target is not at the phrase start */
    CadenceType cadence;
    unsigned long seed;
} Scenario;

typedef struct {
    PiecePlan *plan;
    NodeId owner;
    LaneSpanId target;
    LaneSpanId bass_target;
    LaneSpanId given_chord;
    LaneSpanId given_bass;
    PlanSpan phrase;
    PlanSpan region; /* the target */
    Rational bar_length;
} Fixture;

static Rational fraction(long num, unsigned long den) {
    Rational time;
    assert(rational_make(num, den, &time) == RATIONAL_OK);
    return time;
}

static Rational time_plus(Rational a, Rational b) {
    Rational sum;
    assert(rational_add(a, b, &sum) == RATIONAL_OK);
    return sum;
}

static Rational time_sub(Rational a, Rational b) {
    return fraction(a.num * (long)b.den - b.num * (long)a.den, a.den * b.den);
}

static Rational bars_of(const Fixture *fixture, long bars) {
    return fraction(bars * (long)fixture->bar_length.num, fixture->bar_length.den);
}

static LaneSpanId add_bass(PiecePlan *plan, NodeId owner, PlanSpan span, int pitch) {
    PlanNote note = {pitch, fraction(0, 1), span.length};
    PlanLanePayload payload;
    LaneSpanId id;
    memset(&payload, 0, sizeof(payload));
    payload.notes.notes = &note;
    payload.notes.note_count = 1;
    assert(plan_add_lane_span(plan, owner, PLAN_LANE_BASS, 0, span, &payload, &id) == PLAN_OK);
    return id;
}

static LaneSpanId add_harmony(PiecePlan *plan, NodeId owner, PlanSpan span, const char *roman,
                              PlanHarmonicFunction function) {
    PlanLanePayload payload;
    LaneSpanId id;
    memset(&payload, 0, sizeof(payload));
    strcpy(payload.harmony.roman_numeral, roman);
    payload.harmony.function = function;
    assert(plan_add_lane_span(plan, owner, PLAN_LANE_HARMONY, 0, span, &payload, &id) == PLAN_OK);
    return id;
}

/* A form node a bar longer than the phrase, so that a Given bass note after the phrase shows what is left alone. */
static Fixture build(const Scenario *scenario) {
    Fixture fixture;
    memset(&fixture, 0, sizeof(fixture));
    fixture.plan = plan_create();
    fixture.bar_length = fraction(scenario->beats_per_bar, (unsigned long)scenario->beat_unit);
    fixture.phrase.start = fraction(0, 1);
    fixture.phrase.length = bars_of(&fixture, scenario->bars);
    PlanSpan form = {fraction(0, 1), bars_of(&fixture, scenario->bars + 1)};
    PlanSpan before = {fraction(0, 1), bars_of(&fixture, scenario->before_bars)};
    fixture.region.start = before.length;
    fixture.region.length = bars_of(&fixture, scenario->bars - scenario->before_bars);

    assert(plan_add_node(fixture.plan, PLAN_NODE_NONE, PLAN_NODE_FORM, form, NULL, &fixture.owner) == PLAN_OK);
    assert(plan_node_set_state(fixture.plan, fixture.owner, PLAN_STATE_GIVEN) == PLAN_OK);

    PlanLanePayload payload;
    LaneSpanId id;
    memset(&payload, 0, sizeof(payload));
    payload.meter.beats_per_bar = scenario->beats_per_bar;
    payload.meter.beat_unit = scenario->beat_unit;
    assert(plan_add_lane_span(fixture.plan, fixture.owner, PLAN_LANE_METER, 0, form, &payload, &id) == PLAN_OK);
    assert(plan_lane_span_set_state(fixture.plan, id, PLAN_STATE_GIVEN) == PLAN_OK);
    memset(&payload, 0, sizeof(payload));
    payload.key.tonic_pitch_class = scenario->tonic;
    payload.key.mode = scenario->mode;
    assert(plan_add_lane_span(fixture.plan, fixture.owner, PLAN_LANE_KEY, 0, form, &payload, &id) == PLAN_OK);
    assert(plan_lane_span_set_state(fixture.plan, id, PLAN_STATE_GIVEN) == PLAN_OK);

    if (scenario->before_bars > 0) {
        fixture.given_chord = add_harmony(fixture.plan, fixture.owner, before, scenario->mode == PLAN_MODE_MINOR ? "i" : "I",
                                          PLAN_FUNCTION_TONIC);
        assert(plan_lane_span_set_state(fixture.plan, fixture.given_chord, PLAN_STATE_GIVEN) == PLAN_OK);
    }
    fixture.target = add_harmony(fixture.plan, fixture.owner, fixture.region, "I", PLAN_FUNCTION_UNKNOWN);
    PlanConstraint cadence = {PLAN_CONSTRAINT_CADENCE, {0}};
    cadence.params.cadence = scenario->cadence;
    assert(plan_lane_span_add_constraint(fixture.plan, fixture.target, &cadence) == PLAN_OK);
    fixture.bass_target = add_bass(fixture.plan, fixture.owner, fixture.region, 40);

    PlanSpan after = {fixture.phrase.length, fixture.bar_length};
    fixture.given_bass = add_bass(fixture.plan, fixture.owner, after, 43);
    assert(plan_lane_span_set_state(fixture.plan, fixture.given_bass, PLAN_STATE_GIVEN) == PLAN_OK);
    return fixture;
}

typedef struct {
    const HarmonySchemaParams *schema;
    const HarmonySequenceParams *sequence;
    bool phrase;
    bool bass;
} Operators;

static FillOutcome run(const Fixture *fixture, unsigned long seed, const Operators *operators, FillRecord **record) {
    FillOperatorRegistry *registry = fill_registry_create();
    FillConfig config;
    FillOutcome outcome;
    memset(&config, 0, sizeof(config));
    config.seed = seed;
    config.checker = harmony_phrase_checker;
    if (operators->schema) {
        assert(harmony_schema_register(registry, operators->schema) == FILL_OK);
    }
    if (operators->sequence) {
        assert(harmony_sequence_register(registry, operators->sequence) == FILL_OK);
    }
    if (operators->phrase) {
        assert(harmony_phrase_register(registry) == FILL_OK);
    }
    if (operators->bass) {
        assert(harmony_bass_register(registry, NULL) == FILL_OK);
    }
    assert(fill_run(fixture->plan, registry, &config, &outcome, record) == FILL_OK);
    fill_registry_destroy(registry);
    return outcome;
}

/* The labels the loop chose, one after another, as one string. */
static void chosen_labels(const FillRecord *record, char *out, size_t size) {
    size_t used = 0;
    out[0] = '\0';
    for (size_t s = 0; s < record->step_count; s++) {
        const FillStep *step = &record->steps[s];
        if (step->outcome == FILL_STEP_APPLIED && step->chosen >= 0) {
            used += (size_t)snprintf(out + used, size - used, "%s;", step->considered[step->chosen].label);
            assert(used + 80 < size);
        }
    }
}

/* Writes both lanes as text so two runs can be compared. */
static void signature(const Fixture *fixture, char *out, size_t size) {
    LaneSpanId ids[MAX_CHORDS];
    size_t used = 0;
    out[0] = '\0';
    for (int lane = 0; lane < 2; lane++) {
        PlanLaneKind kind = lane == 0 ? PLAN_LANE_HARMONY : PLAN_LANE_BASS;
        size_t count = plan_lane_find(fixture->plan, kind, 0, fixture->phrase, ids, MAX_CHORDS);
        for (size_t i = 0; i < count; i++) {
            PlanSpan span = plan_lane_span_span(fixture->plan, ids[i]);
            const PlanLanePayload *payload = plan_lane_span_payload(fixture->plan, ids[i]);
            used += (size_t)snprintf(out + used, size - used, "%d@%lld/%lld+%lld/%lld:%d", lane, span.start.num,
                                     span.start.den, span.length.num, span.length.den,
                                     (int)plan_lane_span_state(fixture->plan, ids[i]));
            if (lane == 0) {
                used += (size_t)snprintf(out + used, size - used, " %s", payload->harmony.roman_numeral);
            } else {
                for (size_t n = 0; n < payload->notes.note_count; n++) {
                    used += (size_t)snprintf(out + used, size - used, " %d", payload->notes.notes[n].midi_pitch);
                }
            }
            used += (size_t)snprintf(out + used, size - used, "\n");
            assert(used + 160 < size);
        }
    }
}

/* --- reading the plan back ------------------------------------------------ */

/* The scale degree a Roman numeral names, whatever its case. */
static int numeral_degree(const char *numeral, size_t length) {
    static const char *const NAMES[7] = {"i", "ii", "iii", "iv", "v", "vi", "vii"};
    char lower[8];
    assert(length > 0 && length < sizeof(lower));
    for (size_t i = 0; i < length; i++) {
        lower[i] = (char)((numeral[i] >= 'A' && numeral[i] <= 'Z') ? numeral[i] + 32 : numeral[i]);
    }
    lower[length] = '\0';
    for (int degree = 0; degree < 7; degree++) {
        if (strcmp(lower, NAMES[degree]) == 0) {
            return degree + 1;
        }
    }
    return 0;
}

static bool is_applied(const char *roman) {
    const char *slash = strrchr(roman, '/');
    return slash && slash[1] != '\0' && (slash[1] < '0' || slash[1] > '9');
}

/* Whether the pitch class is what the prototype's bass asks for in the key, by the framework's degrees. */
static bool bass_matches(const GalantDegree *wanted, int tonic, int pc) {
    int major = harmony_applied_scale_semitone(HARMONY_MODE_MAJOR, wanted->degree);
    int options[2];
    int count = 0;
    switch (wanted->alteration) {
    case GALANT_ALTERATION_NATURAL:
        options[count++] = 0;
        break;
    case GALANT_ALTERATION_LOWERED:
        options[count++] = -1;
        break;
    case GALANT_ALTERATION_EITHER:
        options[count++] = 0;
        options[count++] = -1;
        break;
    case GALANT_ALTERATION_RAISED:
        options[count++] = 1;
        break;
    }
    for (int i = 0; i < count; i++) {
        if (((tonic + major + options[i]) % 12 + 12) % 12 == pc) {
            return true;
        }
    }
    return false;
}

static Rational offset_in_bar(const Fixture *fixture, Rational time) {
    Rational start = fixture->phrase.start;
    Rational next = time_plus(start, fixture->bar_length);
    while (rational_cmp(next, time) <= 0) {
        start = next;
        next = time_plus(start, fixture->bar_length);
    }
    return time_sub(time, start);
}

/* The one pitch class of a chord's bass span, which must lie exactly under the chord. */
static int bass_pc_under(const Fixture *fixture, LaneSpanId chord) {
    PlanSpan span = plan_lane_span_span(fixture->plan, chord);
    LaneSpanId found[2];
    assert(plan_lane_find(fixture->plan, PLAN_LANE_BASS, 0, span, found, 2) == 1);
    PlanSpan bass = plan_lane_span_span(fixture->plan, found[0]);
    assert(rational_cmp(bass.start, span.start) == 0 && rational_cmp(bass.length, span.length) == 0);
    assert(plan_lane_span_state(fixture->plan, found[0]) == PLAN_STATE_REALIZED);
    const PlanNotes *notes = &plan_lane_span_payload(fixture->plan, found[0])->notes;
    int pc = notes->notes[0].midi_pitch % 12;
    for (size_t n = 0; n < notes->note_count; n++) {
        assert(notes->notes[n].midi_pitch % 12 == pc);
    }
    return pc;
}

static bool figure_is_seventh(const char *figures) {
    const char *dash = strrchr(figures, '-');
    const char *f = dash ? dash + 1 : figures;
    while (*f == '#' || *f == 'b') {
        f++;
    }
    return strstr(f, "6/5") || strstr(f, "4/3") || strstr(f, "4/2") || f[0] == '7';
}

static bool schema_ref(const Fixture *fixture, LaneSpanId id, PlanSchemaRef *out) {
    size_t count = plan_lane_span_constraint_count(fixture->plan, id);
    for (size_t i = 0; i < count; i++) {
        const PlanConstraint *constraint = plan_lane_span_constraint_at(fixture->plan, id, i);
        if (constraint->kind == PLAN_CONSTRAINT_SCHEMA) {
            *out = constraint->params.schema;
            return true;
        }
    }
    return false;
}

/*
 * Every schema the plan holds is checked against the framework's prototype: the stages in order, each chord's root
 * and kind from the numeral, each bass note from the stage's bass degree, the strength of each stage against the beat
 * it falls on, and the bass against standard-counterpoint's melodic rules. Returns how many schemas there are.
 */
static int check_schemas(const Fixture *fixture, const Scenario *scenario, const HarmonyBassParams *bass,
                         int only_schema) {
    LaneSpanId ids[MAX_CHORDS];
    size_t count = plan_lane_find(fixture->plan, PLAN_LANE_HARMONY, 0, fixture->phrase, ids, MAX_CHORDS);
    int found = 0;
    Rational position = fixture->phrase.start;
    for (size_t i = 0; i < count; i++) {
        PlanSpan span = plan_lane_span_span(fixture->plan, ids[i]);
        assert(rational_cmp(span.start, position) == 0); /* the harmony lane is a tiling of the phrase */
        position = time_plus(span.start, span.length);
    }
    for (size_t i = 0; i < count;) {
        PlanSchemaRef ref;
        if (!schema_ref(fixture, ids[i], &ref)) {
            i++;
            continue;
        }
        assert(ref.stage == 0);
        const GalantPrototype *prototype = galant_prototype((GalantSchemaId)ref.schema);
        assert(prototype);
        assert(only_schema < 0 || ref.schema == only_schema);
        assert(i + prototype->stage_count <= count);
        for (size_t k = 0; k < prototype->stage_count; k++) {
            PlanSchemaRef stage_ref;
            const GalantStage *stage = &prototype->stages[k];
            const char *roman = plan_lane_span_payload(fixture->plan, ids[i + k])->harmony.roman_numeral;
            assert(schema_ref(fixture, ids[i + k], &stage_ref));
            assert(stage_ref.schema == ref.schema && stage_ref.stage == (int)k);

            /* The chord, from the numeral. */
            const char *slash = strchr(stage->numeral, '/');
            if (strcmp(stage->numeral, "Cad.") == 0) {
                HarmonyChord chord;
                assert(harmony_roman_parse(scenario->mode, roman, &chord));
                assert(chord.degree == 1 && chord.inversion == 2);
            } else if (slash) {
                HarmonyAppliedChord applied;
                int inversion;
                assert(is_applied(roman));
                assert(harmony_roman_parse_applied_inverted(scenario->mode, roman, &applied, &inversion));
                assert(applied.target == numeral_degree(slash + 1, strlen(slash + 1)));
                assert((applied.kind == APPLIED_CHORD_KIND_SECONDARY_DOMINANT) == (stage->numeral[0] == 'V'));
                assert(harmony_applied_quality_is_seventh(applied.quality) == figure_is_seventh(stage->figures));
            } else {
                HarmonyChord chord;
                assert(!is_applied(roman));
                assert(harmony_roman_parse(scenario->mode, roman, &chord));
                assert(chord.degree == numeral_degree(stage->numeral, strlen(stage->numeral)));
                assert(chord.seventh == figure_is_seventh(stage->figures));
            }

            /* The bass, from the stage's bass degree. */
            int pc = bass_pc_under(fixture, ids[i + k]);
            assert(bass_matches(&stage->bass, scenario->tonic, pc));

            /* The stage falls on a beat of its strength (the first chord of a weak start is held through the strong
               beat before it). */
            Rational offset = offset_in_bar(fixture, plan_lane_span_span(fixture->plan, ids[i + k]).start);
            bool strong = offset.num == 0;
            if (k > 0 || stage->strength == BEAT_STRENGTH_STRONG || !strong) {
                assert(strong == (stage->strength == BEAT_STRENGTH_STRONG));
            }
        }
        /* The schema runs to the end of a bar, and its bass passes the melodic rules. */
        PlanSpan first = plan_lane_span_span(fixture->plan, ids[i]);
        PlanSpan last = plan_lane_span_span(fixture->plan, ids[i + prototype->stage_count - 1]);
        Rational end = time_plus(last.start, last.length);
        assert(offset_in_bar(fixture, end).num == 0);
        PlanSpan whole = {first.start, time_sub(end, first.start)};
        PlanKey key = {scenario->tonic, scenario->mode};
        const char *violation = "unread";
        assert(harmony_check_bass(fixture->plan, whole, key, bass, &violation));
        assert(violation == NULL);
        found++;
        i += prototype->stage_count;
    }
    return found;
}

/* The Given chord before the phrase's target and the Given bass note after it are as they were made. */
static void check_given(const Fixture *fixture, const Scenario *scenario) {
    if (scenario->before_bars > 0) {
        PlanSpan span = plan_lane_span_span(fixture->plan, fixture->given_chord);
        assert(plan_lane_span_state(fixture->plan, fixture->given_chord) == PLAN_STATE_GIVEN);
        assert(rational_cmp(span.length, bars_of(fixture, scenario->before_bars)) == 0);
        assert(strcmp(plan_lane_span_payload(fixture->plan, fixture->given_chord)->harmony.roman_numeral,
                      scenario->mode == PLAN_MODE_MINOR ? "i" : "I") == 0);
    }
    PlanSpan span = plan_lane_span_span(fixture->plan, fixture->given_bass);
    assert(plan_lane_span_state(fixture->plan, fixture->given_bass) == PLAN_STATE_GIVEN);
    assert(rational_cmp(span.length, fixture->bar_length) == 0);
    assert(plan_lane_span_payload(fixture->plan, fixture->given_bass)->notes.notes[0].midi_pitch == 43);
}

/* --- the schemas --------------------------------------------------------- */

/*
 * Runs one schema, and only that one, on a plan. Returns how many times the schema is in the plan afterwards. A
 * schema that does not fit leaves the target open and the plan as it was.
 */
static int run_schema(const Scenario *scenario, int schema, unsigned roles) {
    static char first[SIGNATURE_SIZE];
    static char second[SIGNATURE_SIZE];
    HarmonySchemaParams params = harmony_schema_params_default();
    params.schemas = HARMONY_SCHEMA_BIT(schema);
    params.roles = roles;
    Operators operators = {&params, NULL, false, false};
    Fixture fixture = build(scenario);
    FillOutcome outcome = run(&fixture, scenario->seed, &operators, NULL);
    int found = check_schemas(&fixture, scenario, &params.bass, schema);
    check_given(&fixture, scenario);
    if (found == 0) {
        /* Nothing fit: the loop reports no valid candidate (or, for a meter it cannot use, no operator), and the
           target and its bass placeholder are as they were. */
        assert(outcome.status == FILL_STATUS_STUCK && outcome.stuck_count >= 1);
        assert(plan_lane_span_state(fixture.plan, fixture.target) == PLAN_STATE_CONSTRAINED);
        assert(plan_lane_span_state(fixture.plan, fixture.bass_target) == PLAN_STATE_CONSTRAINED);
        assert(rational_cmp(plan_lane_span_span(fixture.plan, fixture.target).length, fixture.region.length) == 0);
    }
    fill_outcome_destroy(&outcome);
    signature(&fixture, first, sizeof(first));
    plan_destroy(fixture.plan);

    /* The same seed gives the same result. */
    Fixture again = build(scenario);
    outcome = run(&again, scenario->seed, &operators, NULL);
    signature(&again, second, sizeof(second));
    assert(strcmp(first, second) == 0);
    fill_outcome_destroy(&outcome);
    plan_destroy(again.plan);
    return found;
}

/* The schemas the framework's data makes into chords in the meters that can hold them, found by the sweep below. */
static bool applied_schema[GALANT_SCHEMA_COUNT];

static void test_schema_sweep(void) {
    static const int meters[2][2] = {{4, 4}, {6, 8}};
    static const int tonics[4] = {0, 3, 6, 10};
    for (int schema = 0; schema < GALANT_SCHEMA_COUNT; schema++) {
        for (int m = 0; m < 2; m++) {
            for (int mode = 0; mode < 2; mode++) {
                for (int t = 0; t < 4; t++) {
                    /* As an opening, as a continuation, and filling the end of a phrase exactly. */
                    for (int kind = 0; kind < 6; kind++) {
                        Scenario scenario = {meters[m][0], meters[m][1], mode ? PLAN_MODE_MINOR : PLAN_MODE_MAJOR,
                                             tonics[t], 8, kind == 0 ? 0 : 1, kind == 5 ? CADENCE_TYPE_DECEPTIVE : CADENCE_TYPE_PERFECT_AUTHENTIC,
                                             (unsigned long)(kind + t)};
                        if (kind >= 2) {
                            scenario.bars = 1 + kind - 1; /* a target of 1 to 4 bars */
                        }
                        if (run_schema(&scenario, schema, HARMONY_SCHEMA_ROLE_ALL) > 0) {
                            applied_schema[schema] = true;
                        }
                    }
                }
            }
        }
    }
    int applied = 0;
    for (int schema = 0; schema < GALANT_SCHEMA_COUNT; schema++) {
        applied += applied_schema[schema];
    }
    /* The schemas whose stages make one consistent chord each are offered; the modulating ones and the ones whose
       data does not are not. */
    assert(applied >= 8);
    assert(!applied_schema[GALANT_SCHEMA_MODULATING_PRINNER]);
    assert(!applied_schema[GALANT_SCHEMA_MODULATING_PRINNER_WITH_DOMINANT]);
    assert(!applied_schema[GALANT_SCHEMA_INDUGIO]);
    assert(!applied_schema[GALANT_SCHEMA_QUIESCENZA]);
    assert(!applied_schema[GALANT_SCHEMA_CADENZA_DOPPIA]);
    for (GalantSchemaId id = GALANT_SCHEMA_MEYER; id < GALANT_SCHEMA_COUNT; id++) {
        if (applied_schema[id]) {
            printf("schema applied: %s\n", galant_schema_name(id));
        }
    }
}

/* Why the loop left the harmony target open; the bass placeholder beside it has no operator in these runs. */
static FillStuckReason stuck_reason(const Fixture *fixture, const FillOutcome *outcome) {
    for (size_t i = 0; i < outcome->stuck_count; i++) {
        if (outcome->stuck[i].target.kind == FILL_TARGET_LANE_SPAN && outcome->stuck[i].target.id == fixture->target) {
            return outcome->stuck[i].reason;
        }
    }
    assert(false);
    return FILL_STUCK_NO_OPERATOR;
}

/* A schema that cannot fit is rejected cleanly; a meter without weak beats gets none. */
static void test_unfittable(void) {
    HarmonySchemaParams params = harmony_schema_params_default();
    params.schemas = HARMONY_SCHEMA_BIT(GALANT_SCHEMA_ROMANESCA);
    Operators operators = {&params, NULL, false, false};

    /* Four stages need two bars of 4/4; a target of one bar is too short. */
    Scenario short_target = {4, 4, PLAN_MODE_MAJOR, 0, 2, 1, CADENCE_TYPE_PERFECT_AUTHENTIC, 0};
    Fixture fixture = build(&short_target);
    FillOutcome outcome = run(&fixture, 0, &operators, NULL);
    assert(outcome.status == FILL_STATUS_STUCK);
    assert(stuck_reason(&fixture, &outcome) == FILL_STUCK_ALL_REJECTED);
    assert(plan_lane_span_state(fixture.plan, fixture.target) == PLAN_STATE_CONSTRAINED);
    fill_outcome_destroy(&outcome);
    plan_destroy(fixture.plan);

    /* 3/4 has one chord position a bar, all strong. */
    Scenario waltz = {3, 4, PLAN_MODE_MAJOR, 0, 8, 0, CADENCE_TYPE_PERFECT_AUTHENTIC, 0};
    fixture = build(&waltz);
    outcome = run(&fixture, 0, &operators, NULL);
    assert(outcome.status == FILL_STATUS_STUCK);
    assert(stuck_reason(&fixture, &outcome) == FILL_STUCK_NO_OPERATOR);
    fill_outcome_destroy(&outcome);
    plan_destroy(fixture.plan);

    /* An opening gambit is not offered after the start, and no schema when none is enabled. */
    Scenario later = {4, 4, PLAN_MODE_MAJOR, 0, 8, 1, CADENCE_TYPE_PERFECT_AUTHENTIC, 0};
    fixture = build(&later);
    outcome = run(&fixture, 0, &operators, NULL);
    assert(outcome.status == FILL_STATUS_STUCK && stuck_reason(&fixture, &outcome) == FILL_STUCK_ALL_REJECTED);
    fill_outcome_destroy(&outcome);
    plan_destroy(fixture.plan);
    params.schemas = 0;
    Scenario opening = {4, 4, PLAN_MODE_MAJOR, 0, 8, 0, CADENCE_TYPE_PERFECT_AUTHENTIC, 0};
    fixture = build(&opening);
    outcome = run(&fixture, 0, &operators, NULL);
    assert(outcome.status == FILL_STATUS_STUCK && stuck_reason(&fixture, &outcome) == FILL_STUCK_ALL_REJECTED);
    fill_outcome_destroy(&outcome);
    plan_destroy(fixture.plan);
}

/* --- the schemas and sequences with the plain operators, to the end of the phrase ---------------------------- */

static int schemas_seen;
static int sequences_seen;

static void test_phrases_keep_their_cadence(void) {
    static const CadenceType cadences[4] = {CADENCE_TYPE_PERFECT_AUTHENTIC, CADENCE_TYPE_IMPERFECT_AUTHENTIC, CADENCE_TYPE_HALF, CADENCE_TYPE_DECEPTIVE};
    static const int tonics[2] = {0, 6};
    HarmonySchemaParams schema = harmony_schema_params_default();
    HarmonySequenceParams sequence = harmony_sequence_params_default();
    schema.preference = 1000;
    sequence.preference = 1000;
    Operators operators = {&schema, &sequence, true, true};
    for (int mode = 0; mode < 2; mode++) {
        for (int t = 0; t < 2; t++) {
            for (int c = 0; c < 4; c++) {
                for (int before = 0; before < 2; before++) {
                    for (unsigned long seed = 0; seed < 3; seed++) {
                        Scenario scenario = {4, 4, mode ? PLAN_MODE_MINOR : PLAN_MODE_MAJOR, tonics[t], 8, before,
                                             cadences[c], seed};
                        Fixture fixture = build(&scenario);
                        FillRecord *record = NULL;
                        FillOutcome outcome = run(&fixture, seed, &operators, &record);
                        char labels[SIGNATURE_SIZE];
                        chosen_labels(record, labels, sizeof(labels));
                        bool used_sequence = strstr(labels, "sequence:") != NULL;
                        schemas_seen += strstr(labels, "schema:") != NULL;
                        sequences_seen += used_sequence;

                        /* The harmony is finished; at worst a bass the melodic rules would not take is open. */
                        LaneSpanId ids[MAX_CHORDS];
                        size_t count = plan_lane_find(fixture.plan, PLAN_LANE_HARMONY, 0, fixture.phrase, ids, MAX_CHORDS);
                        for (size_t i = 0; i < count; i++) {
                            assert(plan_lane_span_state(fixture.plan, ids[i]) != PLAN_STATE_CONSTRAINED);
                        }
                        for (size_t i = 0; i < outcome.stuck_count; i++) {
                            assert(outcome.stuck[i].target.kind == FILL_TARGET_LANE_SPAN);
                            assert(plan_lane_span_lane(fixture.plan, (LaneSpanId)outcome.stuck[i].target.id) ==
                                   PLAN_LANE_BASS);
                        }

                        /* The phrase still ends in the cadence asked for, which the recognizer hears. */
                        CadenceType heard;
                        bool at_end;
                        const char *violation = "unread";
                        assert(harmony_hear_cadence(fixture.plan, fixture.phrase, scenario.mode, scenario.cadence,
                                                    &heard, &at_end));
                        assert(at_end);
                        assert((scenario.cadence == CADENCE_TYPE_PERFECT_AUTHENTIC && heard == CADENCE_TYPE_PERFECT_AUTHENTIC) ||
                               (scenario.cadence == CADENCE_TYPE_IMPERFECT_AUTHENTIC && heard == CADENCE_TYPE_IMPERFECT_AUTHENTIC) ||
                               (scenario.cadence == CADENCE_TYPE_HALF && heard == CADENCE_TYPE_HALF) ||
                               (scenario.cadence == CADENCE_TYPE_DECEPTIVE && heard == CADENCE_TYPE_DECEPTIVE));
                        /* Standard-harmony passes it: strictly when every inverted chord has its reason (a schema's
                           has), and for the phrase model alone when a sequence made some. */
                        if (used_sequence) {
                            assert(harmony_check_lane_without_cadence(fixture.plan, fixture.phrase, scenario.mode,
                                                                      &violation));
                        } else {
                            assert(harmony_check_lane(fixture.plan, fixture.phrase, scenario.mode, scenario.cadence,
                                                      &violation));
                        }
                        assert(violation == NULL);
                        check_schemas(&fixture, &scenario, &schema.bass, -1);
                        check_given(&fixture, &scenario);
                        fill_outcome_destroy(&outcome);
                        fill_record_destroy(record);
                        plan_destroy(fixture.plan);
                    }
                }
            }
        }
    }
    assert(schemas_seen > 0);
    assert(sequences_seen > 0);
}

/* --- the sequences ----------------------------------------------------------- */

static bool applied_sequence[SCALE_PATTERN_COUNT];

/* The harmony chords the sequence wrote: from the start of the target to the first span still open. */
static size_t sequence_chords(const Fixture *fixture, LaneSpanId *ids) {
    LaneSpanId found[MAX_CHORDS];
    size_t count = plan_lane_find(fixture->plan, PLAN_LANE_HARMONY, 0, fixture->region, found, MAX_CHORDS);
    size_t used = 0;
    for (size_t i = 0; i < count && plan_lane_span_state(fixture->plan, found[i]) != PLAN_STATE_CONSTRAINED; i++) {
        ids[used++] = found[i];
    }
    return used;
}

static void check_sequence(const Fixture *fixture, const Scenario *scenario, const ScalePattern *pattern,
                           const HarmonySequenceParams *params) {
    LaneSpanId ids[MAX_CHORDS];
    size_t count = sequence_chords(fixture, ids);
    assert(count >= 2);

    /* Standard-counterpoint's melodic rules, on the bass the sequence wrote. */
    PlanSpan first = plan_lane_span_span(fixture->plan, ids[0]);
    PlanSpan last = plan_lane_span_span(fixture->plan, ids[count - 1]);
    PlanSpan whole = {first.start, time_sub(time_plus(last.start, last.length), first.start)};
    PlanKey key = {scenario->tonic, scenario->mode};
    const char *violation = "unread";
    assert(harmony_check_bass(fixture->plan, whole, key, &params->bass, &violation));
    assert(violation == NULL);

    /* The bass runs by step in the pattern's direction, and the chords are the ones the pattern's intervals above the
       bass make: a root position triad for a fifth, a first inversion triad for a sixth or a third. */
    unsigned allowed = 0;
    for (size_t s = 0; s < pattern->unit_length; s++) {
        allowed |= pattern->unit[s].intervals;
    }
    int step_seen = 0;
    int previous_pc = -1;
    for (size_t i = 0; i < count; i++) {
        const char *roman = plan_lane_span_payload(fixture->plan, ids[i])->harmony.roman_numeral;
        HarmonyChord chord;
        assert(!is_applied(roman));
        assert(harmony_roman_parse(scenario->mode, roman, &chord));
        assert(!chord.seventh);
        assert(chord.inversion == 0 ? (allowed & (1u << 5)) != 0 : (allowed & ((1u << 6) | (1u << 3))) != 0);
        int pc = bass_pc_under(fixture, ids[i]);
        if (previous_pc >= 0 && pc != previous_pc) {
            int up = (pc - previous_pc + 12) % 12;
            int down = (previous_pc - pc + 12) % 12;
            assert(up <= 2 || down <= 2); /* a step of the scale */
            int direction = up <= 2 ? 1 : -1;
            if (!pattern->both_directions) {
                assert(direction == (pattern->direction == SCALE_DIRECTION_ASCENDING ? 1 : -1));
            }
            assert(step_seen == 0 || step_seen == direction);
            step_seen = direction;
        }
        previous_pc = pc;
    }
}

/*
 * Applies each candidate the sequence operator proposes for the target, one at a time on a fresh plan (the loop would
 * go on to apply the operator again to what is left), and checks what was written.
 */
static void test_sequence_sweep(void) {
    static const int meters[4][2] = {{2, 4}, {3, 4}, {4, 4}, {6, 8}};
    static const int tonics[2] = {0, 6};
    for (int id = 0; id < SCALE_PATTERN_COUNT; id++) {
        const ScalePattern *pattern = scale_pattern((ScalePatternId)id);
        HarmonySequenceParams params = harmony_sequence_params_default();
        params.patterns = HARMONY_SEQUENCE_BIT(id);
        for (int m = 0; m < 4; m++) {
            for (int mode = 0; mode < 2; mode++) {
                for (int t = 0; t < 2; t++) {
                    for (unsigned long seed = 0; seed < 4; seed++) {
                        Scenario scenario = {meters[m][0], meters[m][1], mode ? PLAN_MODE_MINOR : PLAN_MODE_MAJOR,
                                             tonics[t], 9, 1, CADENCE_TYPE_PERFECT_AUTHENTIC, seed};
                        FillConfig config;
                        memset(&config, 0, sizeof(config));
                        config.seed = seed;
                        config.checker = harmony_phrase_checker;
                        for (size_t pick = 0;; pick++) {
                            static char first[SIGNATURE_SIZE];
                            static char second[SIGNATURE_SIZE];
                            size_t count = 0;
                            for (int pass = 0; pass < 2; pass++) {
                                Fixture fixture = build(&scenario);
                                FillOperatorRegistry *registry = fill_registry_create();
                                FillSuggestions *suggestions = NULL;
                                FillTarget target = {FILL_TARGET_LANE_SPAN, fixture.target};
                                assert(harmony_sequence_register(registry, &params) == FILL_OK);
                                assert(fill_suggest(fixture.plan, registry, &config, target, 8, &suggestions) == FILL_OK);
                                count = fill_suggestions_count(suggestions);
                                if (pick < count) {
                                    char wanted[FILL_LABEL_MAX];
                                    snprintf(wanted, sizeof(wanted), "sequence:%s", pattern->name);
                                    assert(strncmp(fill_suggestion_label(suggestions, pick), wanted, strlen(wanted)) == 0);
                                    assert(fill_apply_suggestion(fixture.plan, suggestions, pick) == FILL_OK);
                                    if (pass == 0) {
                                        applied_sequence[id] = true;
                                        check_sequence(&fixture, &scenario, pattern, &params);
                                        check_given(&fixture, &scenario);
                                        signature(&fixture, first, sizeof(first));
                                    } else {
                                        /* The same seed gives the same result. */
                                        signature(&fixture, second, sizeof(second));
                                        assert(strcmp(first, second) == 0);
                                    }
                                }
                                fill_suggestions_destroy(suggestions);
                                fill_registry_destroy(registry);
                                plan_destroy(fixture.plan);
                            }
                            if (pick + 1 >= count) {
                                break;
                            }
                        }
                    }
                }
            }
        }
    }
    for (int id = 0; id < SCALE_PATTERN_COUNT; id++) {
        printf("sequence applied: %s %d\n", scale_pattern_name((ScalePatternId)id), (int)applied_sequence[id]);
    }
}

int main(void) {
    test_schema_sweep();
    test_unfittable();
    test_sequence_sweep();
    test_phrases_keep_their_cadence();
    return 0;
}
