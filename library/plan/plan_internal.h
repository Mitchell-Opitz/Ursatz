#ifndef URSATZ_PLAN_INTERNAL_H
#define URSATZ_PLAN_INTERNAL_H

#include "plan/plan_constraint.h"
#include "plan/plan_lanes.h"
#include "plan/plan_edit.h"
#include "plan/plan.h"
#include "plan/plan_state.h"

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
    bool has_form_record;
    PlanFormRecord form_record;                       /* the form the plan was built from, when has_form_record */
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

/* Frees every lane span and the note arrays they own. */
void plan_lanes_free(PiecePlan *plan);

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
