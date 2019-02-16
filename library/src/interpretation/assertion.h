#ifndef MUSIC_INTERPRETATION_ASSERTION_H
#define MUSIC_INTERPRETATION_ASSERTION_H

#include "core/value.h"
#include "identity/entity_id.h"

/*
 * Assertion (Category / base type): a claim made by an agent or
 * process, distinct from the underlying objects it describes (TDD
 * §80).
 *
 * subject is an opaque EntityID reference to the object the claim is
 * about -- referenced, never embedded/duplicated, the same
 * reference-only pattern used throughout L1-L5 and by Relationship's
 * source/target (relationship/relationship.h).
 *
 * claim is the content of the assertion itself. Its schema is not yet
 * specified at this layer, so it is kept as an opaque Value
 * (core/value.h) -- the same opaque-token treatment used elsewhere in
 * this codebase (e.g. Cadence's harmonic-context field,
 * structure/cadence.h) for content whose full shape belongs to a
 * later layer.
 *
 * agent is a reference to whoever/whatever made the assertion.
 * Provenance's Agent concept (ProvenanceAgent) does not exist yet at
 * the cross-cutting Provenance module -- this field is an opaque
 * agent-identifier string for now rather than blocking on that.
 * Flagged as a follow-up: replace with a real ProvenanceAgent
 * reference once it exists.
 */

typedef struct Assertion Assertion;

typedef enum {
    ASSERTION_OK = 0,
    ASSERTION_ERR_NULL_ID,
    ASSERTION_ERR_NULL_SUBJECT_ID,
    ASSERTION_ERR_NULL_CLAIM,
    ASSERTION_ERR_NULL_AGENT_IDENTIFIER,
    ASSERTION_ERR_ALLOC
} AssertionResult;

/*
 * Takes ownership of id, subject_id, and claim on success (all
 * required, destroyed by assertion_destroy). agent_identifier is
 * required (copied, non-empty).
 */
AssertionResult assertion_create(EntityID *id, EntityID *subject_id, Value *claim, const char *agent_identifier,
                                  Assertion **out);
void assertion_destroy(Assertion *assertion);

const EntityID *assertion_id(const Assertion *assertion);
const EntityID *assertion_subject_id(const Assertion *assertion);
const Value *assertion_claim(const Assertion *assertion);
const char *assertion_agent_identifier(const Assertion *assertion);

#endif