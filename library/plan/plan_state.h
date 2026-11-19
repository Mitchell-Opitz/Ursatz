#ifndef URSATZ_PLAN_STATE_H
#define URSATZ_PLAN_STATE_H

#include <stdbool.h>

#include "plan/plan_lanes.h"
#include "plan/plan.h"

/*
 * Every node and every lane span is in exactly one state. New ones start
 * Constrained.
 *   Given:       fixed by the user; never changed.
 *   Constrained: not written yet, has requirements.
 *   Realized:    written by the Composer; can be regenerated.
 * Constrained and Realized move freely between each other and into Given.
 * Nothing leaves Given: the call returns PLAN_ERR_GIVEN_LOCKED. Node and lane
 * span states are independent; Given on a parent does not lock its children.
 */
typedef enum {
    PLAN_STATE_CONSTRAINED,
    PLAN_STATE_GIVEN,
    PLAN_STATE_REALIZED
} PlanState;

bool plan_state_transition_allowed(PlanState from, PlanState to);

/* Returns PLAN_STATE_CONSTRAINED for an unknown id. */
PlanState plan_node_state(const Plan *plan, NodeId id);
PlanState plan_lane_span_state(const Plan *plan, LaneSpanId id);

/* Setting the state a node or span is already in succeeds without change, even for Given. */
PlanResult plan_node_set_state(Plan *plan, NodeId id, PlanState state);
PlanResult plan_lane_span_set_state(Plan *plan, LaneSpanId id, PlanState state);

/*
 * Replaces a lane span's payload and marks it Realized. Refused with
 * PLAN_ERR_GIVEN_LOCKED on a Given span, leaving it untouched. A payload the
 * lane cannot hold fails with PLAN_ERR_BAD_PAYLOAD and changes nothing.
 */
PlanResult plan_lane_span_write(Plan *plan, LaneSpanId id, const PlanLanePayload *payload);

#endif
