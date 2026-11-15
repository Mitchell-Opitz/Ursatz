#include <ursatz_composer/piece_plan.h>

#include <stdlib.h>
#include <string.h>

#include "plan/plan_internal.h"
#include "util/plan_util.h"
#include "util/util.h"

PlanResult plan_span_end(PlanSpan span, Rational *out) {
    return plan_result_of_rational(rational_add(span.start, span.length, out));
}

bool plan_span_contains(PlanSpan outer, PlanSpan inner) {
    Rational outer_end;
    Rational inner_end;
    if (plan_span_end(outer, &outer_end) != PLAN_OK || plan_span_end(inner, &inner_end) != PLAN_OK) {
        return false;
    }
    return rational_cmp(inner.start, outer.start) >= 0 && rational_cmp(inner_end, outer_end) <= 0;
}

PiecePlan *plan_create(void) {
    return calloc(1, sizeof(PiecePlan));
}

const char *plan_analyzer_version(const PiecePlan *plan) {
    return plan ? plan->analyzer_version : "";
}

PlanResult plan_set_analyzer_version(PiecePlan *plan, const char *version) {
    if (!plan || !version) {
        return PLAN_ERR_NULL;
    }
    if (strlen(version) >= PLAN_ANALYZER_VERSION_MAX) {
        return PLAN_ERR_BAD_PAYLOAD;
    }
    memcpy(plan->analyzer_version, version, strlen(version) + 1);
    return PLAN_OK;
}

void plan_destroy(PiecePlan *plan) {
    if (!plan) {
        return;
    }
    for (size_t i = 0; i < plan->node_count; i++) {
        free(plan->nodes[i].name);
        free(plan->nodes[i].children);
        constraint_list_free(&plan->nodes[i].constraints);
    }
    plan_lanes_free(plan);
    free(plan->nodes);
    free(plan);
}

PlanNode *plan_node_at(const PiecePlan *plan, NodeId id) {
    if (!plan || id == PLAN_NODE_NONE || id > plan->node_count || plan->nodes[id - 1].removed) {
        return NULL;
    }
    return &plan->nodes[id - 1];
}

