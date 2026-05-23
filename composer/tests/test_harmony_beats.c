#include <assert.h>
#include <stdio.h>
#include <string.h>

#include <ursatz_composer/constraint.h>
#include <ursatz_composer/fill_loop.h>
#include <ursatz_composer/harmony_bass.h>
#include <ursatz_composer/harmony_embellish.h>
#include <ursatz_composer/harmony_prolong.h>
#include <ursatz_composer/harmony_schema.h>
#include <ursatz_composer/harmony_sequence.h>
#include <ursatz_composer/harmony_octave.h>
#include <ursatz_composer/harmony_phrase.h>
#include <ursatz_composer/plan_state.h>

/*
 * Harmony and bass over spans that start and end on any beat (R-168), run through the fill loop with the checker. Beats
 * are counted from the start of the meter lane, which starts on a barline; the expectations are worked out in beats, apart
 * from the code under test.
 */

#define MAX_CHORDS 32
#define FORM_BARS 12

typedef struct {
    int beats_per_bar;
    int beat_unit;
    PlanKeyMode mode;
    int tonic;
    int start;  /* the span's start, in beats from the start of the meter lane */
    int length; /* the span's length, in beats */
    PlanCadenceType cadence;
    unsigned long seed;
} Scenario;

typedef struct {
    PiecePlan *plan;
    NodeId owner;
    LaneSpanId target;
    LaneSpanId bass;
    PlanSpan span;
} Fixture;

static PlanTime fraction(long num, unsigned long den) {
    PlanTime time;
    assert(plan_time_make(num, den, &time) == PLAN_OK);
    return time;
}

static PlanTime beats(const Scenario *scenario, long count) {
    return fraction(count, (unsigned long)scenario->beat_unit);
}

static size_t cells_per_bar(const Scenario *scenario) {
    return (scenario->beat_unit == 4 && scenario->beats_per_bar <= 3) ? 1 : 2;
}

/* The beats from one place a chord may start to the next. */
static int cell_beats(const Scenario *scenario) {
    return scenario->beats_per_bar / (int)cells_per_bar(scenario);
}

typedef struct {
    int starts[64]; /* the beat each cell starts on, then the span's end */
    size_t cells;
} Cells;

static Cells cells_of(const Scenario *scenario) {
    Cells cells;
    int end = scenario->start + scenario->length;
    cells.cells = 0;
    cells.starts[cells.cells++] = scenario->start;
    for (int beat = (scenario->start / cell_beats(scenario) + 1) * cell_beats(scenario); beat < end;
         beat += cell_beats(scenario)) {
        cells.starts[cells.cells++] = beat;
    }
    cells.starts[cells.cells] = end;
    return cells;
}

/* The beat the last chord starts on: the last downbeat the span holds, else the first cell boundary after the lead. */
static int expected_last(const Scenario *scenario, const Cells *cells) {
    int end = scenario->start + scenario->length;
    int last_barline = (end - 1) / scenario->beats_per_bar * scenario->beats_per_bar;
    if (last_barline >= scenario->start) {
        return last_barline;
    }
    return cells->cells > 1 && scenario->start % cell_beats(scenario) != 0 ? cells->starts[1] : scenario->start;
}

/*
 * The cells before the last chord must hold the chords the cadence needs before its last. A cadence on a last chord that
 * does not start on a downbeat need not be heard, so the phrase may be stuck.
 */
static bool expect_progression(const Scenario *scenario, const Cells *cells) {
    int last = expected_last(scenario, cells);
    if (last % scenario->beats_per_bar != 0) {
        return false;
    }
    size_t room = 0;
    while (room < cells->cells && cells->starts[room] < last) {
        room++;
    }
    size_t fixed = 1 + (scenario->cadence == PLAN_CADENCE_HC ? 0 : 1) + (scenario->cadence == PLAN_CADENCE_PC ? 2 : 0);
    return room >= fixed;
}

