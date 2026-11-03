#include <assert.h>
#include <string.h>

#include <ursatz_composer/constraint.h>
#include <ursatz_composer/lanes.h>
#include <ursatz_composer/piece_plan.h>
#include <ursatz_composer/plan_state.h>

/*
 * An 8-bar sentence in 4/4, one whole note per bar:
 *
 *   Form
 *    Section A
 *     Phrase "sentence"            bars 0-8
 *      Subphrase "presentation"    bars 0-4
 *       Idea "basic idea"          bars 0-2
 *        Motive "m1"               bar 0
 *       Idea "repetition"          bars 2-4
 *      Subphrase "continuation"    bars 4-8
 *       Motive "fragment"          bar 4
 *
 * The plan is built, walked and checked for span coverage, state rules and
 * refused writes to Given material.
 */

#define MAX_VISITED 16

typedef struct {
    const char *names[MAX_VISITED];
    size_t depths[MAX_VISITED];
    size_t count;
} Visited;

static PlanSpan bars(long start, long length) {
    PlanSpan span;
    assert(rational_make(start, 1, &span.start) == RATIONAL_OK);
    assert(rational_make(length, 1, &span.length) == RATIONAL_OK);
    return span;
}

static NodeId add(PiecePlan *plan, NodeId parent, PlanNodeKind kind, PlanSpan span, const char *name) {
    NodeId id = PLAN_NODE_NONE;
    assert(plan_add_node(plan, parent, kind, span, name, &id) == PLAN_OK);
    assert(id != PLAN_NODE_NONE);
    return id;
}

static bool record(const PiecePlan *plan, NodeId id, size_t depth, void *context) {
    Visited *visited = context;
    assert(visited->count < MAX_VISITED);
    visited->names[visited->count] = plan_node_name(plan, id);
    visited->depths[visited->count] = depth;
    visited->count++;
    return true;
}

static bool stop_after_three(const PiecePlan *plan, NodeId id, size_t depth, void *context) {
    size_t *seen = context;
    (void)plan;
    (void)id;
    (void)depth;
    return ++(*seen) < 3;
}

static PlanLanePayload meter_payload(void) {
    PlanLanePayload payload;
    memset(&payload, 0, sizeof(payload));
    payload.meter.beats_per_bar = 4;
    payload.meter.beat_unit = 4;
    payload.meter.bars_per_hyperbar = 4;
    return payload;
}

static PlanLanePayload harmony_payload(const char *roman, PlanHarmonicFunction function) {
    PlanLanePayload payload;
    memset(&payload, 0, sizeof(payload));
    strncpy(payload.harmony.roman_numeral, roman, PLAN_ROMAN_NUMERAL_MAX - 1);
    payload.harmony.function = function;
    return payload;
}

static LaneSpanId add_lane(PiecePlan *plan, NodeId owner, PlanLaneKind lane, int voice, PlanSpan span,
                           const PlanLanePayload *payload) {
    LaneSpanId id = PLAN_LANE_SPAN_NONE;
    assert(plan_add_lane_span(plan, owner, lane, voice, span, payload, &id) == PLAN_OK);
    return id;
}

