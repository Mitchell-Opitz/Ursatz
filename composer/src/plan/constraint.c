#include <ursatz_composer/constraint.h>

#include <stdlib.h>
#include <string.h>

#include <galant_prototypes.h>
#include <texture_rules.h>

#include "plan/plan_internal.h"

void constraint_list_free(ConstraintList *list) {
    free(list->items);
    memset(list, 0, sizeof(*list));
}

static bool constraint_is_valid(const PiecePlan *plan, const PlanConstraint *constraint) {
    switch (constraint->kind) {
    case PLAN_CONSTRAINT_LENGTH_BARS:
        return constraint->params.bars > 0;
    case PLAN_CONSTRAINT_CADENCE:
        return constraint->params.cadence >= PLAN_CADENCE_PAC && constraint->params.cadence <= PLAN_CADENCE_PC;
    case PLAN_CONSTRAINT_MOTIVE_REF: {
        const PlanMotiveRef *ref = &constraint->params.motive;
        return plan_has_node(plan, ref->motive) && plan_node_kind(plan, ref->motive) == PLAN_NODE_MOTIVE &&
               ref->transform >= PLAN_TRANSFORM_NONE && ref->transform <= PLAN_TRANSFORM_LIQUIDATED &&
               ref->position >= PLAN_FRAGMENT_ANY && ref->position <= PLAN_FRAGMENT_MIDDLE &&
               ref->axis >= 0 && ref->axis <= 127 && ref->length >= 0 && ref->drop >= 0 && ref->interval >= 0 &&
               ref->ratio_num >= 0 && ref->ratio_den >= 0 && (ref->ratio_num == 0) == (ref->ratio_den == 0);
    }
    case PLAN_CONSTRAINT_KEY:
        return constraint->params.key.tonic_pitch_class >= 0 && constraint->params.key.tonic_pitch_class <= 11 &&
               (constraint->params.key.mode == PLAN_MODE_MAJOR || constraint->params.key.mode == PLAN_MODE_MINOR);
    case PLAN_CONSTRAINT_ARCHETYPE:
        return constraint->params.archetype >= PLAN_ARCHETYPE_SENTENCE &&
               constraint->params.archetype <= PLAN_ARCHETYPE_HYBRID;
    case PLAN_CONSTRAINT_EMBELLISH:
        return true;
    case PLAN_CONSTRAINT_SCHEMA: {
        const GalantPrototype *prototype = galant_prototype((GalantSchemaId)constraint->params.schema.schema);
        return prototype && constraint->params.schema.stage >= 0 &&
               (size_t)constraint->params.schema.stage < prototype->stage_count;
    }
    case PLAN_CONSTRAINT_MATERIAL:
        return true;
    case PLAN_CONSTRAINT_HARMONY_SHAPE: {
        const PlanHarmonyShape *shape = &constraint->params.harmony_shape;
        return shape->kind >= PLAN_HARMONY_SHAPE_PROLONGATION && shape->kind <= PLAN_HARMONY_SHAPE_CADENTIAL &&
               shape->starts_on >= PLAN_HARMONY_ANCHOR_ANY && shape->starts_on <= PLAN_HARMONY_ANCHOR_DOMINANT &&
               shape->ends_on >= PLAN_HARMONY_ANCHOR_ANY && shape->ends_on <= PLAN_HARMONY_ANCHOR_DOMINANT &&
               shape->max_cells_per_chord >= 0;
    }
    case PLAN_CONSTRAINT_THEME_PAIRING: {
        const PlanThemePairing *pairing = &constraint->params.theme_pairing;
        return pairing->beginning >= PLAN_THEME_BEGINNING_PRESENTATION &&
               pairing->beginning <= PLAN_THEME_BEGINNING_COMPOUND_BASIC_IDEA &&
               pairing->ending >= PLAN_THEME_ENDING_CONTINUATION && pairing->ending <= PLAN_THEME_ENDING_CADENTIAL;
    }
    case PLAN_CONSTRAINT_ACCOMPANIMENT: {
        const PlanAccompaniment *accompaniment = &constraint->params.accompaniment;
        int low = accompaniment->low ? accompaniment->low : PLAN_ACCOMPANIMENT_DEFAULT_LOW;
        int high = accompaniment->high ? accompaniment->high : PLAN_ACCOMPANIMENT_DEFAULT_HIGH;
        return accompaniment->pattern >= PLAN_ACCOMPANIMENT_ANY && accompaniment->pattern <= PLAN_ACCOMPANIMENT_OOM_PAH &&
               accompaniment->texture >= -1 && accompaniment->texture < (int)STANDARD_TEXTURE_COUNT &&
               accompaniment->low >= 0 && accompaniment->low <= 127 && accompaniment->high >= 0 &&
               accompaniment->high <= 127 && low < high;
    }
    case PLAN_CONSTRAINT_VOICING:
        return true;
    case PLAN_CONSTRAINT_COUNTERPOINT:
        return constraint->params.counterpoint.species >= 1 && constraint->params.counterpoint.species <= 4;
    }
    return false;
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

PlanResult plan_node_add_constraint(PiecePlan *plan, NodeId id, const PlanConstraint *constraint) {
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
    if (!constraint_is_valid(plan, constraint)) {
        return PLAN_ERR_BAD_PAYLOAD;
    }
    return list_add(&node->constraints, constraint);
}

size_t plan_node_constraint_count(const PiecePlan *plan, NodeId id) {
    const PlanNode *node = plan_node_at(plan, id);
    return node ? node->constraints.count : 0;
}

const PlanConstraint *plan_node_constraint_at(const PiecePlan *plan, NodeId id, size_t index) {
    const PlanNode *node = plan_node_at(plan, id);
    return node ? list_at(&node->constraints, index) : NULL;
}

PlanResult plan_node_remove_constraint(PiecePlan *plan, NodeId id, size_t index) {
    PlanNode *node = plan_node_at(plan, id);
    if (!node) {
        return PLAN_ERR_NOT_FOUND;
    }
    if (node->state == PLAN_STATE_GIVEN) {
        return PLAN_ERR_GIVEN_LOCKED;
    }
    return list_remove(&node->constraints, index);
}

PlanResult plan_lane_span_add_constraint(PiecePlan *plan, LaneSpanId id, const PlanConstraint *constraint) {
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
    if (!constraint_is_valid(plan, constraint)) {
        return PLAN_ERR_BAD_PAYLOAD;
    }
    return list_add(&entry->constraints, constraint);
}

size_t plan_lane_span_constraint_count(const PiecePlan *plan, LaneSpanId id) {
    const LaneSpan *entry = plan_lane_span_at(plan, id);
    return entry ? entry->constraints.count : 0;
}

const PlanConstraint *plan_lane_span_constraint_at(const PiecePlan *plan, LaneSpanId id, size_t index) {
    const LaneSpan *entry = plan_lane_span_at(plan, id);
    return entry ? list_at(&entry->constraints, index) : NULL;
}

PlanResult plan_lane_span_remove_constraint(PiecePlan *plan, LaneSpanId id, size_t index) {
    LaneSpan *entry = plan_lane_span_at(plan, id);
    if (!entry) {
        return PLAN_ERR_NOT_FOUND;
    }
    if (entry->state == PLAN_STATE_GIVEN) {
        return PLAN_ERR_GIVEN_LOCKED;
    }
    return list_remove(&entry->constraints, index);
}
