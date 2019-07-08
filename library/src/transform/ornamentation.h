#ifndef MUSIC_TRANSFORM_ORNAMENTATION_H
#define MUSIC_TRANSFORM_ORNAMENTATION_H

#include <stdbool.h>
#include <stddef.h>

#include "core/provenance_reference.h"
#include "identity/entity_id.h"
#include "transform/transformation.h"

/*
 * Ornamentation (Transformation subtype): one-to-many event
 * derivation -- a single source event decorated into several derived
 * events (e.g. a trill, turn, or mordent expanded into its constituent
 * notes). Per the Registry, Ornamentation is the one Transformation
 * subtype whose row explicitly calls out Provenance as a required
 * dependency, because this fan-out mapping (not a plain one-to-one
 * transform) is the trickiest to get right -- every other concrete
 * Transformation in this module needs provenance too, via the base
 * Transformation contract (transform/transformation.h), but none of
 * them fan out like this.
 *
 * source_event_id is a required, owned EntityID reference to the
 * ornamented event -- referenced, never embedded.
 *
 * derived_event_ids/derived_event_provenances/derived_event_count is
 * the explicit one-to-many mapping: parallel owned arrays, one entry
 * per derived event. derived_event_ids follows the same
 * array-of-references-plus-count shape MotifOccurrence/Fragmentation/
 * Expansion/Contraction use for their own event-id arrays. Each
 * derived_event_provenances[i] is a required, owned ProvenanceReference
 * that MUST be PRESENT (never ABSENT -- an ornament event's derivation
 * is never optional to record) and MUST target source_event_id, so the
 * mapping from every derived event back to its single source is
 * explicit and machine-checkable, not left to be inferred from array
 * position alone.
 *
 * Owns a Transformation (transform/transformation.h) holding the base
 * §139 contract fields: input_type=output_type="NoteEvent",
 * mapping_behavior states the fan-out explicitly.
 * ornamentation_id forwards to the owned Transformation.
 */

typedef struct Ornamentation Ornamentation;

typedef enum {
    ORNAMENTATION_OK = 0,
    ORNAMENTATION_ERR_NULL_ID,
    ORNAMENTATION_ERR_NULL_SOURCE_EVENT_ID,
    ORNAMENTATION_ERR_NULL_DERIVED_EVENT_IDS,
    ORNAMENTATION_ERR_NULL_DERIVED_EVENT_ID,
    ORNAMENTATION_ERR_NULL_DERIVED_EVENT_PROVENANCES,
    ORNAMENTATION_ERR_NULL_DERIVED_EVENT_PROVENANCE,
    ORNAMENTATION_ERR_DERIVED_EVENT_PROVENANCE_ABSENT,
    ORNAMENTATION_ERR_DERIVED_EVENT_PROVENANCE_TARGET_MISMATCH,
    ORNAMENTATION_ERR_NULL_PROVENANCE,
    ORNAMENTATION_ERR_ALLOC
} OrnamentationResult;

/*
 * Takes ownership of id, source_event_id, provenance (all transitively
 * via the owned Transformation, id also directly reachable via
 * ornamentation_id), plus derived_event_ids and derived_event_provenances
 * (and every entry each points to) on success -- all destroyed by
 * ornamentation_destroy.
 *
 * derived_event_ids and derived_event_provenances MUST both be
 * non-NULL with derived_event_count > 0 and the same length (implied
 * by the shared count parameter -- callers pass arrays of that exact
 * length). No entry in either array may be NULL. Every
 * derived_event_provenances[i] MUST have state
 * PROVENANCE_REFERENCE_PRESENT with a target_id equal to
 * source_event_id (entity_id_equals) -- an ABSENT entry fails with
 * ORNAMENTATION_ERR_DERIVED_EVENT_PROVENANCE_ABSENT, and a PRESENT
 * entry targeting a different id fails with
 * ORNAMENTATION_ERR_DERIVED_EVENT_PROVENANCE_TARGET_MISMATCH.
 */
OrnamentationResult ornamentation_create(EntityID *id, EntityID *source_event_id, EntityID **derived_event_ids,
                                          ProvenanceReference **derived_event_provenances, size_t derived_event_count,
                                          ProvenanceReference *provenance, Ornamentation **out);
void ornamentation_destroy(Ornamentation *ornamentation);

const EntityID *ornamentation_id(const Ornamentation *ornamentation);
const Transformation *ornamentation_transformation(const Ornamentation *ornamentation);
const EntityID *ornamentation_source_event_id(const Ornamentation *ornamentation);
size_t ornamentation_derived_event_count(const Ornamentation *ornamentation);
const EntityID *ornamentation_derived_event_id_at(const Ornamentation *ornamentation, size_t index);
const ProvenanceReference *ornamentation_derived_event_provenance_at(const Ornamentation *ornamentation,
                                                                      size_t index);

bool ornamentation_identity_equals(const Ornamentation *a, const Ornamentation *b);

#endif