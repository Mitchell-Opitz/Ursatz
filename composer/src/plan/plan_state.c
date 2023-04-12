#include <ursatz_composer/plan_state.h>

#include <stdlib.h>
#include <string.h>

#include "plan/plan_internal.h"

bool plan_state_transition_allowed(PlanState from, PlanState to) {
    return from == to || from != PLAN_STATE_GIVEN;
}

PlanState plan_node_state(const PiecePlan *plan, NodeId id) {
    const PlanNode *node = plan_node_at(plan, id);
    return node ? node->state : PLAN_STATE_CONSTRAINED;
}

PlanState plan_lane_span_state(const PiecePlan *plan, LaneSpanId id) {
    const LaneSpan *entry = plan_lane_span_at(plan, id);
    return entry ? entry->state : PLAN_STATE_CONSTRAINED;
}

PlanResult plan_node_set_state(PiecePlan *plan, NodeId id, PlanState state) {
    PlanNode *node = plan_node_at(plan, id);
    if (!node) {
        return PLAN_ERR_NOT_FOUND;
    }
    if (!plan_state_transition_allowed(node->state, state)) {
        return PLAN_ERR_GIVEN_LOCKED;
    }
    node->state = state;
    return PLAN_OK;
}

PlanResult plan_lane_span_set_state(PiecePlan *plan, LaneSpanId id, PlanState state) {
    LaneSpan *entry = plan_lane_span_at(plan, id);
    if (!entry) {
        return PLAN_ERR_NOT_FOUND;
    }
    if (!plan_state_transition_allowed(entry->state, state)) {
        return PLAN_ERR_GIVEN_LOCKED;
    }
    entry->state = state;
    return PLAN_OK;
}

PlanResult plan_lane_span_write(PiecePlan *plan, LaneSpanId id, const PlanLanePayload *payload) {
    LaneSpan *entry = plan_lane_span_at(plan, id);
    if (!entry || !payload) {
        return entry ? PLAN_ERR_NULL : PLAN_ERR_NOT_FOUND;
    }
    if (entry->state == PLAN_STATE_GIVEN) {
        return PLAN_ERR_GIVEN_LOCKED;
    }
    if (!lane_payload_is_valid(entry->lane, entry->span, payload)) {
        return PLAN_ERR_BAD_PAYLOAD;
    }

    PlanLanePayload stored = *payload;
    bool note_lane = entry->lane == PLAN_LANE_BASS || entry->lane == PLAN_LANE_MELODY ||
                     entry->lane == PLAN_LANE_INNER_VOICE;
    if (note_lane) {
        size_t bytes = payload->notes.note_count * sizeof(PlanNote);
        PlanNote *copy = malloc(bytes);
        if (!copy) {
            return PLAN_ERR_ALLOC;
        }
        memcpy(copy, payload->notes.notes, bytes);
        stored.notes.notes = copy;
        free((void *)entry->payload.notes.notes);
    }
    entry->payload = stored;
    entry->state = PLAN_STATE_REALIZED;
    return PLAN_OK;
}
