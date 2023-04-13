#ifndef URSATZ_COMPOSER_PLAN_INTERNAL_H
#define URSATZ_COMPOSER_PLAN_INTERNAL_H

#include <ursatz_composer/constraint.h>
#include <ursatz_composer/lanes.h>
#include <ursatz_composer/piece_plan.h>
#include <ursatz_composer/plan_state.h>

typedef struct {
    PlanConstraint *items;
    size_t count;
    size_t capacity;
} ConstraintList;

typedef struct {
    PlanNodeKind kind;
    NodeId parent;
    PlanSpan span;
    char *name;
    NodeId *children;
    size_t child_count;
    size_t child_capacity;
    PlanState state;
    ConstraintList constraints;
} PlanNode;

typedef struct {
    PlanLaneKind lane;
    int voice;
    NodeId owner;
    PlanSpan span;
    PlanLanePayload payload;
    PlanState state;
    ConstraintList constraints;
} LaneSpan;

struct PiecePlan {
    PlanNode *nodes;
    size_t node_count;
    size_t node_capacity;
    LaneSpan *lane_spans;
    size_t lane_span_count;
    size_t lane_span_capacity;
};

void constraint_list_free(ConstraintList *list);

/* Frees every lane span and the note arrays they own. */
void plan_lanes_free(PiecePlan *plan);

/* True when payload is a legal payload for lane over span. */
bool lane_payload_is_valid(PlanLaneKind lane, PlanSpan span, const PlanLanePayload *payload);

/* NULL for LANE_SPAN_NONE or an unknown id. */
LaneSpan *plan_lane_span_at(const PiecePlan *plan, LaneSpanId id);

/* NULL for PLAN_NODE_NONE or an unknown id. */
PlanNode *plan_node_at(const PiecePlan *plan, NodeId id);

#endif
