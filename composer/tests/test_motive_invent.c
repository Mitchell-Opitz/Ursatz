#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <ursatz_composer/constraint.h>
#include <ursatz_composer/fill_loop.h>
#include <ursatz_composer/fill_record.h>
#include <ursatz_composer/lanes.h>
#include <ursatz_composer/motive_develop.h>
#include <ursatz_composer/piece_plan.h>
#include <ursatz_composer/plan_state.h>

/*
 * Motive invention: in 3/4, 4/4 and 6/8, seeds 1 to 20, motive-invent fills the open one-bar melody span of m0 with a
 * motive of 3 to 6 notes that fills the bar, opens on degree 1, 3 or 5, keeps to C4 to C6, moves by steps and leaps of
 * at most a sixth, and passes the checker's melodic rules. The same seed gives the same motive and the same record.
 */

typedef struct {
    const char *name;
    int beats;
    int unit;
    int tonic;
    KeyStructureMode mode;
} Scenario;

static const Scenario SCENARIOS[] = {
    {"C major 4/4", 4, 4, 0, KEY_STRUCTURE_MODE_MAJOR},
    {"A minor 3/4", 3, 4, 9, KEY_STRUCTURE_MODE_MINOR},
    {"D major 6/8", 6, 8, 2, KEY_STRUCTURE_MODE_MAJOR},
};

#define SEEDS 20

static Rational time_of(long num, unsigned long den) {
    Rational time;
    assert(rational_make(num, den, &time) == RATIONAL_OK);
    return time;
}

/* A form of one bar of piece and one of material, with m0's melody open and asking to be invented. */
static PiecePlan *build_plan(const Scenario *scenario, LaneSpanId *open) {
    Rational bar = time_of(scenario->beats, (unsigned long)scenario->unit);
    Rational two;
    assert(rational_add(bar, bar, &two) == RATIONAL_OK);
    PlanSpan form_span = {time_of(0, 1), two};
    PlanSpan piece_span = {time_of(0, 1), bar};
    PlanSpan material_span = {bar, bar};
    PiecePlan *plan = plan_create();
    NodeId form;
    NodeId piece;
    NodeId section;
    NodeId phrase;
    NodeId idea;
    NodeId m0;
    assert(plan_add_node(plan, PLAN_NODE_NONE, PLAN_NODE_FORM, form_span, "form", &form) == PLAN_OK);
    assert(plan_add_node(plan, form, PLAN_NODE_SECTION, piece_span, "A", &piece) == PLAN_OK);
    assert(plan_add_node(plan, form, PLAN_NODE_SECTION, material_span, "material", &section) == PLAN_OK);
    assert(plan_add_node(plan, section, PLAN_NODE_PHRASE, material_span, "material", &phrase) == PLAN_OK);
    assert(plan_add_node(plan, phrase, PLAN_NODE_IDEA, material_span, "material", &idea) == PLAN_OK);
    assert(plan_add_node(plan, idea, PLAN_NODE_MOTIVE, material_span, "m0", &m0) == PLAN_OK);

    PlanLanePayload payload;
    LaneSpanId lane;
    memset(&payload, 0, sizeof(payload));
    payload.meter.beats_per_bar = scenario->beats;
    payload.meter.beat_unit = scenario->unit;
    assert(plan_add_lane_span(plan, form, PLAN_LANE_METER, 0, form_span, &payload, &lane) == PLAN_OK);
    assert(plan_lane_span_set_state(plan, lane, PLAN_STATE_GIVEN) == PLAN_OK);
    memset(&payload, 0, sizeof(payload));
    payload.key = plan_key_of_pitch_class(scenario->tonic, scenario->mode);
    assert(plan_add_lane_span(plan, form, PLAN_LANE_KEY, 0, form_span, &payload, &lane) == PLAN_OK);
    assert(plan_lane_span_set_state(plan, lane, PLAN_STATE_GIVEN) == PLAN_OK);

    PlanNote placeholder = {60, time_of(0, 1), bar};
    memset(&payload, 0, sizeof(payload));
    payload.notes.notes = &placeholder;
    payload.notes.note_count = 1;
    assert(plan_add_lane_span(plan, m0, PLAN_LANE_MELODY, 0, material_span, &payload, open) == PLAN_OK);
    PlanConstraint invent = {PLAN_CONSTRAINT_INVENT_MOTIVE, {0}};
    assert(plan_lane_span_add_constraint(plan, *open, &invent) == PLAN_OK);
    return plan;
}

/* Runs motive-invent alone on a fresh plan; the caller destroys the plan and the record. */
static PiecePlan *invent(const Scenario *scenario, unsigned long seed, LaneSpanId *open, FillRecord **record) {
    PiecePlan *plan = build_plan(scenario, open);
    FillOperatorRegistry *registry = fill_registry_create();
    assert(motive_invent_register(registry, NULL) == FILL_OK);
    FillConfig config;
    memset(&config, 0, sizeof(config));
    config.seed = seed;
    config.checker = motive_invent_checker;
    FillOutcome outcome;
    assert(fill_run(plan, registry, &config, &outcome, record) == FILL_OK);
    fill_outcome_destroy(&outcome);
    fill_registry_destroy(registry);
    return plan;
}

