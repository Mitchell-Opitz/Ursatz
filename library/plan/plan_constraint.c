#include "plan/plan_constraint.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>


#include "plan/plan_constraint_kinds.h"
#include "plan/plan_internal.h"
#include "plan/plan_common.h"

void constraint_list_free(ConstraintList *list) {
    free(list->items);
    memset(list, 0, sizeof(*list));
}

static bool constraint_is_valid(const Plan *plan, const PlanConstraint *constraint) {
    const ConstraintKindRow *row = constraint_kind_row(constraint->kind);
    return row && row->valid(plan, constraint);
}

static PlanResult list_add(ConstraintList *list, const PlanConstraint *constraint) {
    if (list->count == list->capacity) {
        size_t capacity = list->capacity ? list->capacity * 2 : 4;
        PlanConstraint *items = realloc(list->items, capacity * sizeof(PlanConstraint));
        if (!items) {
            return PLAN_ERR_ALLOC;
        }
        list->items = items;
        list->capacity = capacity;
    }
    list->items[list->count++] = *constraint;
    return PLAN_OK;
}

static PlanResult list_remove(ConstraintList *list, size_t index) {
    if (index >= list->count) {
        return PLAN_ERR_BAD_INDEX;
    }
    memmove(&list->items[index], &list->items[index + 1], (list->count - index - 1) * sizeof(PlanConstraint));
    list->count--;
    return PLAN_OK;
}

static const PlanConstraint *list_at(const ConstraintList *list, size_t index) {
    return index < list->count ? &list->items[index] : NULL;
}

/* ------------------------------------------------------------- the description of a section node */

const SectionDescription *plan_node_section(const Plan *plan, NodeId id) {
    const PlanNode *node = plan_node_at(plan, id);
    return node && node->kind == PLAN_NODE_SECTION ? &node->section : NULL;
}

PlanResult plan_node_set_section(Plan *plan, NodeId id, const SectionDescription *description) {
    if (!plan || !description) {
        return PLAN_ERR_NULL;
    }
    PlanNode *node = plan_node_at(plan, id);
    if (!node) {
        return PLAN_ERR_NOT_FOUND;
    }
    if (node->kind != PLAN_NODE_SECTION) {
        return PLAN_ERR_BAD_KIND;
    }
    if (node->state == PLAN_STATE_GIVEN) {
        return PLAN_ERR_GIVEN_LOCKED;
    }
    /* a return names a section node of the plan; the description's own rules are the library's */
    if (!section_description_check(description, SECTION_USE_PLAN, NULL) ||
        (description->returns[0] != '\0' && plan_section_named(plan, description->returns) == PLAN_NODE_NONE)) {
        return PLAN_ERR_BAD_PAYLOAD;
    }
    node->section = *description;
    return PLAN_OK;
}

const SectionDescription *plan_section_or_empty(const Plan *plan, NodeId node) {
    static const SectionDescription EMPTY;
    const SectionDescription *section = plan_node_section(plan, node);
    return section ? section : &EMPTY;
}

bool plan_section_has_role(const Plan *plan, NodeId node, SectionRole role) {
    const SectionDescription *section = plan_node_section(plan, node);
    return section && section->has_role && section->role == role;
}

bool plan_section_part_of(const Plan *plan, NodeId node, int *part) {
    const SectionDescription *section = plan_node_section(plan, node);
    if (!section || !section->has_role || section->role < SECTION_ROLE_INTRO || section->role > SECTION_ROLE_ENDING) {
        return false;
    }
    if (part) {
        *part = (int)(section->role - SECTION_ROLE_INTRO);
    }
    return true;
}

NodeId plan_section_named(const Plan *plan, const char *name) {
    for (NodeId id = 1; name && id <= plan_node_count(plan); id++) {
        const char *node_name = plan_has_node(plan, id) && plan_node_kind(plan, id) == PLAN_NODE_SECTION ? plan_node_name(plan, id) : NULL;
        if (node_name && strcmp(node_name, name) == 0) {
            return id;
        }
    }
    return PLAN_NODE_NONE;
}

bool plan_node_cadence(const Plan *plan, NodeId node, CadenceType *out) {
    const SectionDescription *section = plan_node_section(plan, node);
    if (section) {
        if (section->has_cadence) {
            *out = section->cadence;
        }
        return section->has_cadence;
    }
    for (size_t i = 0; i < plan_node_constraint_count(plan, node); i++) {
        const PlanConstraint *constraint = plan_node_constraint_at(plan, node, i);
        if (constraint->kind == PLAN_CONSTRAINT_CADENCE) {
            *out = constraint->params.cadence;
            return true;
        }
    }
    return false;
}

