#ifndef URSATZ_COMPOSER_FILL_OPERATOR_H
#define URSATZ_COMPOSER_FILL_OPERATOR_H

#include <stdbool.h>
#include <stddef.h>

#include <generation/generator.h>

#include <ursatz_composer/fill_rng.h>
#include <ursatz_composer/fill_target.h>
#include <ursatz_composer/piece_plan.h>

/*
 * Operators: what the fill loop calls to change a plan. There are three kinds.
 *   Refine  splits a node into children (form -> sections -> phrases -> ...).
 *   Grow    builds a node from its children (motive -> idea -> phrase ...). It
 *           fills a node that already exists; it does not insert one above
 *           existing nodes.
 *   Fill    fills a lane span.
 * Refine and Grow take a node as their target, Fill a lane span. The loop never
 * offers an operator a target of the other kind.
 *
 * An operator wraps Ursatz's Generator: its `generator` member carries the name,
 * version, determinism and descriptions (Generator.generate is not used, since
 * it takes a GenerationPlan and returns material ids, with no way to say which
 * node to fill or what to change).
 *
 * An operator must not touch Given material, and it must not change the plan in
 * generate(), which only proposes. Changes happen in apply(). Any randomness in
 * generate() must come from the FillRng it is given (see fill_rng.h).
 */

typedef enum {
    FILL_OK = 0,
    FILL_ERR_NULL,
    FILL_ERR_ALLOC,
    FILL_ERR_BAD_OPERATOR,
    FILL_ERR_DUPLICATE_OPERATOR,
    FILL_ERR_OPERATOR_FAILED, /* an operator's generate or apply failed */
    FILL_ERR_CHECKER_FAILED,  /* the checker failed, or returned a score with a zero denominator */
    FILL_ERR_TARGET_NOT_OPEN  /* the target is missing, or is not Constrained */
} FillResult;

/* --- candidates -------------------------------------------------------- */

#define FILL_LABEL_MAX 64

typedef void (*FillPayloadDestroyFn)(void *payload);

/*
 * One proposed change. The label is short text for the record and for people;
 * the payload is whatever the operator needs to apply the change, and only that
 * operator reads it (the checker may, if it knows the operator).
 */
typedef struct {
    char label[FILL_LABEL_MAX];
    void *payload;
    FillPayloadDestroyFn destroy; /* may be NULL */
} FillCandidate;

typedef struct {
    FillCandidate *items;
    size_t count;
    size_t capacity;
} FillCandidateList;

/* Adds a candidate, copying the label. Returns false on allocation failure, after destroying the payload. */
bool fill_candidates_add(FillCandidateList *list, const char *label, void *payload, FillPayloadDestroyFn destroy);
/* Destroys every payload and frees the list. */
void fill_candidates_clear(FillCandidateList *list);

/* --- operators --------------------------------------------------------- */

typedef enum {
    FILL_OPERATOR_REFINE,
    FILL_OPERATOR_GROW,
    FILL_OPERATOR_FILL
} FillOperatorKind;

typedef struct {
    Generator generator; /* name and version are required; the strings are borrowed and must outlive the registry */
    FillOperatorKind kind;
    /* Can this operator propose anything for the target? Required. */
    bool (*applies)(const PiecePlan *plan, FillTarget target, void *context);
    /* Proposes candidates into out. Returns false on failure. Required. */
    bool (*generate)(const PiecePlan *plan, FillTarget target, FillRng *rng, void *context, FillCandidateList *out);
    /* Applies one candidate to the plan. PLAN_OK on success. Required. */
    PlanResult (*apply)(PiecePlan *plan, FillTarget target, const FillCandidate *candidate, void *context);
    /* Adds constraints to the target's relatives (see fill_relatives) after apply. May be NULL. */
    void (*propagate)(PiecePlan *plan, FillTarget target, const FillCandidate *candidate, void *context);
    void *context; /* passed to every hook; may be NULL */
} FillOperator;

/* --- registry ---------------------------------------------------------- */

typedef struct FillOperatorRegistry FillOperatorRegistry;

FillOperatorRegistry *fill_registry_create(void);
void fill_registry_destroy(FillOperatorRegistry *registry);

/*
 * Registers a copy of operator. FILL_ERR_BAD_OPERATOR when the name or version
 * is empty or a required hook is missing; FILL_ERR_DUPLICATE_OPERATOR when the
 * name is taken. Operators are tried in the order they were registered, and that
 * order is part of what makes a run reproducible.
 */
FillResult fill_registry_register(FillOperatorRegistry *registry, const FillOperator *operator_);
size_t fill_registry_count(const FillOperatorRegistry *registry);
const FillOperator *fill_registry_at(const FillOperatorRegistry *registry, size_t index);

#endif
