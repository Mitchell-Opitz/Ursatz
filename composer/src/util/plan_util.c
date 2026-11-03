#include "util/plan_util.h"

bool plan_spans_overlap(PlanSpan a, PlanSpan b) {
    PlanTime a_end;
    PlanTime b_end;
    if (plan_span_end(a, &a_end) != PLAN_OK || plan_span_end(b, &b_end) != PLAN_OK) {
        return false;
    }
    return plan_time_compare(a.start, b_end) < 0 && plan_time_compare(b.start, a_end) < 0;
}

bool plan_is_note_lane(PlanLaneKind lane) {
    return lane == PLAN_LANE_BASS || lane == PLAN_LANE_MELODY || lane == PLAN_LANE_INNER_VOICE;
}

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

PieceResult piece_result_of_plan(PlanResult result) {
    return result == PLAN_OK ? PIECE_OK : (result == PLAN_ERR_ALLOC ? PIECE_ERR_ALLOC : PIECE_ERR_BAD_REQUEST);
}
