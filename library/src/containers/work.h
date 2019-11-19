#ifndef MUSIC_CONTAINERS_WORK_H
#define MUSIC_CONTAINERS_WORK_H

#include <stdbool.h>
#include <stddef.h>

#include "core/entity.h"
#include "identity/entity_id.h"

/*
 * Work (Entity/Container): the top-level musical work. This is the
 * eventual root object for anything exported (MIDI/MusicXML) or
 * generated as a complete piece -- the most structurally important type
 * in this batch.
 *
 * Composes an Entity (core/entity.h, for identity) with an owned array
 * of EntityID references to Movement (containers/movement.h) -- same
 * composition pattern structure/structure.h's Structure uses toward its
 * own Entity + event_ids. The caller builds the Entity (id, type,
 * schema_version, metadata, provenance) beforehand and hands it to
 * work_create; Work does not construct or second-guess its Entity's
 * fields.
 *
 * Unlike Structure (which requires a non-empty event_ids), movement_ids
 * here may be empty (count == 0, pass NULL) -- a Work may exist before
 * any Movement is attached to it, the same "found nothing is not an
 * error" treatment this batch's other new containers (Staff, Part,
 * Phrase, Section, Movement) give their own reference arrays.
 *
 * Work does not itself resolve movement_ids to actual Movement
 * instances -- that lookup is a registry/repository concern (L9, out of
 * scope here).
 *
 * Work has no mutator: a revision is a new Work construction, not an
 * in-place field change (Law C2). Note Revision (containers/revision.h,
 * this same batch) formalizes that construction-as-revision pattern for
 * Work/Edition specifically.
 *
 * Identity equality: Entity-category, so same EntityID + SchemaVersion,
 * regardless of instance (core/entity.h's entity_identity_equals,
 * delegated to directly). Referenced movement_ids do not factor in.
 *
 * Serialization: {entity, movement_ids}.
 */

typedef struct Work Work;

typedef enum {
    WORK_OK = 0,
    WORK_ERR_NULL_ENTITY,
    WORK_ERR_NULL_MOVEMENT_ID,
    WORK_ERR_ALLOC
} WorkResult;

/*
 * Takes ownership of entity and each EntityID in movement_ids (if any)
 * on success (all destroyed by work_destroy). movement_ids is
 * optional -- pass NULL with count == 0 if not supplied. No entry of a
 * non-empty movement_ids array may be NULL.
 */
WorkResult work_create(Entity *entity, EntityID **movement_ids, size_t movement_id_count, Work **out);
void work_destroy(Work *work);

const Entity *work_entity(const Work *work);
size_t work_movement_id_count(const Work *work);
const EntityID *work_movement_id_at(const Work *work, size_t index);

bool work_identity_equals(const Work *a, const Work *b);

#endif