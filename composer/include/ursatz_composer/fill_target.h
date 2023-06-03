#ifndef URSATZ_COMPOSER_FILL_TARGET_H
#define URSATZ_COMPOSER_FILL_TARGET_H

#include <stdbool.h>
#include <stddef.h>

#include <ursatz_composer/constraint.h>
#include <ursatz_composer/lanes.h>
#include <ursatz_composer/piece_plan.h>
#include <ursatz_composer/plan_state.h>

/*
 * A fill target: one node or one lane span of a plan. The fill loop works on
 * one target at a time, and the open ones are the Constrained ones: Given
 * material is never changed, and Realized material is only regenerated on
 * request, not by the loop.
 */

typedef enum {
    FILL_TARGET_NODE,
    FILL_TARGET_LANE_SPAN
} FillTargetKind;

typedef struct {
    FillTargetKind kind;
    size_t id; /* a NodeId or a LaneSpanId, by kind */
} FillTarget;

bool fill_target_equals(FillTarget a, FillTarget b);

/* True when the target exists in the plan. */
bool fill_target_exists(const PiecePlan *plan, FillTarget target);
/* The accessors below return PLAN_STATE_CONSTRAINED, 0 or an empty span for a target that does not exist. */
PlanState fill_target_state(const PiecePlan *plan, FillTarget target);
size_t fill_target_constraint_count(const PiecePlan *plan, FillTarget target);
PlanSpan fill_target_span(const PiecePlan *plan, FillTarget target);

/*
 * Picks the next target to fill: the open (Constrained) node or lane span with
 * the most constraints on it. Ties go to, in order:
 *   1. the target that starts earlier on the timeline,
 *   2. a node over a lane span,
 *   3. the lower id.
 * The order is fixed, so the same plan always gives the same pick.
 *
 * Targets listed in excluded (the loop passes the ones it could not fill) are
 * not considered; excluded may be NULL when excluded_count is 0.
 *
 * Returns true and sets *out, or false when nothing is open: the plan is
 * finished.
 */
bool fill_pick_open_target(const PiecePlan *plan, const FillTarget *excluded, size_t excluded_count,
                           FillTarget *out);

#endif
