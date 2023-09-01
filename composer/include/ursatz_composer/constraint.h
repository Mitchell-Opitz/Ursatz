#ifndef URSATZ_COMPOSER_CONSTRAINT_H
#define URSATZ_COMPOSER_CONSTRAINT_H

#include <stddef.h>

#include <ursatz_composer/lanes.h>
#include <ursatz_composer/piece_plan.h>

/*
 * Constraints: requirements attached to a node or a lane span ("8 bars",
 * "ends with a PAC", "uses motive m1 inverted", "embellish this chord"). They are data only; nothing
 * here evaluates or checks them. A node or span keeps its constraints in the
 * order they were added. Adding or removing one on a Given node or span is
 * refused with PLAN_ERR_GIVEN_LOCKED.
 */

typedef enum {
    PLAN_CONSTRAINT_LENGTH_BARS,
    PLAN_CONSTRAINT_CADENCE,
    PLAN_CONSTRAINT_MOTIVE_REF,
    PLAN_CONSTRAINT_KEY,
    PLAN_CONSTRAINT_ARCHETYPE,
    PLAN_CONSTRAINT_EMBELLISH, /* no parameters: a harmony span reopened for the embellishment pass */
    PLAN_CONSTRAINT_SCHEMA     /* the galant schema a span was made from, and which stage of it */
} PlanConstraintKind;

typedef enum {
    PLAN_CADENCE_PAC,
    PLAN_CADENCE_IAC,
    PLAN_CADENCE_HC,
    PLAN_CADENCE_DC,
    PLAN_CADENCE_PC
} PlanCadenceType;

typedef enum {
    PLAN_TRANSFORM_NONE,
    PLAN_TRANSFORM_TRANSPOSED,
    PLAN_TRANSFORM_INVERTED,
    PLAN_TRANSFORM_RETROGRADE,
    PLAN_TRANSFORM_AUGMENTED,
    PLAN_TRANSFORM_DIMINISHED,
    PLAN_TRANSFORM_FRAGMENTED
} PlanMotiveTransform;

typedef enum {
    PLAN_ARCHETYPE_SENTENCE,
    PLAN_ARCHETYPE_PERIOD,
    PLAN_ARCHETYPE_HYBRID
} PlanArchetype;

/* motive is a motive node in the same plan; semitones is used only by PLAN_TRANSFORM_TRANSPOSED. */
typedef struct {
    NodeId motive;
    PlanMotiveTransform transform;
    int semitones;
} PlanMotiveRef;

/*
 * The schema a span was made from: schema is the id of a galant-schemas prototype (GalantSchemaId in
 * Ursatz's frameworks/galant-schemas), stage the index of its stage the span sounds. The prototype's
 * other voices (the melody) are read from the framework by that id, not copied here.
 */
typedef struct {
    int schema;
    int stage;
} PlanSchemaRef;

/* The member of params that is used follows the kind. */
typedef struct {
    PlanConstraintKind kind;
    union {
        int bars;
        PlanCadenceType cadence;
        PlanMotiveRef motive;
        PlanKey key;
        PlanArchetype archetype;
        PlanSchemaRef schema;
    } params;
} PlanConstraint;

/* Fails with PLAN_ERR_BAD_PAYLOAD for a constraint that is malformed or names a missing motive node. */
PlanResult plan_node_add_constraint(PiecePlan *plan, NodeId id, const PlanConstraint *constraint);
size_t plan_node_constraint_count(const PiecePlan *plan, NodeId id);
/* NULL for an unknown id or an index past the end. The pointer is valid until the list changes. */
const PlanConstraint *plan_node_constraint_at(const PiecePlan *plan, NodeId id, size_t index);
/* Later constraints shift down one. PLAN_ERR_BAD_INDEX for an index past the end. */
PlanResult plan_node_remove_constraint(PiecePlan *plan, NodeId id, size_t index);

PlanResult plan_lane_span_add_constraint(PiecePlan *plan, LaneSpanId id, const PlanConstraint *constraint);
size_t plan_lane_span_constraint_count(const PiecePlan *plan, LaneSpanId id);
const PlanConstraint *plan_lane_span_constraint_at(const PiecePlan *plan, LaneSpanId id, size_t index);
PlanResult plan_lane_span_remove_constraint(PiecePlan *plan, LaneSpanId id, size_t index);

#endif
