#include "util/plan_util.h"

#include <string.h>

bool plan_in_subtree(const PiecePlan *plan, NodeId node, NodeId root) {
    for (; node != PLAN_NODE_NONE; node = plan_node_parent(plan, node)) {
        if (node == root) {
            return true;
        }
    }
    return false;
}

bool plan_melody_span_of(const PiecePlan *plan, NodeId node, LaneSpanId *out) {
    LaneSpanId ids[16];
    size_t found = plan_node_lane_spans(plan, node, ids, 16);
    for (size_t i = 0; i < found && i < 16; i++) {
        if (plan_lane_span_lane(plan, ids[i]) == PLAN_LANE_MELODY && plan_lane_span_voice(plan, ids[i]) == 0 &&
            plan_lane_span_state(plan, ids[i]) != PLAN_STATE_CONSTRAINED) {
            *out = ids[i];
            return true;
        }
    }
    return false;
}

bool plan_section_function_of(const PiecePlan *plan, NodeId node, SectionFunction *out) {
    const SectionDescription *section = plan_node_section(plan, node);
    if (!section || !section->has_function) {
        return false;
    }
    *out = section->function;
    return true;
}

PieceResult piece_result_of_plan(PlanResult result) {
    return result == PLAN_OK ? PIECE_OK : (result == PLAN_ERR_ALLOC ? PIECE_ERR_ALLOC : PIECE_ERR_BAD_REQUEST);
}

bool plan_add_given_node(PiecePlan *plan, NodeId parent, PlanNodeKind kind, PlanSpan span, NodeId *out) {
    return plan_add_node(plan, parent, kind, span, NULL, out) == PLAN_OK &&
           plan_node_set_state(plan, *out, PLAN_STATE_GIVEN) == PLAN_OK;
}

const char *const plan_fragment_position_names[PLAN_FRAGMENT_POSITION_NAME_COUNT] = {"any", "leading", "trailing", "middle"};

const char *plan_fragment_position_name(long position) {
    return position >= 0 && position < PLAN_FRAGMENT_POSITION_NAME_COUNT ? plan_fragment_position_names[position] : "any";
}
