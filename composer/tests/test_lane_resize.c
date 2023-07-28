#include <assert.h>
#include <string.h>

#include <ursatz_composer/lanes.h>
#include <ursatz_composer/plan_state.h>

static PlanTime fraction(long num, unsigned long den) {
    PlanTime time;
    assert(plan_time_make(num, den, &time) == PLAN_OK);
    return time;
}

static PlanSpan bars(long start, long length) {
    PlanSpan span = {fraction(start, 1), fraction(length, 1)};
    return span;
}

static LaneSpanId add_harmony(PiecePlan *plan, NodeId owner, PlanSpan span, const char *numeral) {
    PlanLanePayload payload;
    LaneSpanId id = PLAN_LANE_SPAN_NONE;
    memset(&payload, 0, sizeof(payload));
    strcpy(payload.harmony.roman_numeral, numeral);
    payload.harmony.function = PLAN_FUNCTION_TONIC;
    assert(plan_add_lane_span(plan, owner, PLAN_LANE_HARMONY, 0, span, &payload, &id) == PLAN_OK);
    return id;
}

/* A resized span frees room for a neighbor, and the lane still refuses overlaps. */
static void test_resize_frees_room(void) {
    PiecePlan *plan = plan_create();
    NodeId form = PLAN_NODE_NONE;
    assert(plan_add_node(plan, PLAN_NODE_NONE, PLAN_NODE_FORM, bars(0, 4), NULL, &form) == PLAN_OK);
    LaneSpanId whole = add_harmony(plan, form, bars(0, 4), "I");

    PlanLanePayload payload;
    LaneSpanId tail = PLAN_LANE_SPAN_NONE;
    memset(&payload, 0, sizeof(payload));
    strcpy(payload.harmony.roman_numeral, "V");
    assert(plan_add_lane_span(plan, form, PLAN_LANE_HARMONY, 0, bars(2, 2), &payload, &tail) == PLAN_ERR_LANE_OVERLAP);

    assert(plan_lane_span_resize(plan, whole, fraction(2, 1)) == PLAN_OK);
    assert(plan_lane_span_span(plan, whole).length.num == 2);
    assert(plan_lane_span_state(plan, whole) == PLAN_STATE_CONSTRAINED);
    assert(plan_add_lane_span(plan, form, PLAN_LANE_HARMONY, 0, bars(2, 2), &payload, &tail) == PLAN_OK);
    plan_destroy(plan);
}

static void test_resize_refusals(void) {
    PiecePlan *plan = plan_create();
    NodeId form = PLAN_NODE_NONE;
    assert(plan_add_node(plan, PLAN_NODE_NONE, PLAN_NODE_FORM, bars(0, 4), NULL, &form) == PLAN_OK);
    LaneSpanId span = add_harmony(plan, form, bars(0, 4), "I");

    assert(plan_lane_span_resize(plan, span, fraction(5, 1)) == PLAN_ERR_BAD_SPAN);
    assert(plan_lane_span_resize(plan, span, fraction(0, 1)) == PLAN_ERR_BAD_SPAN);
    assert(plan_lane_span_resize(plan, span, fraction(-1, 1)) == PLAN_ERR_BAD_SPAN);
    assert(plan_lane_span_resize(plan, 99, fraction(1, 1)) == PLAN_ERR_NOT_FOUND);
    assert(plan_lane_span_resize(NULL, span, fraction(1, 1)) == PLAN_ERR_NULL);
    assert(plan_lane_span_span(plan, span).length.num == 4);

    assert(plan_lane_span_set_state(plan, span, PLAN_STATE_GIVEN) == PLAN_OK);
    assert(plan_lane_span_resize(plan, span, fraction(1, 1)) == PLAN_ERR_GIVEN_LOCKED);
    plan_destroy(plan);
}

static void test_resize_checks_notes(void) {
    PiecePlan *plan = plan_create();
    NodeId form = PLAN_NODE_NONE;
    assert(plan_add_node(plan, PLAN_NODE_NONE, PLAN_NODE_FORM, bars(0, 4), NULL, &form) == PLAN_OK);
    PlanNote note = {60, fraction(2, 1), fraction(1, 1)};
    PlanLanePayload payload;
    LaneSpanId melody = PLAN_LANE_SPAN_NONE;
    memset(&payload, 0, sizeof(payload));
    payload.notes.notes = &note;
    payload.notes.note_count = 1;
    assert(plan_add_lane_span(plan, form, PLAN_LANE_MELODY, 0, bars(0, 4), &payload, &melody) == PLAN_OK);
    assert(plan_lane_span_resize(plan, melody, fraction(2, 1)) == PLAN_ERR_BAD_PAYLOAD);
    assert(plan_lane_span_resize(plan, melody, fraction(3, 1)) == PLAN_OK);
    plan_destroy(plan);
}

int main(void) {
    test_resize_frees_room();
    test_resize_refusals();
    test_resize_checks_notes();
    return 0;
}
