#include <ursatz_composer/constraint.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <theory/galant/galant_prototypes.h>
#include <analysis/texture/texture_rules.h>

#include "plan/plan_internal.h"
#include "util/plan_util.h"
#include "util/util.h"

void constraint_list_free(ConstraintList *list) {
    free(list->items);
    memset(list, 0, sizeof(*list));
}

/* The meter the plan starts in, from its first meter lane span; 4/4 while it has none (a pickup on 1/32 is on every
   meter's grid, so only a triplet pickup depends on which meter is chosen). */
static PlanMeter first_meter(const PiecePlan *plan) {
    LaneSpanId id;
    PlanSpan whole = plan_node_span(plan, plan_root(plan));
    if (plan_lane_find(plan, PLAN_LANE_METER, 0, whole, &id, 1) == 0) {
        return (PlanMeter){4, 4, 0};
    }
    return plan_lane_span_payload(plan, id)->meter;
}

static bool constraint_is_valid(const PiecePlan *plan, const PlanConstraint *constraint) {
    switch (constraint->kind) {
    case PLAN_CONSTRAINT_LENGTH_BARS:
        return constraint->params.bars > 0;
    case PLAN_CONSTRAINT_CADENCE:
        return plan_cadence_index(constraint->params.cadence, NULL);
    case PLAN_CONSTRAINT_MOTIVE_REF: {
        const PlanMotiveRef *ref = &constraint->params.motive;
        return plan_has_node(plan, ref->motive) && plan_node_kind(plan, ref->motive) == PLAN_NODE_MOTIVE &&
               (!ref->has_step || (ref->step.kind >= 0 && ref->step.kind < MOTIVE_RELATION_KIND_COUNT));
    }
    case PLAN_CONSTRAINT_KEY:
        return analysis_key_tonic_pitch_class(&constraint->params.key) >= 0 && analysis_key_tonic_pitch_class(&constraint->params.key) <= 11 &&
               composer_key_valid(&constraint->params.key);
    case PLAN_CONSTRAINT_EMBELLISH:
        return true;
    case PLAN_CONSTRAINT_SCHEMA: {
        const GalantPrototype *prototype = galant_prototype((GalantSchemaId)constraint->params.schema.schema);
        return prototype && constraint->params.schema.stage >= 0 &&
               (size_t)constraint->params.schema.stage < prototype->stage_count;
    }
    case PLAN_CONSTRAINT_HARMONY_SHAPE: {
        const PlanHarmonyShape *shape = &constraint->params.harmony_shape;
        return shape->kind >= PLAN_HARMONY_SHAPE_PROLONGATION && shape->kind <= PLAN_HARMONY_SHAPE_CADENTIAL &&
               shape->starts_on >= PLAN_HARMONY_ANCHOR_ANY && shape->starts_on <= PLAN_HARMONY_ANCHOR_PIVOT &&
               shape->ends_on >= PLAN_HARMONY_ANCHOR_ANY && shape->ends_on <= PLAN_HARMONY_ANCHOR_DOMINANT &&
               shape->max_cells_per_chord >= 0 &&
               (!shape->pedal || (shape->kind == PLAN_HARMONY_SHAPE_PROLONGATION &&
                                  shape->starts_on == PLAN_HARMONY_ANCHOR_DOMINANT &&
                                  shape->ends_on == PLAN_HARMONY_ANCHOR_DOMINANT));
    }
    case PLAN_CONSTRAINT_ACCOMPANIMENT: {
        const PlanAccompaniment *accompaniment = &constraint->params.accompaniment;
        int low = accompaniment->low ? accompaniment->low : PLAN_ACCOMPANIMENT_DEFAULT_LOW;
        int high = accompaniment->high ? accompaniment->high : PLAN_ACCOMPANIMENT_DEFAULT_HIGH;
        return accompaniment->pattern >= PLAN_ACCOMPANIMENT_ANY && accompaniment->pattern <= PLAN_ACCOMPANIMENT_MOTIVE &&
               (accompaniment->pattern == PLAN_ACCOMPANIMENT_MOTIVE) == (accompaniment->motive_node[0] != '\0') &&
               memchr(accompaniment->motive_node, '\0', sizeof(accompaniment->motive_node)) &&
               accompaniment->texture >= -1 && accompaniment->texture < (int)STANDARD_TEXTURE_COUNT &&
               accompaniment->low >= 0 && accompaniment->low <= 127 && accompaniment->high >= 0 &&
               accompaniment->high <= 127 && low < high;
    }
    case PLAN_CONSTRAINT_VOICING:
        return true;
    case PLAN_CONSTRAINT_COUNTERPOINT:
        return constraint->params.counterpoint.species >= 1 && constraint->params.counterpoint.species <= 4;
    case PLAN_CONSTRAINT_INVENT_MOTIVE:
        return true;
    case PLAN_CONSTRAINT_STYLE: {
        const PlanStyle *style = &constraint->params.style;
        size_t length = strnlen(style->path, PLAN_STYLE_PATH_MAX);
        /* A weight of 0/0 is the default; any other has a denominator. */
        size_t blend_length = strnlen(style->blend_path, PLAN_STYLE_PATH_MAX);
        if (length == 0 || length >= PLAN_STYLE_PATH_MAX || blend_length >= PLAN_STYLE_PATH_MAX ||
            (style->weight_denominator == 0 && style->weight_numerator != 0)) {
            return false;
        }
        for (int d = 0; blend_length > 0 && d < 4; d++) {
            if (style->blend_denominator[d] == 0 || style->blend_numerator[d] > style->blend_denominator[d]) {
                return false;
            }
        }
        if (style->without_count > PLAN_STYLE_WITHOUT_MAX) {
            return false;
        }
        for (size_t i = 0; i < style->without_count; i++) {
            size_t name_length = strnlen(style->without[i], PLAN_STYLE_WITHOUT_NAME_MAX);
            if (name_length == 0 || name_length >= PLAN_STYLE_WITHOUT_NAME_MAX) {
                return false;
            }
        }
        return true;
    }
    case PLAN_CONSTRAINT_SOURCE: {
        const PlanSource *source = &constraint->params.source;
        size_t length = strnlen(source->piece, PLAN_SOURCE_PIECE_MAX);
        return length > 0 && length < PLAN_SOURCE_PIECE_MAX && source->score_denominator > 0 &&
               source->score_numerator <= source->score_denominator;
    }
    case PLAN_CONSTRAINT_PICKUP: {
        /* The bar it must be shorter than is the meter's, which the melody operator reads; no bar is as long as two wholes. */
        Rational pickup = constraint->params.pickup;
        return pickup.den > 0 && pickup.num > 0 && pickup.num < 2 * pickup.den &&
               plan_meter_pickup_on_grid(first_meter(plan), pickup);
    }
    case PLAN_CONSTRAINT_MOTIVE_TONES: {
        const PlanMotiveTones *tones = &constraint->params.motive_tones;
        if (tones->chord_count == 0 || tones->chord_count > PLAN_TONES_MAX_CHORDS || tones->label_count == 0 ||
            tones->label_count > PLAN_TONES_MAX_NOTES) {
            return false;
        }
        for (size_t i = 0; i < tones->chord_count; i++) {
            const PlanToneChord *chord = &tones->chords[i];
            size_t length = strnlen(chord->numeral, PLAN_ROMAN_NUMERAL_MAX);
            if (length == 0 || length >= PLAN_ROMAN_NUMERAL_MAX || chord->onset.den == 0 || chord->onset.num < 0 ||
                chord->length.den == 0 || chord->length.num <= 0) {
                return false;
            }
        }
        for (size_t i = 0; i < tones->label_count; i++) {
            size_t length = strnlen(tones->labels[i], PLAN_TONE_LABEL_MAX);
            if (length == 0 || length >= PLAN_TONE_LABEL_MAX) {
                return false;
            }
        }
        return true;
    }
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

/* ------------------------------------------------------------- the description of a section node */

const SectionDescription *plan_node_section(const PiecePlan *plan, NodeId id) {
    const PlanNode *node = plan_node_at(plan, id);
    return node && node->kind == PLAN_NODE_SECTION ? &node->section : NULL;
}

PlanResult plan_node_set_section(PiecePlan *plan, NodeId id, const SectionDescription *description) {
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

const SectionDescription *plan_section_or_empty(const PiecePlan *plan, NodeId node) {
    static const SectionDescription EMPTY;
    const SectionDescription *section = plan_node_section(plan, node);
    return section ? section : &EMPTY;
}

bool plan_section_has_role(const PiecePlan *plan, NodeId node, SectionRole role) {
    const SectionDescription *section = plan_node_section(plan, node);
    return section && section->has_role && section->role == role;
}

bool plan_section_part_of(const PiecePlan *plan, NodeId node, int *part) {
    const SectionDescription *section = plan_node_section(plan, node);
    if (!section || !section->has_role || section->role < SECTION_ROLE_INTRO || section->role > SECTION_ROLE_ENDING) {
        return false;
    }
    if (part) {
        *part = (int)(section->role - SECTION_ROLE_INTRO);
    }
    return true;
}

NodeId plan_section_named(const PiecePlan *plan, const char *name) {
    for (NodeId id = 1; name && id <= plan_node_count(plan); id++) {
        const char *node_name = plan_has_node(plan, id) && plan_node_kind(plan, id) == PLAN_NODE_SECTION ? plan_node_name(plan, id) : NULL;
        if (node_name && strcmp(node_name, name) == 0) {
            return id;
        }
    }
    return PLAN_NODE_NONE;
}

bool plan_node_cadence(const PiecePlan *plan, NodeId node, CadenceType *out) {
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
    /* a section's cadences are in its description */
    if (!constraint_is_valid(plan, constraint) || (constraint->kind == PLAN_CONSTRAINT_CADENCE && node->kind == PLAN_NODE_SECTION)) {
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

const PlanConstraint *plan_node_find_constraint(const PiecePlan *plan, NodeId id, PlanConstraintKind kind) {
    for (size_t i = 0; i < plan_node_constraint_count(plan, id); i++) {
        const PlanConstraint *constraint = plan_node_constraint_at(plan, id, i);
        if (constraint->kind == kind) {
            return constraint;
        }
    }
    return NULL;
}

const PlanConstraint *plan_lane_span_find_constraint(const PiecePlan *plan, LaneSpanId id, PlanConstraintKind kind) {
    for (size_t i = 0; i < plan_lane_span_constraint_count(plan, id); i++) {
        const PlanConstraint *constraint = plan_lane_span_constraint_at(plan, id, i);
        if (constraint->kind == kind) {
            return constraint;
        }
    }
    return NULL;
}

