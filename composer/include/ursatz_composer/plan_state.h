#ifndef URSATZ_COMPOSER_PLAN_STATE_H
#define URSATZ_COMPOSER_PLAN_STATE_H

#include <stdbool.h>

#include <ursatz_composer/lanes.h>
#include <ursatz_composer/piece_plan.h>

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
PlanState plan_node_state(const PiecePlan *plan, NodeId id);
PlanState plan_lane_span_state(const PiecePlan *plan, LaneSpanId id);

/* Setting the state a node or span is already in succeeds without change, even for Given. */
PlanResult plan_node_set_state(PiecePlan *plan, NodeId id, PlanState state);
PlanResult plan_lane_span_set_state(PiecePlan *plan, LaneSpanId id, PlanState state);

/*
 * Replaces a lane span's payload and marks it Realized. Refused with
 * PLAN_ERR_GIVEN_LOCKED on a Given span, leaving it untouched. A payload the
 * lane cannot hold fails with PLAN_ERR_BAD_PAYLOAD and changes nothing.
 */
PlanResult plan_lane_span_write(PiecePlan *plan, LaneSpanId id, const PlanLanePayload *payload);

#endif
