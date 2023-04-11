#include <ursatz_composer/piece_plan.h>

#include <stdlib.h>
#include <string.h>

#include "plan/plan_internal.h"

static unsigned long long gcd_ull(unsigned long long a, unsigned long long b) {
    while (b != 0) {
        unsigned long long remainder = a % b;
        a = b;
        b = remainder;
    }
    return a;
}

PlanResult plan_time_make(long num, unsigned long den, PlanTime *out) {
    if (!out) {
        return PLAN_ERR_NULL;
    }
    if (den == 0) {
        return PLAN_ERR_BAD_TIME;
    }
    unsigned long long magnitude = num < 0 ? 0ULL - (unsigned long long)num : (unsigned long long)num;
    unsigned long long divisor = gcd_ull(magnitude, den);
    out->num = (long)((long long)num / (long long)divisor);
    out->den = (unsigned long)(den / divisor);
    return PLAN_OK;
}

PlanResult plan_time_add(PlanTime a, PlanTime b, PlanTime *out) {
    if (!out) {
        return PLAN_ERR_NULL;
    }
    long long num = (long long)a.num * (long long)b.den + (long long)b.num * (long long)a.den;
    unsigned long long den = (unsigned long long)a.den * (unsigned long long)b.den;
    return plan_time_make((long)num, (unsigned long)den, out);
}

int plan_time_compare(PlanTime a, PlanTime b) {
    long long left = (long long)a.num * (long long)b.den;
    long long right = (long long)b.num * (long long)a.den;
    return (left > right) - (left < right);
}

PlanResult plan_span_end(PlanSpan span, PlanTime *out) {
    return plan_time_add(span.start, span.length, out);
}

bool plan_span_contains(PlanSpan outer, PlanSpan inner) {
    PlanTime outer_end;
    PlanTime inner_end;
    if (plan_span_end(outer, &outer_end) != PLAN_OK || plan_span_end(inner, &inner_end) != PLAN_OK) {
        return false;
    }
    return plan_time_compare(inner.start, outer.start) >= 0 && plan_time_compare(inner_end, outer_end) <= 0;
}

static char *copy_string(const char *s) {
    size_t size = strlen(s) + 1;
    char *copy = malloc(size);
    if (copy) {
        memcpy(copy, s, size);
    }
    return copy;
}

PiecePlan *plan_create(void) {
    return calloc(1, sizeof(PiecePlan));
}

void plan_destroy(PiecePlan *plan) {
    if (!plan) {
        return;
    }
    for (size_t i = 0; i < plan->node_count; i++) {
        free(plan->nodes[i].name);
        free(plan->nodes[i].children);
    }
    plan_lanes_free(plan);
    free(plan->nodes);
    free(plan);
}

PlanNode *plan_node_at(const PiecePlan *plan, NodeId id) {
    if (!plan || id == PLAN_NODE_NONE || id > plan->node_count) {
        return NULL;
    }
    return &plan->nodes[id - 1];
}

static bool span_is_valid(PlanSpan span) {
    return span.start.den != 0 && span.length.den != 0 && span.length.num > 0;
}

static bool kind_fits_under(PlanNodeKind kind, PlanNodeKind parent_kind) {
    switch (kind) {
    case PLAN_NODE_SECTION:
        return parent_kind == PLAN_NODE_FORM;
    case PLAN_NODE_PHRASE:
        return parent_kind == PLAN_NODE_SECTION;
    case PLAN_NODE_SUBPHRASE:
        return parent_kind == PLAN_NODE_PHRASE;
    case PLAN_NODE_IDEA:
        return parent_kind == PLAN_NODE_PHRASE || parent_kind == PLAN_NODE_SUBPHRASE;
    case PLAN_NODE_MOTIVE:
        return parent_kind == PLAN_NODE_SUBPHRASE || parent_kind == PLAN_NODE_IDEA;
    case PLAN_NODE_FORM:
        return false;
    }
    return false;
}

static PlanResult append_child(PlanNode *parent, NodeId child) {
    if (parent->child_count == parent->child_capacity) {
        size_t capacity = parent->child_capacity ? parent->child_capacity * 2 : 4;
        NodeId *children = realloc(parent->children, capacity * sizeof(NodeId));
        if (!children) {
            return PLAN_ERR_ALLOC;
        }
        parent->children = children;
        parent->child_capacity = capacity;
    }
    parent->children[parent->child_count++] = child;
    return PLAN_OK;
}