NodeId plan_node_rank(const PiecePlan *plan, NodeId id) {
    if (!plan_node_at(plan, id) || plan->removed_node_count == 0) {
        return id;
    }
    NodeId rank = 0;
    for (NodeId other = 1; other <= id; other++) {
        rank += !plan->nodes[other - 1].removed;
    }
    return rank;
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
        name_copy = util_copy_string(name);
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

void plan_replace_contents(PiecePlan *dst, PiecePlan *src) {
    PiecePlan old = *dst;
    *dst = *src;
    *src = old;
    plan_destroy(src);
}

/* ------------------------------------------------------------- removal */

typedef struct {
    NodeId *ids;
    size_t count;
} NodeSet;

static bool set_has(const NodeSet *set, NodeId id) {
    for (size_t i = 0; i < set->count; i++) {
        if (set->ids[i] == id) {
            return true;
        }
    }
    return false;
}

/* Every descendant of id (not id), into set, which has room for all of them. */
static void collect_descendants(const PiecePlan *plan, NodeId id, NodeSet *set) {
    const PlanNode *node = plan_node_at(plan, id);
    for (size_t i = 0; node && i < node->child_count; i++) {
        set->ids[set->count++] = node->children[i];
        collect_descendants(plan, node->children[i], set);
    }
}

/* Whether the constraint names a node in set: a motive it is made from, or the section it returns. */
static bool names_removed(const PlanConstraint *constraint, const NodeSet *set) {
    if (constraint->kind == PLAN_CONSTRAINT_MOTIVE_REF) {
        return set_has(set, constraint->params.motive.motive);
    }
    return constraint->kind == PLAN_CONSTRAINT_RETURN && set_has(set, constraint->params.section_return.section);
}

/* The constraints of a list that name a node in set, counted; with remove, taken out of it. */
static size_t constraints_naming(ConstraintList *list, const NodeSet *set, bool remove) {
    size_t found = 0;
    for (size_t i = 0; i < list->count;) {
        if (!names_removed(&list->items[i], set)) {
            i++;
            continue;
        }
        found++;
        if (remove) {
            memmove(&list->items[i], &list->items[i + 1], (list->count - i - 1) * sizeof(PlanConstraint));
            list->count--;
        } else {
            i++;
        }
    }
    return found;
}

static void free_node(PlanNode *node) {
    free(node->name);
    free(node->children);
    constraint_list_free(&node->constraints);
    node->name = NULL;
    node->children = NULL;
    node->child_count = 0;
    node->child_capacity = 0;
}

/* Takes the node out of its parent's children. */
static void detach_child(PiecePlan *plan, NodeId id) {
    PlanNode *parent = plan_node_at(plan, plan->nodes[id - 1].parent);
    for (size_t i = 0; parent && i < parent->child_count; i++) {
        if (parent->children[i] == id) {
            memmove(&parent->children[i], &parent->children[i + 1], (parent->child_count - i - 1) * sizeof(NodeId));
            parent->child_count--;
            return;
        }
    }
}

/* Removes the nodes of set, which holds whole subtrees, and what goes with them; nothing is changed on a refusal. */
static PlanResult remove_nodes(PiecePlan *plan, const NodeSet *set) {
    /* Nothing is changed until every part of the removal is known to be allowed. */
    PlanResult result = PLAN_OK;
    for (size_t i = 0; result == PLAN_OK && i < set->count; i++) {
        if (plan->nodes[set->ids[i] - 1].state == PLAN_STATE_GIVEN) {
            result = PLAN_ERR_GIVEN_LOCKED;
        }
    }
    for (size_t i = 0; result == PLAN_OK && i < plan->lane_span_count; i++) {
        LaneSpan *entry = &plan->lane_spans[i];
        if (entry->removed || entry->state != PLAN_STATE_GIVEN) {
            continue;
        }
        if (set_has(set, entry->owner) || constraints_naming(&entry->constraints, set, false) > 0) {
            result = PLAN_ERR_GIVEN_LOCKED;
        }
    }
    for (size_t i = 0; result == PLAN_OK && i < plan->node_count; i++) {
        PlanNode *node = &plan->nodes[i];
        if (!node->removed && !set_has(set, (NodeId)(i + 1)) && node->state == PLAN_STATE_GIVEN &&
            constraints_naming(&node->constraints, set, false) > 0) {
            result = PLAN_ERR_GIVEN_LOCKED;
        }
    }
    if (result != PLAN_OK) {
        return result;
    }

    for (size_t i = 0; i < set->count; i++) {
        PlanNode *node = &plan->nodes[set->ids[i] - 1];
        if (!set_has(set, node->parent)) {
            detach_child(plan, set->ids[i]);
        }
    }
    for (size_t i = 0; i < set->count; i++) {
        plan_lane_span_remove_owned(plan, set->ids[i]);
        free_node(&plan->nodes[set->ids[i] - 1]);
        plan->nodes[set->ids[i] - 1].removed = true;
        plan->removed_node_count++;
    }
    for (size_t i = 0; i < plan->node_count; i++) {
        if (!plan->nodes[i].removed) {
            constraints_naming(&plan->nodes[i].constraints, set, true);
        }
    }
    for (size_t i = 0; i < plan->lane_span_count; i++) {
        if (!plan->lane_spans[i].removed) {
            constraints_naming(&plan->lane_spans[i].constraints, set, true);
        }
    }
    return PLAN_OK;
}

PlanResult plan_remove_subtree(PiecePlan *plan, NodeId id) {
    if (!plan) {
        return PLAN_ERR_NULL;
    }
    if (!plan_node_at(plan, id)) {
        return PLAN_ERR_NOT_FOUND;
    }
    NodeSet set = {NULL, 0};
    set.ids = plan->node_count ? calloc(plan->node_count, sizeof(NodeId)) : NULL;
    if (plan->node_count && !set.ids) {
        return PLAN_ERR_ALLOC;
    }
    collect_descendants(plan, id, &set);
    PlanResult result = remove_nodes(plan, &set);
    free(set.ids);
    return result;
}

PlanResult plan_node_remove_leaf(PiecePlan *plan, NodeId id) {
    if (!plan) {
        return PLAN_ERR_NULL;
    }
    const PlanNode *node = plan_node_at(plan, id);
    if (!node) {
        return PLAN_ERR_NOT_FOUND;
    }
    if (node->parent == PLAN_NODE_NONE || node->child_count > 0) {
        return PLAN_ERR_BAD_KIND;
    }
    NodeId only = id;
    NodeSet set = {&only, 1};
    return remove_nodes(plan, &set);
}
