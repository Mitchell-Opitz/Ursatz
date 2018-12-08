#ifndef MUSIC_CORE_OCCURRENCE_H
#define MUSIC_CORE_OCCURRENCE_H

#include "identity/entity_id.h"

/*
 * Occurrence: base type for a situated instance of a conceptual object.
 * A Motif is one abstract thing; MotifOccurrence O1, O2, O3 are situated
 * instances of it. An Occurrence distinguishes conceptual identity (the
 * target it is an occurrence of) from manifestation (its own identity) —
 * a pattern used throughout later layers.
 *
 * An Occurrence references its target concept by EntityID only. It does
 * not hold, copy, or duplicate the target's fields — resolving the
 * target concept is the concern of whatever higher-layer code looks it
 * up by that id.
 *
 * The Occurrence/Entity/Event category boundary is contested and
 * unresolved upstream (Domain Spec, System Registry) — this type does
 * not assume a resolution.
 */

typedef struct Occurrence Occurrence;

typedef enum {
    OCCURRENCE_OK = 0,
    OCCURRENCE_ERR_NULL_ID,
    OCCURRENCE_ERR_NULL_TARGET_ID,
    OCCURRENCE_ERR_ALLOC
} OccurrenceResult;

/* Takes ownership of id and target_id on success (destroyed by occurrence_destroy). */
OccurrenceResult occurrence_create(EntityID *id, EntityID *target_id, Occurrence **out);
void occurrence_destroy(Occurrence *occurrence);

const EntityID *occurrence_id(const Occurrence *occurrence);
const EntityID *occurrence_target_id(const Occurrence *occurrence);

#endif