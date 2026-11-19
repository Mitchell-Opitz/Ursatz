#include "plan/plan_edit.h"

#include <stdlib.h>
#include <string.h>

#include "plan/plan_internal.h"
#include "plan/plan_common.h"

/* The constraints of a list, copied; a motive a constraint names is the node the map gives. */
static bool constraints_clone(const ConstraintList *from, ConstraintList *to, const NodeId *node_map) {
    memset(to, 0, sizeof(*to));
    if (from->count == 0) {
        return true;
    }
    to->items = malloc(from->count * sizeof(PlanConstraint));
    if (!to->items) {
        return false;
    }
    memcpy(to->items, from->items, from->count * sizeof(PlanConstraint));
    to->count = from->count;
    to->capacity = from->count;
    for (size_t i = 0; i < to->count; i++) {
        if (to->items[i].kind == PLAN_CONSTRAINT_MOTIVE_REF) {
            to->items[i].params.motive.motive = node_map[to->items[i].params.motive.motive];
        }
    }
    return true;
}

/* The ids of the live nodes, 1 to n in id order; 0 for a removed node. */
static NodeId *node_map_make(const Plan *plan, size_t *live) {
    NodeId *map = calloc(plan->node_count + 1, sizeof(NodeId));
    *live = 0;
    for (size_t i = 0; map && i < plan->node_count; i++) {
        if (!plan->nodes[i].removed) {
            map[i + 1] = ++*live;
        }
    }
    return map;
}

static PlanResult nodes_clone(const Plan *plan, Plan *copy, const NodeId *node_map, size_t live) {
    if (live == 0) {
        return PLAN_OK;
    }
    copy->nodes = calloc(live, sizeof(PlanNode));
    if (!copy->nodes) {
        return PLAN_ERR_ALLOC;
    }
    copy->node_capacity = live;
    for (size_t i = 0; i < plan->node_count; i++) {
        const PlanNode *from = &plan->nodes[i];
        if (from->removed) {
            continue;
        }
        PlanNode *to = &copy->nodes[node_map[i + 1] - 1];
        copy->node_count++;
        to->kind = from->kind;
        to->parent = node_map[from->parent];
        to->span = from->span;
        to->state = from->state;
        to->section = from->section;
        if (from->name) {
            to->name = malloc(strlen(from->name) + 1);
            if (!to->name) {
                return PLAN_ERR_ALLOC;
            }
            memcpy(to->name, from->name, strlen(from->name) + 1);
        }
        if (from->child_count > 0) {
            to->children = malloc(from->child_count * sizeof(NodeId));
            if (!to->children) {
                return PLAN_ERR_ALLOC;
            }
            to->child_capacity = from->child_count;
            for (size_t c = 0; c < from->child_count; c++) {
                to->children[c] = node_map[from->children[c]];
            }
            to->child_count = from->child_count;
        }
        if (!constraints_clone(&from->constraints, &to->constraints, node_map)) {
            return PLAN_ERR_ALLOC;
        }
    }
    return PLAN_OK;
}

static PlanResult lane_spans_clone(const Plan *plan, Plan *copy, const NodeId *node_map) {
    size_t live = plan->lane_span_count - plan->removed_lane_span_count;
    if (live == 0) {
        return PLAN_OK;
    }
    copy->lane_spans = calloc(live, sizeof(LaneSpan));
    if (!copy->lane_spans) {
        return PLAN_ERR_ALLOC;
    }
    copy->lane_span_capacity = live;
    for (size_t i = 0; i < plan->lane_span_count; i++) {
        const LaneSpan *from = &plan->lane_spans[i];
        if (from->removed) {
            continue;
        }
        LaneSpan *to = &copy->lane_spans[copy->lane_span_count++];
        *to = *from;
        to->owner = node_map[from->owner];
        memset(&to->constraints, 0, sizeof(to->constraints));
        if (plan_is_note_lane(from->lane)) {
            to->payload.notes.notes = NULL;
            if (from->payload.notes.note_count > 0) {
                PlanNote *notes = malloc(from->payload.notes.note_count * sizeof(PlanNote));
                if (!notes) {
                    to->payload.notes.note_count = 0;
                    return PLAN_ERR_ALLOC;
                }
                memcpy(notes, from->payload.notes.notes, from->payload.notes.note_count * sizeof(PlanNote));
                to->payload.notes.notes = notes;
            }
        }
        if (!constraints_clone(&from->constraints, &to->constraints, node_map)) {
            return PLAN_ERR_ALLOC;
        }
    }
    return PLAN_OK;
}

PlanResult plan_clone(const Plan *plan, Plan **out) {
    if (!out) {
        return PLAN_ERR_NULL;
    }
    *out = NULL;
    if (!plan) {
        return PLAN_ERR_NULL;
    }
    Plan *copy = plan_create();
    size_t live = 0;
    NodeId *node_map = copy ? node_map_make(plan, &live) : NULL;
    if (!copy || !node_map) {
        plan_destroy(copy);
        free(node_map);
        return PLAN_ERR_ALLOC;
    }
    memcpy(copy->analyzer_version, plan->analyzer_version, sizeof(copy->analyzer_version));
    copy->next_note_id = plan->next_note_id;
    copy->has_form_record = plan->has_form_record;
    copy->form_record = plan->form_record;
    copy->has_derivation = plan->has_derivation;
    copy->derivation = plan->derivation;
    PlanResult result = nodes_clone(plan, copy, node_map, live);
    if (result == PLAN_OK) {
        result = lane_spans_clone(plan, copy, node_map);
    }
    free(node_map);
    if (result != PLAN_OK) {
        plan_destroy(copy);
        return result;
    }
    *out = copy;
    return PLAN_OK;
}

PlanResult plan_copy_into(Plan *plan, const Plan *source) {
    if (!plan || !source) {
        return PLAN_ERR_NULL;
    }
    Plan *copy = NULL;
    PlanResult result = plan_clone(source, &copy);
    if (result != PLAN_OK) {
        return result;
    }
    Plan old = *plan;
    *plan = *copy;
    *copy = old;
    plan_destroy(copy);
    return PLAN_OK;
}

PlanResult plan_lane_span_reopen(Plan *plan, LaneSpanId span, NodeId owner) {
    if (!plan) {
        return PLAN_ERR_NULL;
    }
    LaneSpan *entry = plan_lane_span_at(plan, span);
    const PlanNode *node = plan_node_at(plan, owner);
    if (!entry || !node) {
        return PLAN_ERR_NOT_FOUND;
    }
    if (entry->state != PLAN_STATE_GIVEN) {
        return PLAN_ERR_BAD_KIND;
    }
    if (!plan_span_contains(node->span, entry->span)) {
        return PLAN_ERR_BAD_SPAN;
    }
    entry->state = PLAN_STATE_REALIZED;
    entry->owner = owner;
    return PLAN_OK;
}
