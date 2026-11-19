#ifndef URSATZ_PLAN_PLAN_H
#define URSATZ_PLAN_PLAN_H

#include <stdbool.h>
#include <stddef.h>

#include <model/core/rational.h>

/*
 * The piece plan: a tree of Form > Section > Phrase > Subphrase/Idea > Motive
 * nodes, each owning a span of the plan timeline. It is the same tree
 * Ursatz-Analyzer writes for an existing piece, built in memory.
 *
 * Time is a library Rational position in whole notes from the start of the piece, in
 * lowest terms, the same unit as the Analyzer's analysis document.
 */

typedef struct Plan Plan;

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
 * The Analyzer version recorded in a plan file as metadata.analyzer_version, "" for a plan built here. A read and a write
 * keep it. plan_set_analyzer_version fails with PLAN_ERR_BAD_PAYLOAD for a version
 * of PLAN_ANALYZER_VERSION_MAX or more characters.
 */
#define PLAN_ANALYZER_VERSION_MAX 32
const char *plan_analyzer_version(const Plan *plan);
PlanResult plan_set_analyzer_version(Plan *plan, const char *version);

/*
 * The form a plan was built from (R-608): the name and version of its document, the hash of the built-in document it was built
 * from ("" for a form that is not built in), the checks the form runs (FormCheck bits, 31 at most) and whether it closes with a
 * cadence. A plan composes and checks its cadences from this and its section descriptions, not from a form found by name.
 * plan_set_form_record fails with PLAN_ERR_BAD_PAYLOAD for an empty name, a name of 32 or more characters, a hash that is not
 * "" or 16 lowercase hexadecimal digits, or checks above 31. plan_form_record is NULL for a plan that has none.
 */
#define PLAN_FORM_NAME_MAX 32
#define PLAN_FORM_HASH_LEN 16
typedef struct {
    char name[PLAN_FORM_NAME_MAX];
    unsigned version;
    char hash[PLAN_FORM_HASH_LEN + 1];
    unsigned checks;
    bool closing;
} PlanFormRecord;
PlanResult plan_set_form_record(Plan *plan, const PlanFormRecord *record);
const PlanFormRecord *plan_form_record(const Plan *plan);

/*
 * What a plan was made from (R-626, R-635): the id of the reading a plan was made from (a file name or a stored name, 1 to
 * PLAN_DERIVATION_READING_MAX - 1 characters) and the digest of the realization that reading names (16 lowercase hexadecimal digits).
 * Nothing sets it before a plan is made from a reading (P62 B14); a plan file keeps it as `derived_from` and a clone copies it.
 * plan_set_derived_from fails with PLAN_ERR_BAD_PAYLOAD for a reading that is empty or too long or a digest that is not 16 lowercase
 * hexadecimal digits. plan_derived_from is NULL for a plan that has none.
 */
#define PLAN_DERIVATION_READING_MAX 64
#define PLAN_DERIVATION_DIGEST_LEN 16
typedef struct {
    char reading[PLAN_DERIVATION_READING_MAX];
    char realization[PLAN_DERIVATION_DIGEST_LEN + 1];
} PlanDerivation;
PlanResult plan_set_derived_from(Plan *plan, const PlanDerivation *derivation);
const PlanDerivation *plan_derived_from(const Plan *plan);

/* The span's end time: start plus length. */
PlanResult plan_span_end(PlanSpan span, Rational *out);
/* True when inner lies entirely inside outer. */
bool plan_span_contains(PlanSpan outer, PlanSpan inner);

Plan *plan_create(void);
void plan_destroy(Plan *plan);

/*
 * Adds a node. A form is the root: parent is PLAN_NODE_NONE and a plan has one.
 * Every other kind needs a parent that may hold it:
 *   section under form, phrase under section, subphrase under phrase,
 *   idea under phrase or subphrase, motive under subphrase or idea.
 * The span must have a positive length and lie inside the parent's span.
 * name may be NULL; it is copied.
 */
PlanResult plan_add_node(Plan *plan, NodeId parent, PlanNodeKind kind, PlanSpan span, const char *name,
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
PlanResult plan_remove_subtree(Plan *plan, NodeId id);

bool plan_has_node(const Plan *plan, NodeId id);
/* The last id given out: ids run 1 to this count, and an id removed by plan_remove_subtree is not a node. */
size_t plan_node_count(const Plan *plan);
NodeId plan_root(const Plan *plan);

/* The accessors below return a zero value or NULL for an unknown id. */
PlanNodeKind plan_node_kind(const Plan *plan, NodeId id);
NodeId plan_node_parent(const Plan *plan, NodeId id);
PlanSpan plan_node_span(const Plan *plan, NodeId id);
const char *plan_node_name(const Plan *plan, NodeId id);
size_t plan_node_child_count(const Plan *plan, NodeId id);
NodeId plan_node_child_at(const Plan *plan, NodeId id, size_t index);

#endif