/* The scale degree (1 to 7) of a diatonic pitch, or 0 for one outside the scale. */
static int degree_of(const Scenario *scenario, int pitch) {
    static const int MAJOR[7] = {0, 2, 4, 5, 7, 9, 11};
    static const int MINOR[7] = {0, 2, 3, 5, 7, 8, 10};
    const int *scale = scenario->mode == KEY_STRUCTURE_MODE_MINOR ? MINOR : MAJOR;
    int offset = ((pitch - scenario->tonic) % 12 + 12) % 12;
    for (int degree = 0; degree < 7; degree++) {
        if (scale[degree] == offset) {
            return degree + 1;
        }
    }
    return 0;
}

static void check_motive(const Scenario *scenario, const PiecePlan *plan, LaneSpanId open, const FillRecord *record) {
    Rational bar = time_of(scenario->beats, (unsigned long)scenario->unit);
    assert(plan_lane_span_state(plan, open) == PLAN_STATE_REALIZED);
    assert(plan_node_state(plan, plan_lane_span_owner(plan, open)) == PLAN_STATE_REALIZED);
    const PlanNotes *notes = &plan_lane_span_payload(plan, open)->notes;
    assert(notes->note_count >= 3 && notes->note_count <= 6);

    /* The notes follow one another from the barline and end on the next one. */
    Rational at = time_of(0, 1);
    for (size_t i = 0; i < notes->note_count; i++) {
        const PlanNote *note = &notes->notes[i];
        assert(rational_cmp(note->offset, at) == 0);
        assert(note->duration.num > 0);
        assert(rational_add(at, note->duration, &at) == RATIONAL_OK);
        assert(note->midi_pitch >= 60 && note->midi_pitch <= 84);
        assert(degree_of(scenario, note->midi_pitch) != 0);
        if (i > 0) {
            int leap = abs(note->midi_pitch - notes->notes[i - 1].midi_pitch);
            /* At most a sixth; no tritone or seventh. */
            assert(leap <= 9 && leap != 6);
        }
    }
    assert(rational_cmp(at, bar) == 0);
    int first = degree_of(scenario, notes->notes[0].midi_pitch);
    assert(first == 1 || first == 3 || first == 5);

    /* The chosen candidate passed the checker: the melodic rules hold. */
    bool applied = false;
    for (size_t s = 0; s < record->step_count; s++) {
        const FillStep *step = &record->steps[s];
        if (step->outcome == FILL_STEP_APPLIED) {
            assert(step->chosen >= 0 && !step->considered[step->chosen].rejected);
            assert(strcmp(step->considered[step->chosen].operator_name, MOTIVE_INVENT_OPERATOR_NAME) == 0);
            applied = true;
        }
    }
    assert(applied);
}

static bool same_notes(const PiecePlan *a, const PiecePlan *b, LaneSpanId id) {
    const PlanNotes *x = &plan_lane_span_payload(a, id)->notes;
    const PlanNotes *y = &plan_lane_span_payload(b, id)->notes;
    if (x->note_count != y->note_count) {
        return false;
    }
    for (size_t i = 0; i < x->note_count; i++) {
        if (x->notes[i].midi_pitch != y->notes[i].midi_pitch ||
            rational_cmp(x->notes[i].offset, y->notes[i].offset) != 0 ||
            rational_cmp(x->notes[i].duration, y->notes[i].duration) != 0) {
            return false;
        }
    }
    return true;
}

static void test_scenario(const Scenario *scenario) {
    size_t distinct = 0;
    PiecePlan *previous = NULL;
    LaneSpanId previous_open = PLAN_LANE_SPAN_NONE;
    for (unsigned long seed = 1; seed <= SEEDS; seed++) {
        LaneSpanId open;
        LaneSpanId again_open;
        FillRecord *record = NULL;
        FillRecord *again_record = NULL;
        PiecePlan *plan = invent(scenario, seed, &open, &record);
        check_motive(scenario, plan, open, record);

        /* The same seed gives the same motive and the same record. */
        PiecePlan *again = invent(scenario, seed, &again_open, &again_record);
        assert(open == again_open && same_notes(plan, again, open));
        char *text = fill_record_to_json(record);
        char *again_text = fill_record_to_json(again_record);
        assert(text && again_text && strcmp(text, again_text) == 0);
        free(text);
        free(again_text);
        fill_record_destroy(again_record);
        plan_destroy(again);

        distinct += !previous || !same_notes(previous, plan, previous_open);
        plan_destroy(previous);
        previous = plan;
        previous_open = open;
        fill_record_destroy(record);
    }
    plan_destroy(previous);
    printf("%s: %lu seeds, %zu changes from one seed to the next\n", scenario->name, (unsigned long)SEEDS, distinct);
    assert(distinct > 1);
}

int main(void) {
    for (size_t i = 0; i < sizeof(SCENARIOS) / sizeof(SCENARIOS[0]); i++) {
        test_scenario(&SCENARIOS[i]);
    }
    return 0;
}
