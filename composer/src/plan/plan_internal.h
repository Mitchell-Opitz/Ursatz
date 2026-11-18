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
    SectionDescription section; /* a section node's description (R-607): what its letter, role, pairing, cadences, return and the rest say */
    bool removed; /* a node plan_remove_subtree took out: its id stays, nothing else does */
} PlanNode;

typedef struct {
    PlanLaneKind lane;
    int voice;
    NodeId owner;
    PlanSpan span;
    PlanLanePayload payload;
    PlanKeyRelation key_relation; /* relation[0] is '\0' for a span with none; key lane only */
    PlanState state;
    ConstraintList constraints;
    bool removed; /* as for a node */
} LaneSpan;

struct PiecePlan {
    PlanNode *nodes;
    size_t node_count;
    size_t node_capacity;
    LaneSpan *lane_spans;
    size_t lane_span_count;
    size_t lane_span_capacity;
    size_t removed_node_count;
    size_t removed_lane_span_count;
    char analyzer_version[PLAN_ANALYZER_VERSION_MAX]; /* "" unless the plan was loaded from an Analyzer result */
    PlanNoteId next_note_id;                          /* the id the next new note gets: 1 in a new plan, only rises */
};

/*
 * Gives the ids of a span's notes as the plan's rules say (lanes.h, "Note ids"): `notes` is the stored copy that is about to be, or
 * has just been, the payload of the span; `replacing` is the span it replaces (PLAN_LANE_SPAN_NONE for a new span), whose old
 * notes do not count as holders of an id.
 */
void plan_note_ids_assign(PiecePlan *plan, PlanNote *notes, size_t count, LaneSpanId replacing);

/* Sets the next note id (a plan file names it); it only rises: a value at or below the current one changes nothing. */
void plan_note_next_id_set(PiecePlan *plan, PlanNoteId next);

void constraint_list_free(ConstraintList *list);

/* dst takes everything src holds and src is destroyed; ids and pointers into dst's old contents are void. */
void plan_replace_contents(PiecePlan *dst, PiecePlan *src);

/* Frees every lane span and the note arrays they own. */
void plan_lanes_free(PiecePlan *plan);

/* Removes every lane span owner owns, whatever its state: the caller has checked that none is Given. */
PlanResult plan_lane_span_remove_owned(PiecePlan *plan, NodeId owner);

/*
 * Removes one lane span: PLAN_ERR_GIVEN_LOCKED for a Given one, PLAN_ERR_NOT_FOUND for an unknown id. What other
 * spans or nodes name it is not touched (nothing names a lane span).
 */
PlanResult plan_lane_span_remove(PiecePlan *plan, LaneSpanId id);

/*
 * Removes one node that has no children, with the lane spans it owns and the constraints that name it, as
 * plan_remove_subtree does for a subtree. PLAN_ERR_BAD_KIND for the root or a node with children,
 * PLAN_ERR_GIVEN_LOCKED when it or anything removed with it is Given, or a Given holder names it.
 */
PlanResult plan_node_remove_leaf(PiecePlan *plan, NodeId id);

/* True when payload is a legal payload for lane over span. */
bool lane_payload_is_valid(PlanLaneKind lane, PlanSpan span, const PlanLanePayload *payload);

/* NULL for LANE_SPAN_NONE, an unknown id or a removed span. */
LaneSpan *plan_lane_span_at(const PiecePlan *plan, LaneSpanId id);

/* NULL for PLAN_NODE_NONE, an unknown id or a removed node. */
PlanNode *plan_node_at(const PiecePlan *plan, NodeId id);

/*
 * The number a live node or lane span is given when the plan is written: its place among the ones that were not
 * removed, so a file's ids run from 1 without gaps. The id itself when nothing was removed.
 */
NodeId plan_node_rank(const PiecePlan *plan, NodeId id);
LaneSpanId plan_lane_span_rank(const PiecePlan *plan, LaneSpanId id);

#endif
