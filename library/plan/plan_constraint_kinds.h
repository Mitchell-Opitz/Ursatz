#ifndef URSATZ_PLAN_CONSTRAINT_KINDS_H
#define URSATZ_PLAN_CONSTRAINT_KINDS_H

#include <stdbool.h>

#include <cJSON.h>
#include "plan/plan_constraint.h"
#include "plan/plan.h"
#include "plan/plan_lanes.h"

#include "plan/plan_json_internal.h"

/*
 * The constraint kinds a plan holds, one row each in PlanConstraintKind order (R-609). A row names the kind as the plan file
 * does and holds the three things that depend on it: whether a constraint of the kind is valid in a plan, how it is written as
 * the members of its JSON object (after "kind"), and how it is read from one (item is the object, path its path). Adding a kind
 * is a row in constraint_kinds.c and nothing else.
 */
typedef struct {
    PlanConstraintKind kind;
    const char *name;
    bool (*write)(cJSON *object, const Plan *plan, const PlanConstraint *constraint);
    bool (*read)(JsonReader *reader, const cJSON *item, const char *path, PlanConstraint *out);
    bool (*valid)(const Plan *plan, const PlanConstraint *constraint);
} ConstraintKindRow;

/* The row of a kind, or NULL for a value that is no kind. */
const ConstraintKindRow *constraint_kind_row(PlanConstraintKind kind);
/* The row of the kind a plan file names, or NULL. */
const ConstraintKindRow *constraint_kind_named(const char *name);

/*
 * Why a melody constraint (anchor, event, climax: R-746) may not go on a node (span_id PLAN_LANE_SPAN_NONE) or on the lane
 * span span_id, as text for an error message ("anchor: on a melody span only"), or NULL when it may, and for every other
 * kind. The checks that need the plan: a melody lane span, an offset below the span's length, one anchor and one event to an
 * offset on a span, one climax in the plan.
 */
const char *constraint_melody_refusal(const Plan *plan, LaneSpanId span_id, const PlanConstraint *constraint);
#endif
