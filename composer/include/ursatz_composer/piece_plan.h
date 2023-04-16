#ifndef URSATZ_COMPOSER_PIECE_PLAN_H
#define URSATZ_COMPOSER_PIECE_PLAN_H

#include <stdbool.h>
#include <stddef.h>

/*
 * The piece plan: a tree of Form > Section > Phrase > Subphrase/Idea > Motive
 * nodes, each owning a span of the plan timeline. It is the same tree
 * Ursatz-Analyzer writes for an existing piece, built in memory.
 *
 * Time is a rational position in whole notes from the start of the piece, in
 * lowest terms, the same unit as the Analyzer's analysis document.
 */

typedef struct PiecePlan PiecePlan;

/* Stable for the life of the plan, never reused. PLAN_NODE_NONE is not a node. */
typedef size_t NodeId;
#define PLAN_NODE_NONE ((NodeId)0)

typedef enum {
    PLAN_OK = 0,
    PLAN_ERR_NULL,
    PLAN_ERR_ALLOC,
    PLAN_ERR_BAD_TIME,
    PLAN_ERR_BAD_SPAN,
    PLAN_ERR_BAD_KIND,
    PLAN_ERR_ROOT_EXISTS,
    PLAN_ERR_NOT_FOUND,
    PLAN_ERR_LANE_OVERLAP,
    PLAN_ERR_BAD_PAYLOAD,
    PLAN_ERR_GIVEN_LOCKED,
    PLAN_ERR_BAD_INDEX
} PlanResult;

typedef struct {
    long num;
    unsigned long den;
} PlanTime;

typedef struct {
    PlanTime start;
    PlanTime length;
} PlanSpan;

typedef enum {
    PLAN_NODE_FORM,
    PLAN_NODE_SECTION,
    PLAN_NODE_PHRASE,
    PLAN_NODE_SUBPHRASE,
    PLAN_NODE_IDEA,
    PLAN_NODE_MOTIVE
} PlanNodeKind;

/* Builds a time in lowest terms. Fails with PLAN_ERR_BAD_TIME on a zero denominator. */
PlanResult plan_time_make(long num, unsigned long den, PlanTime *out);
PlanResult plan_time_add(PlanTime a, PlanTime b, PlanTime *out);
/* Negative, zero or positive as a is before, equal to or after b. */
int plan_time_compare(PlanTime a, PlanTime b);

/* The span's end time: start plus length. */
PlanResult plan_span_end(PlanSpan span, PlanTime *out);
/* True when inner lies entirely inside outer. */
bool plan_span_contains(PlanSpan outer, PlanSpan inner);

PiecePlan *plan_create(void);
void plan_destroy(PiecePlan *plan);

/*
 * Adds a node. A form is the root: parent is PLAN_NODE_NONE and a plan has one.
 * Every other kind needs a parent that may hold it:
 *   section under form, phrase under section, subphrase under phrase,
 *   idea under phrase or subphrase, motive under subphrase or idea.
 * The span must have a positive length and lie inside the parent's span.
 * name may be NULL; it is copied.
 */
PlanResult plan_add_node(PiecePlan *plan, NodeId parent, PlanNodeKind kind, PlanSpan span, const char *name,
                         NodeId *out_id);

bool plan_has_node(const PiecePlan *plan, NodeId id);
size_t plan_node_count(const PiecePlan *plan);
NodeId plan_root(const PiecePlan *plan);

/* The accessors below return a zero value or NULL for an unknown id. */
PlanNodeKind plan_node_kind(const PiecePlan *plan, NodeId id);
NodeId plan_node_parent(const PiecePlan *plan, NodeId id);
PlanSpan plan_node_span(const PiecePlan *plan, NodeId id);
const char *plan_node_name(const PiecePlan *plan, NodeId id);
size_t plan_node_child_count(const PiecePlan *plan, NodeId id);
NodeId plan_node_child_at(const PiecePlan *plan, NodeId id, size_t index);

/* Called once per node. Return false to stop the walk. */
typedef bool (*PlanVisitFn)(const PiecePlan *plan, NodeId id, size_t depth, void *context);

/* Visits every node in pre-order: a node before its children, children in insertion order. */
PlanResult plan_walk(const PiecePlan *plan, PlanVisitFn visit, void *context);

#endif
