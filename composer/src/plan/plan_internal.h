#ifndef URSATZ_COMPOSER_PLAN_INTERNAL_H
#define URSATZ_COMPOSER_PLAN_INTERNAL_H

#include <ursatz_composer/piece_plan.h>

typedef struct {
    PlanNodeKind kind;
    NodeId parent;
    PlanSpan span;
    char *name;
    NodeId *children;
    size_t child_count;
    size_t child_capacity;
} PlanNode;

struct PiecePlan {
    PlanNode *nodes;
    size_t node_count;
    size_t node_capacity;
};

/* NULL for PLAN_NODE_NONE or an unknown id. */
PlanNode *plan_node_at(const PiecePlan *plan, NodeId id);

#endif