int main(void) {
    PiecePlan *plan = plan_create();
    assert(plan != NULL);

    /* Tree */
    NodeId form = add(plan, PLAN_NODE_NONE, PLAN_NODE_FORM, bars(0, 8), "form");
    NodeId section = add(plan, form, PLAN_NODE_SECTION, bars(0, 8), "A");
    NodeId phrase = add(plan, section, PLAN_NODE_PHRASE, bars(0, 8), "sentence");
    NodeId presentation = add(plan, phrase, PLAN_NODE_SUBPHRASE, bars(0, 4), "presentation");
    NodeId basic_idea = add(plan, presentation, PLAN_NODE_IDEA, bars(0, 2), "basic idea");
    NodeId m1 = add(plan, basic_idea, PLAN_NODE_MOTIVE, bars(0, 1), "m1");
    NodeId repetition = add(plan, presentation, PLAN_NODE_IDEA, bars(2, 2), "repetition");
    NodeId continuation = add(plan, phrase, PLAN_NODE_SUBPHRASE, bars(4, 4), "continuation");
    NodeId fragment = add(plan, continuation, PLAN_NODE_MOTIVE, bars(4, 1), "fragment");

    assert(plan_node_count(plan) == 9);
    assert(plan_root(plan) == form);
    assert(plan_node_parent(plan, form) == PLAN_NODE_NONE);
    assert(plan_node_parent(plan, m1) == basic_idea);
    assert(plan_node_kind(plan, phrase) == PLAN_NODE_PHRASE);
    assert(strcmp(plan_node_name(plan, continuation), "continuation") == 0);

    /* Walk order is pre-order, children in insertion order. */
    Visited visited = {{NULL}, {0}, 0};
    assert(plan_walk(plan, record, &visited) == PLAN_OK);
    const char *expected_names[] = {"form",       "A",          "sentence",     "presentation", "basic idea",
                                    "m1",         "repetition", "continuation", "fragment"};
    const size_t expected_depths[] = {0, 1, 2, 3, 4, 5, 4, 3, 4};
    assert(visited.count == 9);
    for (size_t i = 0; i < visited.count; i++) {
        assert(strcmp(visited.names[i], expected_names[i]) == 0);
        assert(visited.depths[i] == expected_depths[i]);
    }
    size_t seen = 0;
    assert(plan_walk(plan, stop_after_three, &seen) == PLAN_OK);
    assert(seen == 3);

    /* Span coverage: the phrase's children tile it with no gap or overlap. */
    assert(plan_node_child_count(plan, phrase) == 2);
    PlanSpan first = plan_node_span(plan, plan_node_child_at(plan, phrase, 0));
    PlanSpan second = plan_node_span(plan, plan_node_child_at(plan, phrase, 1));
    Rational first_end;
    Rational second_end;
    Rational phrase_end;
    assert(plan_span_end(first, &first_end) == PLAN_OK);
    assert(plan_span_end(second, &second_end) == PLAN_OK);
    assert(plan_span_end(plan_node_span(plan, phrase), &phrase_end) == PLAN_OK);
    assert(rational_cmp(first.start, plan_node_span(plan, phrase).start) == 0);
    assert(rational_cmp(first_end, second.start) == 0);
    assert(rational_cmp(second_end, phrase_end) == 0);

    /* Structure rules refuse bad additions. */
    NodeId rejected = PLAN_NODE_NONE;
    assert(plan_add_node(plan, form, PLAN_NODE_FORM, bars(0, 8), NULL, &rejected) == PLAN_ERR_BAD_KIND);
    assert(plan_add_node(plan, PLAN_NODE_NONE, PLAN_NODE_FORM, bars(0, 8), NULL, &rejected) == PLAN_ERR_ROOT_EXISTS);
    assert(plan_add_node(plan, phrase, PLAN_NODE_SECTION, bars(0, 8), NULL, &rejected) == PLAN_ERR_BAD_KIND);
    assert(plan_add_node(plan, presentation, PLAN_NODE_IDEA, bars(3, 2), NULL, &rejected) == PLAN_ERR_BAD_SPAN);
    assert(plan_add_node(plan, presentation, PLAN_NODE_IDEA, bars(0, 0), NULL, &rejected) == PLAN_ERR_BAD_SPAN);
    assert(plan_add_node(plan, 99, PLAN_NODE_IDEA, bars(0, 1), NULL, &rejected) == PLAN_ERR_NOT_FOUND);
    assert(plan_node_count(plan) == 9);

    /* Lanes */
    PlanLanePayload meter = meter_payload();
    LaneSpanId meter_span = add_lane(plan, form, PLAN_LANE_METER, 0, bars(0, 8), &meter);

    PlanLanePayload key;
    memset(&key, 0, sizeof(key));
    key.key.tonic_pitch_class = 0;
    key.key.mode = PLAN_MODE_MAJOR;
    LaneSpanId key_span = add_lane(plan, section, PLAN_LANE_KEY, 0, bars(0, 8), &key);

    PlanLanePayload tonic = harmony_payload("I", PLAN_FUNCTION_TONIC);
    PlanLanePayload dominant = harmony_payload("V", PLAN_FUNCTION_DOMINANT);
    PlanLanePayload predominant = harmony_payload("ii6", PLAN_FUNCTION_PREDOMINANT);
    LaneSpanId harmony_one = add_lane(plan, basic_idea, PLAN_LANE_HARMONY, 0, bars(0, 2), &tonic);
    LaneSpanId harmony_two = add_lane(plan, repetition, PLAN_LANE_HARMONY, 0, bars(2, 2), &dominant);
    add_lane(plan, continuation, PLAN_LANE_HARMONY, 0, bars(4, 2), &predominant);
    add_lane(plan, continuation, PLAN_LANE_HARMONY, 0, bars(6, 1), &dominant);
    add_lane(plan, continuation, PLAN_LANE_HARMONY, 0, bars(7, 1), &tonic);
    assert(strcmp(plan_lane_span_payload(plan, harmony_two)->harmony.roman_numeral, "V") == 0);
    assert(plan_lane_span_payload(plan, harmony_two)->harmony.function == PLAN_FUNCTION_DOMINANT);

    PlanNote melody_notes[2];
    memset(melody_notes, 0, sizeof(melody_notes));
    melody_notes[0].midi_pitch = 64;
    assert(rational_make(0, 1, &melody_notes[0].offset) == RATIONAL_OK);
    assert(rational_make(1, 2, &melody_notes[0].duration) == RATIONAL_OK);
    melody_notes[1].midi_pitch = 67;
    assert(rational_make(1, 2, &melody_notes[1].offset) == RATIONAL_OK);
    assert(rational_make(1, 2, &melody_notes[1].duration) == RATIONAL_OK);
    PlanLanePayload melody;
    memset(&melody, 0, sizeof(melody));
    melody.notes.notes = melody_notes;
    melody.notes.note_count = 2;
    LaneSpanId melody_span = add_lane(plan, m1, PLAN_LANE_MELODY, 0, bars(0, 1), &melody);
    assert(plan_lane_span_payload(plan, melody_span)->notes.note_count == 2);
    assert(plan_lane_span_payload(plan, melody_span)->notes.notes != melody_notes);
    assert(plan_lane_span_payload(plan, melody_span)->notes.notes[1].midi_pitch == 67);

    LaneSpanId overlap = PLAN_LANE_SPAN_NONE;
    assert(plan_add_lane_span(plan, presentation, PLAN_LANE_HARMONY, 0, bars(1, 2), &tonic, &overlap) ==
           PLAN_ERR_LANE_OVERLAP);
    assert(plan_add_lane_span(plan, basic_idea, PLAN_LANE_HARMONY, 0, bars(2, 1), &tonic, &overlap) ==
           PLAN_ERR_BAD_SPAN);
    assert(plan_lane_span_count(plan) == 8);

    LaneSpanId found[8];
    assert(plan_lane_find(plan, PLAN_LANE_HARMONY, 0, bars(1, 4), found, 8) == 3);
    assert(found[0] == harmony_one);
    assert(found[1] == harmony_two);
    assert(plan_node_lane_spans(plan, continuation, found, 8) == 3);

    /* Constraints are data on nodes and spans. */
    PlanConstraint length = {PLAN_CONSTRAINT_LENGTH_BARS, {0}};
    length.params.bars = 8;
    PlanConstraint cadence = {PLAN_CONSTRAINT_CADENCE, {0}};
    cadence.params.cadence = CADENCE_TYPE_PERFECT_AUTHENTIC;
    PlanConstraint motive_ref = {PLAN_CONSTRAINT_MOTIVE_REF, {0}};
    motive_ref.params.motive.motive = m1;
    motive_ref.params.motive.transform = PLAN_TRANSFORM_INVERTED;
    assert(plan_node_add_constraint(plan, phrase, &length) == PLAN_OK);
    assert(plan_node_add_constraint(plan, phrase, &cadence) == PLAN_OK);
    assert(plan_node_add_constraint(plan, fragment, &motive_ref) == PLAN_OK);
    assert(plan_node_constraint_count(plan, phrase) == 2);
    assert(plan_node_constraint_at(plan, phrase, 1)->params.cadence == CADENCE_TYPE_PERFECT_AUTHENTIC);
    assert(plan_node_constraint_at(plan, fragment, 0)->params.motive.motive == m1);
    assert(plan_node_constraint_at(plan, phrase, 2) == NULL);
    assert(plan_lane_span_add_constraint(plan, harmony_two, &motive_ref) == PLAN_OK);
    assert(plan_lane_span_constraint_count(plan, harmony_two) == 1);

    PlanConstraint bad_ref = {PLAN_CONSTRAINT_MOTIVE_REF, {0}};
    bad_ref.params.motive.motive = phrase;
    assert(plan_node_add_constraint(plan, phrase, &bad_ref) == PLAN_ERR_BAD_PAYLOAD);
    assert(plan_node_remove_constraint(plan, phrase, 0) == PLAN_OK);
    assert(plan_node_constraint_count(plan, phrase) == 1);
    assert(plan_node_constraint_at(plan, phrase, 0)->kind == PLAN_CONSTRAINT_CADENCE);
    assert(plan_node_remove_constraint(plan, phrase, 5) == PLAN_ERR_BAD_INDEX);

    /* Everything starts Constrained and moves to Realized and back. */
    assert(plan_node_state(plan, phrase) == PLAN_STATE_CONSTRAINED);
    assert(plan_lane_span_state(plan, melody_span) == PLAN_STATE_CONSTRAINED);
    melody_notes[1].midi_pitch = 69;
    assert(plan_lane_span_write(plan, melody_span, &melody) == PLAN_OK);
    assert(plan_lane_span_state(plan, melody_span) == PLAN_STATE_REALIZED);
    assert(plan_lane_span_payload(plan, melody_span)->notes.notes[1].midi_pitch == 69);
    assert(plan_lane_span_set_state(plan, melody_span, PLAN_STATE_CONSTRAINED) == PLAN_OK);
    assert(plan_node_set_state(plan, phrase, PLAN_STATE_REALIZED) == PLAN_OK);
    assert(plan_node_set_state(plan, phrase, PLAN_STATE_CONSTRAINED) == PLAN_OK);

    /* Given material is never changed. */
    assert(plan_node_set_state(plan, m1, PLAN_STATE_GIVEN) == PLAN_OK);
    assert(plan_node_set_state(plan, m1, PLAN_STATE_GIVEN) == PLAN_OK);
    assert(plan_node_set_state(plan, m1, PLAN_STATE_REALIZED) == PLAN_ERR_GIVEN_LOCKED);
    assert(plan_node_set_state(plan, m1, PLAN_STATE_CONSTRAINED) == PLAN_ERR_GIVEN_LOCKED);
    assert(plan_node_state(plan, m1) == PLAN_STATE_GIVEN);
    assert(plan_node_add_constraint(plan, m1, &length) == PLAN_ERR_GIVEN_LOCKED);
    assert(plan_node_constraint_count(plan, m1) == 0);
    /* A Given parent does not lock its children. */
    assert(plan_node_set_state(plan, basic_idea, PLAN_STATE_GIVEN) == PLAN_OK);
    assert(plan_node_state(plan, m1) == PLAN_STATE_GIVEN);
    assert(plan_node_set_state(plan, repetition, PLAN_STATE_REALIZED) == PLAN_OK);

    assert(plan_lane_span_set_state(plan, meter_span, PLAN_STATE_GIVEN) == PLAN_OK);
    PlanLanePayload other_meter = meter_payload();
    other_meter.meter.beats_per_bar = 3;
    assert(plan_lane_span_write(plan, meter_span, &other_meter) == PLAN_ERR_GIVEN_LOCKED);
    assert(plan_lane_span_payload(plan, meter_span)->meter.beats_per_bar == 4);
    assert(plan_lane_span_state(plan, meter_span) == PLAN_STATE_GIVEN);
    assert(plan_lane_span_set_state(plan, meter_span, PLAN_STATE_REALIZED) == PLAN_ERR_GIVEN_LOCKED);
    assert(plan_lane_span_add_constraint(plan, meter_span, &length) == PLAN_ERR_GIVEN_LOCKED);

    assert(plan_lane_span_set_state(plan, key_span, PLAN_STATE_GIVEN) == PLAN_OK);
    PlanLanePayload bad_key;
    memset(&bad_key, 0, sizeof(bad_key));
    bad_key.key.tonic_pitch_class = 7;
    assert(plan_lane_span_write(plan, key_span, &bad_key) == PLAN_ERR_GIVEN_LOCKED);
    assert(plan_lane_span_payload(plan, key_span)->key.tonic_pitch_class == 0);

    plan_destroy(plan);
    return 0;
}
