#ifndef URSATZ_COMPOSER_PIECE_PLAN_H
#define URSATZ_COMPOSER_PIECE_PLAN_H

#include <stdbool.h>
#include <stddef.h>

#include <core/rational.h>

/*
 * The piece plan: a tree of Form > Section > Phrase > Subphrase/Idea > Motive
 * nodes, each owning a span of the plan timeline. It is the same tree
 * Ursatz-Analyzer writes for an existing piece, built in memory.
 *
 * Time is a library Rational position in whole notes from the start of the piece, in
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
    PLAN_ERR_BAD_INDEX,
    PLAN_ERR_IO,
    PLAN_ERR_PARSE,
    PLAN_ERR_FORMAT
} PlanResult;

typedef struct {
    Rational start;
    Rational length;
} PlanSpan;

typedef enum {
    PLAN_NODE_FORM,
    PLAN_NODE_SECTION,
    PLAN_NODE_PHRASE,
    PLAN_NODE_SUBPHRASE,
    PLAN_NODE_IDEA,
    PLAN_NODE_MOTIVE
} PlanNodeKind;

/*
 * The Analyzer version of the result a plan was loaded from (plan_from_analyzer_result), "" for any other plan. The plan
 * file records it as metadata.analyzer_version. plan_set_analyzer_version fails with PLAN_ERR_BAD_PAYLOAD for a version
 * of PLAN_ANALYZER_VERSION_MAX or more characters.
 */
#define PLAN_ANALYZER_VERSION_MAX 32
const char *plan_analyzer_version(const PiecePlan *plan);
PlanResult plan_set_analyzer_version(PiecePlan *plan, const char *version);

/* The span's end time: start plus length. */
PlanResult plan_span_end(PlanSpan span, Rational *out);
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

/*
 * Removes every descendant of id (not id itself) and every lane span they own, and every constraint, anywhere in the
 * plan, that names one of them (a motive a constraint is made from, a section a constraint returns). The ids of what
 * was removed are not reused and no longer exist: plan_has_node is false for them. A plan file renumbers what is left
 * from 1 when it is written.
 *
 * Nothing is changed and PLAN_ERR_GIVEN_LOCKED is returned when any of it is Given: a descendant, a lane span one
 * owns, or a node or span whose constraint would be removed. PLAN_ERR_NOT_FOUND for an unknown id.
 */
PlanResult plan_remove_subtree(PiecePlan *plan, NodeId id);

bool plan_has_node(const PiecePlan *plan, NodeId id);
/* The last id given out: ids run 1 to this count, and an id removed by plan_remove_subtree is not a node. */
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
