#ifndef MUSIC_CONTAINERS_ARRANGEMENT_H
#define MUSIC_CONTAINERS_ARRANGEMENT_H

#include <stdbool.h>

#include "core/entity.h"
#include "identity/entity_id.h"

/*
 * Arrangement (Entity): an adapted version of a Work (containers/
 * work.h) -- e.g. a transcription for a different instrumentation.
 * Composes an Entity (core/entity.h, for identity) with work_id, an
 * EntityID reference to the Work this Arrangement adapts --
 * referenced, never embedded, same pattern Edition uses toward its own
 * work_id (containers/edition.h).
 *
 * Structurally identical to Edition ({Entity, work_id}), kept as a
 * distinct type rather than merged with it because the two are
 * semantically distinct per the Registry: Edition is a distinct
 * published version of a Work, Arrangement is an adapted version.
 * Nothing here assumes one can substitute for the other.
 *
 * The caller builds the Entity (id, type, schema_version, metadata,
 * provenance) beforehand and hands it to arrangement_create;
 * Arrangement does not construct or second-guess its Entity's fields.
 *
 * Arrangement does not itself resolve work_id to the actual Work
 * instance -- that lookup is a registry/repository concern (L9, out of
 * scope here).
 *
 * Arrangement has no mutator: a revision is a new Arrangement
 * construction, not an in-place field change (Law C2). Note Revision
 * (containers/revision.h, this same batch) formalizes that
 * construction-as-revision pattern for Work/Edition specifically --
 * Arrangement is not itself a Revision target this pass.
 *
 * Identity equality: Entity-category, so same EntityID + SchemaVersion,
 * regardless of instance (core/entity.h's entity_identity_equals,
 * delegated to directly). Referenced work_id does not factor in.
 *
 * Serialization: {entity, work_id}.
 */

typedef struct Arrangement Arrangement;

typedef enum {
    ARRANGEMENT_OK = 0,
    ARRANGEMENT_ERR_NULL_ENTITY,
    ARRANGEMENT_ERR_NULL_WORK_ID,
    ARRANGEMENT_ERR_ALLOC
} ArrangementResult;

/* Takes ownership of entity and work_id on success (both destroyed by arrangement_destroy). */
ArrangementResult arrangement_create(Entity *entity, EntityID *work_id, Arrangement **out);
void arrangement_destroy(Arrangement *arrangement);

const Entity *arrangement_entity(const Arrangement *arrangement);
const EntityID *arrangement_work_id(const Arrangement *arrangement);

bool arrangement_identity_equals(const Arrangement *a, const Arrangement *b);

#endif