PlanResult plan_node_add_constraint(Plan *plan, NodeId id, const PlanConstraint *constraint) {
    if (!plan || !constraint) {
        return PLAN_ERR_NULL;
    }
    PlanNode *node = plan_node_at(plan, id);
    if (!node) {
        return PLAN_ERR_NOT_FOUND;
    }
    if (node->state == PLAN_STATE_GIVEN) {
        return PLAN_ERR_GIVEN_LOCKED;
    }
    /* a section's cadences are in its description */
    if (!constraint_is_valid(plan, constraint) || (constraint->kind == PLAN_CONSTRAINT_CADENCE && node->kind == PLAN_NODE_SECTION) ||
        constraint_melody_refusal(plan, PLAN_LANE_SPAN_NONE, constraint)) {
        return PLAN_ERR_BAD_PAYLOAD;
    }
    return list_add(&node->constraints, constraint);
}

size_t plan_node_constraint_count(const Plan *plan, NodeId id) {
    const PlanNode *node = plan_node_at(plan, id);
    return node ? node->constraints.count : 0;
}

const PlanConstraint *plan_node_constraint_at(const Plan *plan, NodeId id, size_t index) {
    const PlanNode *node = plan_node_at(plan, id);
    return node ? list_at(&node->constraints, index) : NULL;
}

PlanResult plan_node_remove_constraint(Plan *plan, NodeId id, size_t index) {
    PlanNode *node = plan_node_at(plan, id);
    if (!node) {
        return PLAN_ERR_NOT_FOUND;
    }
    if (node->state == PLAN_STATE_GIVEN) {
        return PLAN_ERR_GIVEN_LOCKED;
    }
    return list_remove(&node->constraints, index);
}

PlanResult plan_lane_span_add_constraint(Plan *plan, LaneSpanId id, const PlanConstraint *constraint) {
    if (!plan || !constraint) {
        return PLAN_ERR_NULL;
    }
    LaneSpan *entry = plan_lane_span_at(plan, id);
    if (!entry) {
        return PLAN_ERR_NOT_FOUND;
    }
    if (entry->state == PLAN_STATE_GIVEN) {
        return PLAN_ERR_GIVEN_LOCKED;
    }
    if (!constraint_is_valid(plan, constraint) || constraint_melody_refusal(plan, id, constraint)) {
        return PLAN_ERR_BAD_PAYLOAD;
    }
    return list_add(&entry->constraints, constraint);
}

size_t plan_lane_span_constraint_count(const Plan *plan, LaneSpanId id) {
    const LaneSpan *entry = plan_lane_span_at(plan, id);
    return entry ? entry->constraints.count : 0;
}

const PlanConstraint *plan_lane_span_constraint_at(const Plan *plan, LaneSpanId id, size_t index) {
    const LaneSpan *entry = plan_lane_span_at(plan, id);
    return entry ? list_at(&entry->constraints, index) : NULL;
}

PlanResult plan_lane_span_remove_constraint(Plan *plan, LaneSpanId id, size_t index) {
    LaneSpan *entry = plan_lane_span_at(plan, id);
    if (!entry) {
        return PLAN_ERR_NOT_FOUND;
    }
    if (entry->state == PLAN_STATE_GIVEN) {
        return PLAN_ERR_GIVEN_LOCKED;
    }
    return list_remove(&entry->constraints, index);
}

const PlanConstraint *plan_node_find_constraint(const Plan *plan, NodeId id, PlanConstraintKind kind) {
    for (size_t i = 0; i < plan_node_constraint_count(plan, id); i++) {
        const PlanConstraint *constraint = plan_node_constraint_at(plan, id, i);
        if (constraint->kind == kind) {
            return constraint;
        }
    }
    return NULL;
}

const PlanConstraint *plan_lane_span_find_constraint(const Plan *plan, LaneSpanId id, PlanConstraintKind kind) {
    for (size_t i = 0; i < plan_lane_span_constraint_count(plan, id); i++) {
        const PlanConstraint *constraint = plan_lane_span_constraint_at(plan, id, i);
        if (constraint->kind == kind) {
            return constraint;
        }
    }
    return NULL;
}

