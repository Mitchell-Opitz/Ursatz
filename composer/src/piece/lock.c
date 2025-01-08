#include <ursatz_composer/regenerate.h>

#include <stdlib.h>

#include <ursatz_composer/lanes.h>
#include <ursatz_composer/plan_state.h>

/* The nodes of the subtree under id, id first, into out (room for every node of the plan). */
static size_t collect_subtree(const PiecePlan *plan, NodeId id, NodeId *out, size_t count) {
    out[count++] = id;
    for (size_t i = 0; i < plan_node_child_count(plan, id); i++) {
        count = collect_subtree(plan, plan_node_child_at(plan, id, i), out, count);
    }
    return count;
}

PlanResult plan_lock(PiecePlan *plan, FillTarget target) {
    if (!plan) {
        return PLAN_ERR_NULL;
    }
    if (!fill_target_exists(plan, target)) {
        return PLAN_ERR_NOT_FOUND;
    }
    if (target.kind == FILL_TARGET_LANE_SPAN) {
        LaneSpanId id = (LaneSpanId)target.id;
        return plan_lane_span_state(plan, id) == PLAN_STATE_CONSTRAINED ? PLAN_ERR_BAD_PAYLOAD
                                                                          : plan_lane_span_set_state(plan, id, PLAN_STATE_GIVEN);
    }

    NodeId *nodes = calloc(plan_node_count(plan) + 1, sizeof(NodeId));
    if (!nodes) {
        return PLAN_ERR_ALLOC;
    }
    size_t node_count = collect_subtree(plan, (NodeId)target.id, nodes, 0);

    /* Nothing is changed unless every node and span is written. */
    PlanResult result = PLAN_OK;
    for (size_t n = 0; result == PLAN_OK && n < node_count; n++) {
        if (plan_node_state(plan, nodes[n]) == PLAN_STATE_CONSTRAINED) {
            result = PLAN_ERR_BAD_PAYLOAD;
            break;
        }
        size_t spans = plan_node_lane_spans(plan, nodes[n], NULL, 0);
        LaneSpanId *ids = spans ? calloc(spans, sizeof(LaneSpanId)) : NULL;
        if (spans && !ids) {
            result = PLAN_ERR_ALLOC;
            break;
        }
        plan_node_lane_spans(plan, nodes[n], ids, spans);
        for (size_t s = 0; result == PLAN_OK && s < spans; s++) {
            if (plan_lane_span_state(plan, ids[s]) == PLAN_STATE_CONSTRAINED) {
                result = PLAN_ERR_BAD_PAYLOAD;
            }
        }
        free(ids);
    }
    for (size_t n = 0; result == PLAN_OK && n < node_count; n++) {
        size_t spans = plan_node_lane_spans(plan, nodes[n], NULL, 0);
        LaneSpanId *ids = spans ? calloc(spans, sizeof(LaneSpanId)) : NULL;
        if (spans && !ids) {
            result = PLAN_ERR_ALLOC;
            break;
        }
        plan_node_lane_spans(plan, nodes[n], ids, spans);
        for (size_t s = 0; s < spans; s++) {
            plan_lane_span_set_state(plan, ids[s], PLAN_STATE_GIVEN);
        }
        free(ids);
        plan_node_set_state(plan, nodes[n], PLAN_STATE_GIVEN);
    }
    free(nodes);
    return result;
}
