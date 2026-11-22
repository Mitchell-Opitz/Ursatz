#include <assert.h>
#include <stdio.h>
#include <string.h>

#include <plan/plan_edit.h>
#include <plan/plan_lanes.h>
#include <plan/plan_state.h>

/*
 * The removals of plan_edit.h: one lane span, every lane span of an owner, and one leaf node with the lane spans it owns. A Given
 * span or node is never removed, and what is refused leaves the plan as it was.
 */

static Rational fraction(long num, unsigned long den) {
    Rational time;
    assert(rational_make(num, den, &time) == RATIONAL_OK);
    return time;
}

static PlanSpan bars(long start, long length) {
    PlanSpan span = {fraction(start, 1), fraction(length, 1)};
    return span;
}

static LaneSpanId add_harmony(Plan *plan, NodeId owner, PlanSpan span, const char *numeral) {
    PlanLanePayload payload;
    LaneSpanId id = PLAN_LANE_SPAN_NONE;
    memset(&payload, 0, sizeof(payload));
    strcpy(payload.harmony.roman_numeral, numeral);
    payload.harmony.function = HARMONIC_FUNCTION_TONIC;
    assert(plan_add_lane_span(plan, owner, PLAN_LANE_HARMONY, 0, span, &payload, &id) == PLAN_OK);
    return id;
}

static NodeId add_node(Plan *plan, NodeId parent, PlanNodeKind kind, PlanSpan span) {
    NodeId id = PLAN_NODE_NONE;
    assert(plan_add_node(plan, parent, kind, span, NULL, &id) == PLAN_OK);
    return id;
}

static void test_span_remove(void) {
    Plan *plan = plan_create();
    NodeId form = add_node(plan, PLAN_NODE_NONE, PLAN_NODE_FORM, bars(0, 4));
    LaneSpanId first = add_harmony(plan, form, bars(0, 2), "I");
    LaneSpanId second = add_harmony(plan, form, bars(2, 2), "V");
    assert(plan_has_lane_span(plan, first) && plan_has_lane_span(plan, second));

    assert(plan_lane_span_remove(plan, first) == PLAN_OK);
    assert(!plan_has_lane_span(plan, first) && plan_has_lane_span(plan, second));
    assert(plan_lane_span_remove(plan, first) == PLAN_ERR_NOT_FOUND);
    assert(plan_lane_span_remove(NULL, second) == PLAN_ERR_NULL);

    assert(plan_lane_span_set_state(plan, second, PLAN_STATE_GIVEN) == PLAN_OK);
    assert(plan_lane_span_remove(plan, second) == PLAN_ERR_GIVEN_LOCKED);
    assert(plan_has_lane_span(plan, second));
    plan_destroy(plan);
}

static void test_span_remove_owned(void) {
    Plan *plan = plan_create();
    NodeId form = add_node(plan, PLAN_NODE_NONE, PLAN_NODE_FORM, bars(0, 8));
    NodeId first = add_node(plan, form, PLAN_NODE_SECTION, bars(0, 4));
    NodeId second = add_node(plan, form, PLAN_NODE_SECTION, bars(4, 4));
    LaneSpanId plain = add_harmony(plan, first, bars(0, 2), "I");
    LaneSpanId given = add_harmony(plan, first, bars(2, 2), "V");
    LaneSpanId other = add_harmony(plan, second, bars(4, 4), "I");
    assert(plan_lane_span_set_state(plan, given, PLAN_STATE_GIVEN) == PLAN_OK);

    assert(plan_lane_span_remove_owned(plan, first) == PLAN_OK); /* the Given one goes too: the caller has checked */
    assert(!plan_has_lane_span(plan, plain) && !plan_has_lane_span(plan, given));
    assert(plan_has_lane_span(plan, other) && plan_lane_span_span(plan, other).length.num == 4);
    assert(plan_lane_span_remove_owned(NULL, first) == PLAN_OK);
    assert(plan_lane_span_remove_owned(plan, first) == PLAN_OK);
    assert(plan_has_lane_span(plan, other));
    plan_destroy(plan);
}

static void test_node_remove_leaf(void) {
    Plan *plan = plan_create();
    NodeId form = add_node(plan, PLAN_NODE_NONE, PLAN_NODE_FORM, bars(0, 8));
    NodeId section = add_node(plan, form, PLAN_NODE_SECTION, bars(0, 4));
    NodeId leaf = add_node(plan, section, PLAN_NODE_PHRASE, bars(0, 4));
    LaneSpanId leaf_span = add_harmony(plan, leaf, bars(0, 4), "I");

    assert(plan_node_remove_leaf(plan, form) == PLAN_ERR_BAD_KIND);    /* the root */
    assert(plan_node_remove_leaf(plan, section) == PLAN_ERR_BAD_KIND); /* a node with a child */
    assert(plan_has_node(plan, form) && plan_has_node(plan, section) && plan_has_lane_span(plan, leaf_span));

    assert(plan_node_remove_leaf(plan, leaf) == PLAN_OK);
    assert(!plan_has_node(plan, leaf) && plan_has_node(plan, section));
    assert(!plan_has_lane_span(plan, leaf_span)); /* its span went with it */
    assert(plan_node_remove_leaf(plan, leaf) == PLAN_ERR_NOT_FOUND);
    assert(plan_node_remove_leaf(NULL, leaf) == PLAN_ERR_NULL);

    NodeId given = add_node(plan, section, PLAN_NODE_PHRASE, bars(0, 4));
    assert(plan_node_set_state(plan, given, PLAN_STATE_GIVEN) == PLAN_OK);
    assert(plan_node_remove_leaf(plan, given) == PLAN_ERR_GIVEN_LOCKED);
    assert(plan_has_node(plan, given));
    plan_destroy(plan);
}

int main(void) {
    test_span_remove();
    test_span_remove_owned();
    test_node_remove_leaf();
    printf("test_plan_edit: ok\n");
    return 0;
}