PlanResult plan_add_node(PiecePlan *plan, NodeId parent, PlanNodeKind kind, PlanSpan span, const char *name,
                         NodeId *out_id) {
    if (!plan || !out_id) {
        return PLAN_ERR_NULL;
    }
    if (!span_is_valid(span)) {
        return PLAN_ERR_BAD_SPAN;
    }

    PlanNode *parent_node = NULL;
    if (parent == PLAN_NODE_NONE) {
        if (kind != PLAN_NODE_FORM) {
            return PLAN_ERR_BAD_KIND;
        }
        if (plan->node_count != 0) {
            return PLAN_ERR_ROOT_EXISTS;
        }
    } else {
        parent_node = plan_node_at(plan, parent);
        if (!parent_node) {
            return PLAN_ERR_NOT_FOUND;
        }
        if (!kind_fits_under(kind, parent_node->kind)) {
            return PLAN_ERR_BAD_KIND;
        }
        if (!plan_span_contains(parent_node->span, span)) {
            return PLAN_ERR_BAD_SPAN;
        }
    }

    char *name_copy = NULL;
    if (name) {
        name_copy = copy_string(name);
        if (!name_copy) {
            return PLAN_ERR_ALLOC;
        }
    }

    if (plan->node_count == plan->node_capacity) {
        size_t capacity = plan->node_capacity ? plan->node_capacity * 2 : 16;
        PlanNode *nodes = realloc(plan->nodes, capacity * sizeof(PlanNode));
        if (!nodes) {
            free(name_copy);
            return PLAN_ERR_ALLOC;
        }
        plan->nodes = nodes;
        plan->node_capacity = capacity;
        /* The array may have moved. */
        if (parent != PLAN_NODE_NONE) {
            parent_node = plan_node_at(plan, parent);
        }
    }

    NodeId id = plan->node_count + 1;
    if (parent_node) {
        PlanResult result = append_child(parent_node, id);
        if (result != PLAN_OK) {
            free(name_copy);
            return result;
        }
    }

    PlanNode *node = &plan->nodes[plan->node_count++];
    memset(node, 0, sizeof(*node));
    node->kind = kind;
    node->parent = parent;
    node->span = span;
    node->name = name_copy;
    *out_id = id;
    return PLAN_OK;
}

bool plan_has_node(const PiecePlan *plan, NodeId id) {
    return plan_node_at(plan, id) != NULL;
}

size_t plan_node_count(const PiecePlan *plan) {
    return plan ? plan->node_count : 0;
}

NodeId plan_root(const PiecePlan *plan) {
    return plan && plan->node_count > 0 ? 1 : PLAN_NODE_NONE;
}

PlanNodeKind plan_node_kind(const PiecePlan *plan, NodeId id) {
    const PlanNode *node = plan_node_at(plan, id);
    return node ? node->kind : PLAN_NODE_FORM;
}

NodeId plan_node_parent(const PiecePlan *plan, NodeId id) {
    const PlanNode *node = plan_node_at(plan, id);
    return node ? node->parent : PLAN_NODE_NONE;
}

PlanSpan plan_node_span(const PiecePlan *plan, NodeId id) {
    const PlanNode *node = plan_node_at(plan, id);
    PlanSpan empty = {{0, 1}, {0, 1}};
    return node ? node->span : empty;
}

const char *plan_node_name(const PiecePlan *plan, NodeId id) {
    const PlanNode *node = plan_node_at(plan, id);
    return node ? node->name : NULL;
}

size_t plan_node_child_count(const PiecePlan *plan, NodeId id) {
    const PlanNode *node = plan_node_at(plan, id);
    return node ? node->child_count : 0;
}

NodeId plan_node_child_at(const PiecePlan *plan, NodeId id, size_t index) {
    const PlanNode *node = plan_node_at(plan, id);
    if (!node || index >= node->child_count) {
        return PLAN_NODE_NONE;
    }
    return node->children[index];
}

/* Returns false once a visit asks to stop. */
static bool walk_node(const PiecePlan *plan, NodeId id, size_t depth, PlanVisitFn visit, void *context) {
    if (!visit(plan, id, depth, context)) {
        return false;
    }
    const PlanNode *node = plan_node_at(plan, id);
    for (size_t i = 0; i < node->child_count; i++) {
        if (!walk_node(plan, node->children[i], depth + 1, visit, context)) {
            return false;
        }
    }
    return true;
}

PlanResult plan_walk(const PiecePlan *plan, PlanVisitFn visit, void *context) {
    if (!plan || !visit) {
        return PLAN_ERR_NULL;
    }
    if (plan->node_count > 0) {
        walk_node(plan, plan_root(plan), 0, visit, context);
    }
    return PLAN_OK;
}
