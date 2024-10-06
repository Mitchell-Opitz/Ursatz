#ifndef URSATZ_COMPOSER_FORM_RETURN_INTERNAL_H
#define URSATZ_COMPOSER_FORM_RETURN_INTERNAL_H

#include <stdbool.h>
#include <stddef.h>

#include <ursatz_composer/constraint.h>
#include <ursatz_composer/form_return.h>
#include <ursatz_composer/lanes.h>
#include <ursatz_composer/piece_plan.h>
#include <ursatz_composer/plan_state.h>

#define RETURN_PAYLOAD_MAGIC 0x52544E31UL
/* The owner of a node or span that belongs to the section itself. */
#define RETURN_SECTION ((size_t)-1)

/* One node of the subtree of a section, below the section. A node's parent comes before it. */
typedef struct {
    PlanNodeKind kind;
    size_t parent; /* an index into the copy's nodes, or RETURN_SECTION */
    PlanSpan span;
    char *name;
    PlanState state;
    PlanConstraint *constraints;
    size_t constraint_count;
} ReturnNode;

/* One written lane span under the section: harmony, bass, melody or inner voice. */
typedef struct {
    PlanLaneKind lane;
    int voice;
    size_t owner; /* an index into the copy's nodes, or RETURN_SECTION */
    PlanSpan span;
    PlanState state;
    PlanHarmony harmony; /* the chord, in the harmony lane */
    PlanNote *notes;     /* the notes, in the others */
    size_t note_count;
    PlanConstraint *constraints;
    size_t constraint_count;
} ReturnSpan;

/* A section's subtree and lane spans, copied out of the plan. */
typedef struct {
    ReturnNode *nodes;
    size_t node_count;
    size_t node_capacity;
    ReturnSpan *spans;
    size_t span_count;
    size_t span_capacity;
} ReturnCopy;

/* What a candidate of form-return carries: the returned section as it is, and the target as it is to become. */
typedef struct {
    unsigned long magic;
    PlanReturnVariation variation;
    PlanSpan span;       /* the target section's span */
    PlanKey key;         /* the key at its start */
    PlanMeter meter;
    int transpose;       /* the key difference, in semitones, the copy is moved by */
    ReturnCopy source;   /* the returned section, as read */
    ReturnCopy result;   /* the target's, after the variation */
} ReturnPayload;

/* Reads the subtree and written lane spans of section into out. False on allocation failure. */
bool return_copy_read(const PiecePlan *plan, NodeId section, ReturnCopy *out);
bool return_copy_clone(const ReturnCopy *from, ReturnCopy *out);
void return_copy_destroy(ReturnCopy *copy);
/* Moves the copy from a section starting at from to one starting at to, its pitches by transpose semitones. */
bool return_copy_move(ReturnCopy *copy, PlanTime from, PlanTime to, int transpose);
/* Adds the copy to the plan under section: its nodes, then its lane spans. */
PlanResult return_copy_write(PiecePlan *plan, NodeId section, const ReturnCopy *copy);

void return_payload_destroy(void *payload);

#endif
