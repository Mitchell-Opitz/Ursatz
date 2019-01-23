#ifndef MUSIC_STRUCTURE_STRUCTURE_H
#define MUSIC_STRUCTURE_STRUCTURE_H

#include <stddef.h>

#include "core/entity.h"
#include "identity/entity_id.h"

/*
 * Structure: base category for higher-level semantic organization of
 * musical material (this pass: Motif, Cadence; later/deferred: Figure,
 * Phrase, Theme, Section, Movement, Work -- see the music-structure
 * module notes for what is and is not built yet).
 *
 * A Structure composes an Entity (for identity) with a set of event
 * references -- EntityID pointers at the NoteEvents that make up the
 * structure. It holds only those references, never a resolved
 * NoteEvent or a copy of its fields (TDD Sec56, Sec152): the type
 * signature only accepts EntityID, so there is nothing here through
 * which event data could be embedded or duplicated.
 *
 * No hierarchy is assumed between Structure subtypes (TDD Sec57) -- a
 * Structure graph is not required to follow Motif -> Phrase -> Theme
 * -> Section as a universal shape, and nothing in this base type
 * hard-codes that shape.
 */

typedef struct Structure Structure;

typedef enum {
    STRUCTURE_OK = 0,
    STRUCTURE_ERR_NULL_ENTITY,
    STRUCTURE_ERR_NULL_EVENT_IDS,
    STRUCTURE_ERR_NULL_EVENT_ID,
    STRUCTURE_ERR_ALLOC
} StructureResult;

/*
 * Takes ownership of entity and of event_ids (and each EntityID it
 * points to) on success -- all destroyed by structure_destroy.
 * event_ids must be non-NULL with event_id_count > 0; no entry may be
 * NULL.
 */
StructureResult structure_create(Entity *entity, EntityID **event_ids, size_t event_id_count, Structure **out);
void structure_destroy(Structure *structure);

const Entity *structure_entity(const Structure *structure);
size_t structure_event_id_count(const Structure *structure);
const EntityID *structure_event_id_at(const Structure *structure, size_t index);

#endif