static Fixture build(const Scenario *scenario, bool with_bass) {
    Fixture fixture;
    memset(&fixture, 0, sizeof(fixture));
    fixture.plan = plan_create();
    PlanSpan form = {fraction(0, 1), beats(scenario, (long)FORM_BARS * scenario->beats_per_bar)};
    fixture.span.start = beats(scenario, scenario->start);
    fixture.span.length = beats(scenario, scenario->length);

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

    memset(&payload, 0, sizeof(payload));
    strcpy(payload.harmony.roman_numeral, "I");
    payload.harmony.function = PLAN_FUNCTION_UNKNOWN;
    assert(plan_add_lane_span(fixture.plan, fixture.owner, PLAN_LANE_HARMONY, 0, fixture.span, &payload,
                              &fixture.target) == PLAN_OK);
    PlanConstraint cadence = {PLAN_CONSTRAINT_CADENCE, {0}};
    cadence.params.cadence = scenario->cadence;
    assert(plan_lane_span_add_constraint(fixture.plan, fixture.target, &cadence) == PLAN_OK);

    if (with_bass) {
        PlanNote note;
        note.midi_pitch = 60;
        note.offset = fraction(0, 1);
        note.duration = fixture.span.length;
        memset(&payload, 0, sizeof(payload));
        payload.notes.notes = &note;
        payload.notes.note_count = 1;
        assert(plan_add_lane_span(fixture.plan, fixture.owner, PLAN_LANE_BASS, 0, fixture.span, &payload,
                                  &fixture.bass) == PLAN_OK);
    }
    return fixture;
}

