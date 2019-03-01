#ifndef MUSIC_RELATIONSHIP_RELATIONSHIP_H
#define MUSIC_RELATIONSHIP_RELATIONSHIP_H

#include "core/provenance_reference.h"
#include "core/value.h"
#include "identity/entity_id.h"

/*
 * Relationship (Category / base type): a first-class connection
 * between objects (TDD §75).
 *
 * Fields: id, source, predicate, target, scope, context, qualifiers,
 * evidence, uncertainty, provenance.
 *
 * source/target are opaque EntityID references, not embedded objects
 * -- the same reference-only pattern used throughout L1-L5 (and by
 * music-structure's event refs). A Relationship never holds a copy of
 * the objects it connects.
 *
 * predicate is an open vocabulary name (string), not a closed enum:
 * PredicateDefinition (music-relationship, this module) is the
 * explicitly-extensible registry of predicate metadata (TDD §76-77);
 * Relationship itself only carries the name a PredicateDefinition can
 * later be looked up by, so this type has no compile-time dependency
 * on PredicateDefinition's construction order.
 *
 * context is an EntityID reference to a Context (music-context, L6).
 * It is a reference by id, not a Context* pointer: music-relationship
 * MUST NOT import music-context (sideways L6 dependency), so the
 * reference stays opaque the same way source/target do.
 *
 * evidence is likewise an opaque EntityID reference (to an Evidence,
 * music-interpretation) -- referenced, not embedded, and with no
 * compile-time dependency on music-interpretation.
 *
 * uncertainty is a PLACEHOLDER: Uncertainty (music-interpretation,
 * TDD §84) does not exist yet at this point in the build. Until it
 * does, this field is an opaque, nullable Value (core/value.h) --
 * the same opaque-token treatment Cadence's harmonic-context field
 * uses (structure/cadence.h) for a not-yet-built dependency. Flagged
 * as a follow-up: replace with a real Uncertainty reference once
 * music-interpretation exists.
 *
 * scope and qualifiers are similarly kept as opaque, nullable Values
 * for now -- minimal placeholders, not a frozen schema.
 *
 * provenance is required (ProvenanceReference, core/provenance_reference.h,
 * L0-L1): its constructor enforces the three-state presence model, so
 * there is no silently-omitted provenance here.
 */

typedef struct Relationship Relationship;

typedef enum {
    RELATIONSHIP_OK = 0,
    RELATIONSHIP_ERR_NULL_ID,
    RELATIONSHIP_ERR_NULL_SOURCE_ID,
    RELATIONSHIP_ERR_NULL_PREDICATE,
    RELATIONSHIP_ERR_NULL_TARGET_ID,
    RELATIONSHIP_ERR_NULL_PROVENANCE,
    RELATIONSHIP_ERR_ALLOC
} RelationshipResult;

/*
 * Takes ownership of id, source_id, target_id, and provenance on
 * success (all required, destroyed by relationship_destroy).
 * predicate_name is copied (required, non-empty).
 *
 * context_id, evidence_id, scope, qualifiers, and uncertainty are all
 * optional -- pass NULL for any not supplied. When non-NULL, ownership
 * of each transfers on success and each is destroyed by
 * relationship_destroy.
 */
RelationshipResult relationship_create(EntityID *id, EntityID *source_id, const char *predicate_name,
                                        EntityID *target_id, Value *scope, EntityID *context_id,
                                        Value *qualifiers, EntityID *evidence_id, Value *uncertainty,
                                        ProvenanceReference *provenance, Relationship **out);
void relationship_destroy(Relationship *relationship);

const EntityID *relationship_id(const Relationship *relationship);
const EntityID *relationship_source_id(const Relationship *relationship);
const char *relationship_predicate_name(const Relationship *relationship);
const EntityID *relationship_target_id(const Relationship *relationship);
const Value *relationship_scope(const Relationship *relationship);
const EntityID *relationship_context_id(const Relationship *relationship);
const Value *relationship_qualifiers(const Relationship *relationship);
const EntityID *relationship_evidence_id(const Relationship *relationship);
const Value *relationship_uncertainty(const Relationship *relationship);
const ProvenanceReference *relationship_provenance(const Relationship *relationship);

#endif