static FillOutcome run(const Fixture *fixture, unsigned long seed, bool with_bass) {
    FillOperatorRegistry *registry = fill_registry_create();
    FillConfig config;
    FillOutcome outcome;
    memset(&config, 0, sizeof(config));
    config.seed = seed;
    config.checker = harmony_phrase_checker;
    assert(harmony_phrase_register(registry) == FILL_OK);
    HarmonyBassParams params = harmony_bass_params_default();
    if (with_bass) {
        assert(harmony_bass_register(registry, &params) == FILL_OK);
    }
    assert(fill_run(fixture->plan, registry, &config, &outcome, NULL) == FILL_OK);
    fill_registry_destroy(registry);
    return outcome;
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

/* Chord spans of a lane tile the span, each starting on a place a chord may start; the last starts where it should. */
static size_t check_lane_tiles(const Fixture *fixture, const Scenario *scenario, const Cells *cells, PlanLaneKind lane,
                               LaneSpanId *ids) {
    size_t count = plan_lane_find(fixture->plan, lane, 0, fixture->span, ids, MAX_CHORDS);
    assert(count >= 1 && count <= MAX_CHORDS);
    PlanTime position = fixture->span.start;
    for (size_t i = 0; i < count; i++) {
        PlanSpan span = plan_lane_span_span(fixture->plan, ids[i]);
        assert(plan_time_compare(span.start, position) == 0);
        bool on_cell = false;
        for (size_t c = 0; c < cells->cells; c++) {
            on_cell = on_cell || plan_time_compare(span.start, beats(scenario, cells->starts[c])) == 0;
        }
        assert(on_cell);
        PlanTime end;
        assert(plan_span_end(span, &end) == PLAN_OK);
        position = end;
    }
    PlanTime span_end;
    assert(plan_span_end(fixture->span, &span_end) == PLAN_OK);
    assert(plan_time_compare(position, span_end) == 0);
    return count;
}

static void check_harmony(const Fixture *fixture, const Scenario *scenario, const Cells *cells) {
    LaneSpanId ids[MAX_CHORDS];
    size_t count = check_lane_tiles(fixture, scenario, cells, PLAN_LANE_HARMONY, ids);
    for (size_t i = 0; i < count; i++) {
        assert(plan_lane_span_state(fixture->plan, ids[i]) == PLAN_STATE_REALIZED);
    }
    PlanSpan last = plan_lane_span_span(fixture->plan, ids[count - 1]);
    assert(plan_time_compare(last.start, beats(scenario, expected_last(scenario, cells))) == 0);
    assert(plan_lane_span_constraint_count(fixture->plan, ids[count - 1]) == 1);

    CadenceType heard;
    bool at_end;
    assert(harmony_hear_cadence(fixture->plan, fixture->span, scenario->mode, scenario->cadence, &heard, &at_end));
    assert(heard == expected_type(scenario->cadence));
    assert(at_end);
    const char *violation = "unread";
    assert(harmony_check_lane(fixture->plan, fixture->span, scenario->mode, scenario->cadence, &violation));
    assert(violation == NULL);
}

/* Writes the chords as text, so two runs can be compared. */
static void signature(const Fixture *fixture, char *out, size_t size) {
    LaneSpanId ids[MAX_CHORDS];
    size_t count = plan_lane_find(fixture->plan, PLAN_LANE_HARMONY, 0, fixture->span, ids, MAX_CHORDS);
    size_t used = 0;
    out[0] = '\0';
    for (size_t i = 0; i < count; i++) {
        PlanSpan span = plan_lane_span_span(fixture->plan, ids[i]);
        int written = snprintf(out + used, size - used, "%s@%ld/%lu+%ld/%lu ",
                               plan_lane_span_payload(fixture->plan, ids[i])->harmony.roman_numeral, span.start.num,
                               span.start.den, span.length.num, span.length.den);
        assert(written > 0 && (size_t)written < size - used);
        used += (size_t)written;
    }
}

static void check_scenario(const Scenario *scenario) {
    Fixture fixture = build(scenario, false);
    Cells cells = cells_of(scenario);
    FillOutcome outcome = run(&fixture, scenario->seed, false);
    if (outcome.status == FILL_STATUS_STUCK) {
        assert(!expect_progression(scenario, &cells));
        assert(outcome.stuck_count == 1 && outcome.stuck[0].reason == FILL_STUCK_ALL_REJECTED);
        assert(plan_lane_span_state(fixture.plan, fixture.target) == PLAN_STATE_CONSTRAINED);
    } else {
        assert(outcome.status == FILL_STATUS_COMPLETE);
        check_harmony(&fixture, scenario, &cells);
        char first[512];
        char second[512];
        signature(&fixture, first, sizeof(first));
        Fixture again = build(scenario, false);
        FillOutcome again_outcome = run(&again, scenario->seed, false);
        assert(again_outcome.status == FILL_STATUS_COMPLETE);
        signature(&again, second, sizeof(second));
        assert(strcmp(first, second) == 0);
        fill_outcome_destroy(&again_outcome);
        plan_destroy(again.plan);
    }
    fill_outcome_destroy(&outcome);
    plan_destroy(fixture.plan);
}

/* In 4/4, spans of 6, 7 and 10 beats from beat 1 and from beat 3, every cadence. */
static void test_four_four(void) {
    static const int lengths[3] = {6, 7, 10};
    static const int starts[2] = {0, 2};
    static const PlanCadenceType cadences[5] = {PLAN_CADENCE_PAC, PLAN_CADENCE_IAC, PLAN_CADENCE_HC, PLAN_CADENCE_DC,
                                                PLAN_CADENCE_PC};
    size_t produced = 0;
    for (int mode = 0; mode < 2; mode++) {
        for (int l = 0; l < 3; l++) {
            for (int s = 0; s < 2; s++) {
                for (int c = 0; c < 5; c++) {
                    for (unsigned long seed = 0; seed < 4; seed++) {
                        Scenario scenario = {4, 4, mode ? PLAN_MODE_MINOR : PLAN_MODE_MAJOR, mode ? 9 : 2,
                                             starts[s], lengths[l], cadences[c], seed};
                        Cells cells = cells_of(&scenario);
                        produced += expect_progression(&scenario, &cells);
                        check_scenario(&scenario);
                    }
                }
            }
        }
    }
    assert(produced > 0);
}

/* Spans starting on beat 2, in 3/4 and in 6/8, and in 4/4 and 2/4 as well, with lengths that end between cells. */
static void test_off_cell_starts(void) {
    static const int meters[4][2] = {{2, 4}, {3, 4}, {4, 4}, {6, 8}};
    static const PlanCadenceType cadences[3] = {PLAN_CADENCE_PAC, PLAN_CADENCE_HC, PLAN_CADENCE_DC};
    for (int m = 0; m < 4; m++) {
        for (int start = 1; start < meters[m][0]; start++) {
            for (int length = 2; length <= 3 * meters[m][0] + 1; length++) {
                for (int c = 0; c < 3; c++) {
                    for (unsigned long seed = 0; seed < 2; seed++) {
                        Scenario scenario = {meters[m][0], meters[m][1], PLAN_MODE_MAJOR, 7, start, length,
                                             cadences[c], seed};
                        check_scenario(&scenario);
                    }
                }
            }
        }
    }
}

/* A span that ends where a bar does, started on beat 2: the lead is its first cell. */
static void test_lead_and_tail(void) {
    Scenario scenario = {4, 4, PLAN_MODE_MAJOR, 0, 1, 11, PLAN_CADENCE_PAC, 1};
    Fixture fixture = build(&scenario, false);
    Cells cells = cells_of(&scenario);
    /* Beats 2 to 12: a lead of one beat, then whole cells to the barline of beat 12, and no tail. */
    assert(cells.cells == 1 + 5);
    FillOutcome outcome = run(&fixture, 1, false);
    assert(outcome.status == FILL_STATUS_COMPLETE);
    check_harmony(&fixture, &scenario, &cells);
    LaneSpanId ids[MAX_CHORDS];
    size_t count = plan_lane_find(fixture.plan, PLAN_LANE_HARMONY, 0, fixture.span, ids, MAX_CHORDS);
    /* The first chord starts on the lead, one beat before the first whole cell. */
    assert(plan_time_compare(plan_lane_span_span(fixture.plan, ids[0]).start, beats(&scenario, 1)) == 0);
    assert(count >= 2);
    fill_outcome_destroy(&outcome);
    plan_destroy(fixture.plan);

    /* A tail: beats 1 to 8 end a beat into the second bar's second cell. */
    Scenario tailed = {4, 4, PLAN_MODE_MAJOR, 0, 0, 7, PLAN_CADENCE_HC, 2};
    fixture = build(&tailed, false);
    cells = cells_of(&tailed);
    assert(cells.cells == 4);
    outcome = run(&fixture, 2, false);
    assert(outcome.status == FILL_STATUS_COMPLETE);
    check_harmony(&fixture, &tailed, &cells);
    fill_outcome_destroy(&outcome);
    plan_destroy(fixture.plan);
}

/* The fewest cells a cadence fits in, and a span with fewer is stuck. */
static void test_minimum_cells(void) {
    PlanMeter four;
    PlanMeter six_eight;
    PlanMeter odd;
    memset(&four, 0, sizeof(four));
    memset(&six_eight, 0, sizeof(six_eight));
    memset(&odd, 0, sizeof(odd));
    four.beats_per_bar = 4;
    four.beat_unit = 4;
    six_eight.beats_per_bar = 6;
    six_eight.beat_unit = 8;
    odd.beats_per_bar = 5;
    odd.beat_unit = 4;
    assert(harmony_phrase_min_cells(PLAN_CADENCE_PAC, &four) == 3);
    assert(harmony_phrase_min_cells(PLAN_CADENCE_HC, &four) == 2);
    assert(harmony_phrase_min_cells(PLAN_CADENCE_PC, &six_eight) == 5);
    assert(harmony_phrase_min_cells(PLAN_CADENCE_PAC, &odd) == 0);
    /* A phrase of whole bars still ends in a whole bar. */
    assert(harmony_phrase_min_bars(PLAN_CADENCE_PAC, &four) == 2);
    assert(harmony_phrase_min_bars(PLAN_CADENCE_PC, &four) == 3);

    static const PlanCadenceType cadences[5] = {PLAN_CADENCE_PAC, PLAN_CADENCE_IAC, PLAN_CADENCE_HC, PLAN_CADENCE_DC,
                                                PLAN_CADENCE_PC};
    for (int c = 0; c < 5; c++) {
        size_t minimum = harmony_phrase_min_cells(cadences[c], &four);
        for (int start = 0; start < 4; start++) {
            for (int length = 1; length <= 14; length++) {
                Scenario scenario = {4, 4, PLAN_MODE_MAJOR, 0, start, length, cadences[c], 0};
                Cells cells = cells_of(&scenario);
                if (cells.cells < minimum) {
                    Fixture fixture = build(&scenario, false);
                    FillOutcome outcome = run(&fixture, 0, false);
                    assert(outcome.status == FILL_STATUS_STUCK);
                    assert(plan_lane_span_state(fixture.plan, fixture.target) == PLAN_STATE_CONSTRAINED);
                    fill_outcome_destroy(&outcome);
                    plan_destroy(fixture.plan);
                }
            }
        }
    }
}

/* A span that starts or ends between beats is not a target. */
static void test_off_beat_is_not_a_target(void) {
    /* An eighth note after the barline, in 4/4. */
    Fixture fixture;
    memset(&fixture, 0, sizeof(fixture));
    PlanSpan off_start = {fraction(1, 8), fraction(2, 1)};
    fixture.plan = plan_create();
    PlanSpan form = {fraction(0, 1), fraction(4, 1)};
    assert(plan_add_node(fixture.plan, PLAN_NODE_NONE, PLAN_NODE_FORM, form, NULL, &fixture.owner) == PLAN_OK);
    assert(plan_node_set_state(fixture.plan, fixture.owner, PLAN_STATE_GIVEN) == PLAN_OK);
    PlanLanePayload payload;
    LaneSpanId id;
    memset(&payload, 0, sizeof(payload));
    payload.meter.beats_per_bar = 4;
    payload.meter.beat_unit = 4;
    assert(plan_add_lane_span(fixture.plan, fixture.owner, PLAN_LANE_METER, 0, form, &payload, &id) == PLAN_OK);
    assert(plan_lane_span_set_state(fixture.plan, id, PLAN_STATE_GIVEN) == PLAN_OK);
    memset(&payload, 0, sizeof(payload));
    payload.key.mode = PLAN_MODE_MAJOR;
    assert(plan_add_lane_span(fixture.plan, fixture.owner, PLAN_LANE_KEY, 0, form, &payload, &id) == PLAN_OK);
    assert(plan_lane_span_set_state(fixture.plan, id, PLAN_STATE_GIVEN) == PLAN_OK);
    memset(&payload, 0, sizeof(payload));
    strcpy(payload.harmony.roman_numeral, "I");
    assert(plan_add_lane_span(fixture.plan, fixture.owner, PLAN_LANE_HARMONY, 0, off_start, &payload,
                              &fixture.target) == PLAN_OK);
    PlanConstraint cadence = {PLAN_CONSTRAINT_CADENCE, {0}};
    cadence.params.cadence = PLAN_CADENCE_PAC;
    assert(plan_lane_span_add_constraint(fixture.plan, fixture.target, &cadence) == PLAN_OK);
    FillOutcome outcome = run(&fixture, 0, false);
    assert(outcome.status == FILL_STATUS_STUCK);
    assert(outcome.stuck_count == 1 && outcome.stuck[0].reason == FILL_STUCK_NO_OPERATOR);
    fill_outcome_destroy(&outcome);
    plan_destroy(fixture.plan);
}

/* The bass under a 7-beat span: a note or more per chord, in the spans of the chords, passing the bass checker. */
static void test_bass_under_beat_spans(void) {
    static const int starts[3] = {0, 1, 2};
    static const int lengths[3] = {7, 9, 10};
    for (int s = 0; s < 3; s++) {
        for (int l = 0; l < 3; l++) {
            for (unsigned long seed = 0; seed < 4; seed++) {
                Scenario scenario = {4, 4, PLAN_MODE_MAJOR, 5, starts[s], lengths[l], PLAN_CADENCE_PAC, seed};
                Cells cells = cells_of(&scenario);
                if (!expect_progression(&scenario, &cells)) {
                    continue;
                }
                Fixture fixture = build(&scenario, true);
                FillOutcome outcome = run(&fixture, seed, true);
                assert(outcome.status == FILL_STATUS_COMPLETE);
                check_harmony(&fixture, &scenario, &cells);
                LaneSpanId chords[MAX_CHORDS];
                LaneSpanId basses[MAX_CHORDS];
                size_t chord_count = plan_lane_find(fixture.plan, PLAN_LANE_HARMONY, 0, fixture.span, chords, MAX_CHORDS);
                size_t bass_count = check_lane_tiles(&fixture, &scenario, &cells, PLAN_LANE_BASS, basses);
                assert(bass_count == chord_count);
                for (size_t i = 0; i < chord_count; i++) {
                    PlanSpan chord = plan_lane_span_span(fixture.plan, chords[i]);
                    PlanSpan bass = plan_lane_span_span(fixture.plan, basses[i]);
                    assert(plan_time_compare(chord.start, bass.start) == 0);
                    assert(plan_time_compare(chord.length, bass.length) == 0);
                    const PlanNotes *notes = &plan_lane_span_payload(fixture.plan, basses[i])->notes;
                    assert(notes->note_count >= 1);
                    /* The notes run from the chord's start to its end. */
                    PlanTime position = fraction(0, 1);
                    for (size_t n = 0; n < notes->note_count; n++) {
                        assert(plan_time_compare(notes->notes[n].offset, position) == 0);
                        assert(plan_time_add(position, notes->notes[n].duration, &position) == PLAN_OK);
                    }
                    assert(plan_time_compare(position, chord.length) == 0);
                    /* A chord of one beat gets one note. */
                    if (plan_time_compare(chord.length, beats(&scenario, 1)) == 0) {
                        assert(notes->note_count == 1);
                    }
                }
                fill_outcome_destroy(&outcome);
                plan_destroy(fixture.plan);
            }
        }
    }
}

/* The lane tiles the span on cells, and passes the harmony checks with the cadence still heard at its end. */
static void check_lane_valid(const Fixture *fixture, const Scenario *scenario, const Cells *cells) {
    LaneSpanId ids[MAX_CHORDS];
    check_lane_tiles(fixture, scenario, cells, PLAN_LANE_HARMONY, ids);
    CadenceType heard;
    bool at_end;
    assert(harmony_hear_cadence(fixture->plan, fixture->span, scenario->mode, scenario->cadence, &heard, &at_end));
    assert(heard == expected_type(scenario->cadence));
    assert(at_end);
    const char *violation = "unread";
    assert(harmony_check_lane(fixture->plan, fixture->span, scenario->mode, scenario->cadence, &violation));
    assert(violation == NULL);
}

/* The embellishment operator over a progression with part cells: it rewrites chords on the cells it has. */
static void test_embellish_part_cells(void) {
    static const int spans[5][2] = {{0, 7}, {0, 10}, {1, 11}, {2, 10}, {3, 12}};
    size_t changed = 0;
    for (int w = 0; w < 5; w++) {
        for (unsigned long seed = 0; seed < 12; seed++) {
            Scenario scenario = {4, 4, PLAN_MODE_MAJOR, 0, spans[w][0], spans[w][1], PLAN_CADENCE_PAC, seed};
            Cells cells = cells_of(&scenario);
            if (!expect_progression(&scenario, &cells)) {
                continue;
            }
            Fixture fixture = build(&scenario, false);
            FillOutcome outcome = run(&fixture, seed, false);
            assert(outcome.status == FILL_STATUS_COMPLETE);
            fill_outcome_destroy(&outcome);
            LaneSpanId before[MAX_CHORDS];
            size_t before_count = plan_lane_find(fixture.plan, PLAN_LANE_HARMONY, 0, fixture.span, before, MAX_CHORDS);

            assert(harmony_embellish_open(fixture.plan, fixture.span) == PLAN_OK);
            HarmonyEmbellishParams params = {1000, HARMONY_EMBELLISH_ALL};
            FillOperatorRegistry *registry = fill_registry_create();
            FillConfig config;
            memset(&config, 0, sizeof(config));
            config.seed = seed;
            config.checker = harmony_phrase_checker;
            assert(harmony_embellish_register(registry, &params) == FILL_OK);
            assert(fill_run(fixture.plan, registry, &config, &outcome, NULL) == FILL_OK);
            assert(outcome.status == FILL_STATUS_COMPLETE);
            fill_outcome_destroy(&outcome);
            fill_registry_destroy(registry);

            check_lane_valid(&fixture, &scenario, &cells);
            LaneSpanId after[MAX_CHORDS];
            changed += plan_lane_find(fixture.plan, PLAN_LANE_HARMONY, 0, fixture.span, after, MAX_CHORDS) != before_count;
            plan_destroy(fixture.plan);
        }
    }
    assert(changed > 0);
}

/* The prolongation operator over spans with part cells: chords on the cells, no cadence, the last V off a downbeat. */
static void test_prolongation_part_cells(void) {
    static const int spans[6][2] = {{0, 6}, {0, 7}, {1, 7}, {2, 9}, {1, 11}, {3, 5}};
    static const PlanHarmonyAnchor ends[2] = {PLAN_HARMONY_ANCHOR_TONIC, PLAN_HARMONY_ANCHOR_DOMINANT};
    size_t completed = 0;
    for (int w = 0; w < 6; w++) {
        for (int e = 0; e < 2; e++) {
            for (unsigned long seed = 0; seed < 4; seed++) {
                Scenario scenario = {4, 4, PLAN_MODE_MAJOR, 0, spans[w][0], spans[w][1], PLAN_CADENCE_HC, seed};
                Cells cells = cells_of(&scenario);
                Fixture fixture = build(&scenario, false);
                /* The cadence is not asked for: a prolongation is the shape. */
                assert(plan_lane_span_remove_constraint(fixture.plan, fixture.target, 0) == PLAN_OK);
                PlanConstraint shape = {PLAN_CONSTRAINT_HARMONY_SHAPE, {0}};
                shape.params.harmony_shape.kind = PLAN_HARMONY_SHAPE_PROLONGATION;
                shape.params.harmony_shape.starts_on = PLAN_HARMONY_ANCHOR_TONIC;
                shape.params.harmony_shape.ends_on = ends[e];
                assert(plan_lane_span_add_constraint(fixture.plan, fixture.target, &shape) == PLAN_OK);

                FillOperatorRegistry *registry = fill_registry_create();
                FillConfig config;
                FillOutcome outcome;
                memset(&config, 0, sizeof(config));
                config.seed = seed;
                config.checker = harmony_prolong_checker;
                assert(harmony_prolong_register(registry) == FILL_OK);
                assert(fill_run(fixture.plan, registry, &config, &outcome, NULL) == FILL_OK);
                if (outcome.status == FILL_STATUS_COMPLETE) {
                    LaneSpanId ids[MAX_CHORDS];
                    check_lane_tiles(&fixture, &scenario, &cells, PLAN_LANE_HARMONY, ids);
                    completed++;
                }
                fill_outcome_destroy(&outcome);
                fill_registry_destroy(registry);
                plan_destroy(fixture.plan);
            }
        }
    }
    assert(completed > 0);
}

/* The schema and sequence operators need whole cells: a span with a lead or a tail is not theirs. */
static void test_schemas_skip_part_cells(void) {
    static const int spans[4][2] = {{1, 11}, {0, 7}, {3, 10}, {2, 7}};
    for (int w = 0; w < 4; w++) {
        Scenario scenario = {4, 4, PLAN_MODE_MAJOR, 0, spans[w][0], spans[w][1], PLAN_CADENCE_PAC, 0};
        Fixture fixture = build(&scenario, true);
        FillOperatorRegistry *registry = fill_registry_create();
        FillConfig config;
        FillOutcome outcome;
        memset(&config, 0, sizeof(config));
        config.checker = harmony_phrase_checker;
        assert(harmony_schema_register(registry, NULL) == FILL_OK);
        assert(harmony_sequence_register(registry, NULL) == FILL_OK);
        assert(fill_run(fixture.plan, registry, &config, &outcome, NULL) == FILL_OK);
        assert(outcome.status == FILL_STATUS_STUCK);
        assert(outcome.stuck_count >= 1);
        assert(outcome.stuck[0].reason == FILL_STUCK_NO_OPERATOR);
        assert(plan_lane_span_state(fixture.plan, fixture.target) == PLAN_STATE_CONSTRAINED);
        fill_outcome_destroy(&outcome);
        fill_registry_destroy(registry);
        plan_destroy(fixture.plan);
    }
}

/* The harmony from a Given bass whose notes last whole cells from a span that starts and ends between cells. */
static void test_octave_on_beat_spans(void) {
    /* Tonic, step up, tonic: the lengths in beats of the three notes, for spans of 7 beats from beat 1 and beat 2. */
    static const struct {
        int start;
        int lengths[3];
    } SPANS[] = {{0, {4, 2, 1}}, {1, {1, 4, 2}}, {2, {2, 4, 1}}};
    static const int PITCHES[3] = {48, 50, 48};
    for (size_t w = 0; w < sizeof(SPANS) / sizeof(SPANS[0]); w++) {
        for (unsigned long seed = 0; seed < 4; seed++) {
            Scenario scenario = {4, 4, PLAN_MODE_MAJOR, 0, SPANS[w].start, 7, PLAN_CADENCE_PAC, seed};
            Fixture fixture = build(&scenario, false);
            PlanNote notes[3];
            PlanTime position = fraction(0, 1);
            for (int n = 0; n < 3; n++) {
                notes[n].midi_pitch = PITCHES[n];
                notes[n].offset = position;
                notes[n].duration = beats(&scenario, SPANS[w].lengths[n]);
                assert(plan_time_add(position, notes[n].duration, &position) == PLAN_OK);
            }
            PlanLanePayload payload;
            LaneSpanId bass;
            memset(&payload, 0, sizeof(payload));
            payload.notes.notes = notes;
            payload.notes.note_count = 3;
            assert(plan_add_lane_span(fixture.plan, fixture.owner, PLAN_LANE_BASS, 0, fixture.span, &payload,
                                      &bass) == PLAN_OK);
            assert(plan_lane_span_set_state(fixture.plan, bass, PLAN_STATE_GIVEN) == PLAN_OK);
            assert(plan_lane_span_remove_constraint(fixture.plan, fixture.target, 0) == PLAN_OK);

            FillOperatorRegistry *registry = fill_registry_create();
            FillConfig config;
            FillOutcome outcome;
            memset(&config, 0, sizeof(config));
            config.seed = seed;
            config.checker = harmony_phrase_checker;
            assert(harmony_octave_register(registry) == FILL_OK);
            assert(fill_run(fixture.plan, registry, &config, &outcome, NULL) == FILL_OK);
            assert(outcome.status == FILL_STATUS_COMPLETE);
            /* One chord for each note, on the notes' spans. */
            LaneSpanId ids[MAX_CHORDS];
            Cells cells = cells_of(&scenario);
            assert(check_lane_tiles(&fixture, &scenario, &cells, PLAN_LANE_HARMONY, ids) == 3);
            PlanTime at = fixture.span.start;
            for (int n = 0; n < 3; n++) {
                PlanSpan chord = plan_lane_span_span(fixture.plan, ids[n]);
                assert(plan_time_compare(chord.start, at) == 0);
                assert(plan_time_compare(chord.length, beats(&scenario, SPANS[w].lengths[n])) == 0);
                assert(plan_time_add(at, chord.length, &at) == PLAN_OK);
            }
            fill_outcome_destroy(&outcome);
            fill_registry_destroy(registry);
            plan_destroy(fixture.plan);
        }
    }
}

int main(void) {
    test_four_four();
    test_off_cell_starts();
    test_lead_and_tail();
    test_minimum_cells();
    test_off_beat_is_not_a_target();
    test_embellish_part_cells();
    test_prolongation_part_cells();
    test_schemas_skip_part_cells();
    test_bass_under_beat_spans();
    test_octave_on_beat_spans();
    puts("test_harmony_beats ok");
    return 0;
}
