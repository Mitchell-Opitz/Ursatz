#ifndef MUSIC_CONTAINERS_EDITION_H
#define MUSIC_CONTAINERS_EDITION_H

#include <stdbool.h>

#include "core/entity.h"
#include "identity/entity_id.h"

/*
 * Edition (Entity): a distinct published version of a Work
 * (containers/work.h) -- e.g. a critical edition, a publisher's
 * engraved edition. Composes an Entity (core/entity.h, for identity)
 * with work_id, an EntityID reference to the Work this Edition
 * publishes -- referenced, never embedded, same pattern Work itself
 * uses toward Movement.
 *
 * The caller builds the Entity (id, type, schema_version, metadata,
 * provenance) beforehand and hands it to edition_create; Edition does
 * not construct or second-guess its Entity's fields.
 *
 * Edition does not itself resolve work_id to the actual Work instance
 * -- that lookup is a registry/repository concern (L9, out of scope
 * here).
 *
 * Edition has no mutator: a revision is a new Edition construction, not
 * an in-place field change (Law C2). Note Revision (containers/
 * revision.h, this same batch) formalizes that construction-as-revision
 * pattern for Work/Edition specifically.
 *
 * Identity equality: Entity-category, so same EntityID + SchemaVersion,
 * regardless of instance (core/entity.h's entity_identity_equals,
 * delegated to directly). Referenced work_id does not factor in.
 *
 * Serialization: {entity, work_id}.
 */

typedef struct Edition Edition;

typedef enum {
    EDITION_OK = 0,
    EDITION_ERR_NULL_ENTITY,
    EDITION_ERR_NULL_WORK_ID,
    EDITION_ERR_ALLOC
} EditionResult;

/* Takes ownership of entity and work_id on success (both destroyed by edition_destroy). */
EditionResult edition_create(Entity *entity, EntityID *work_id, Edition **out);
void edition_destroy(Edition *edition);

const Entity *edition_entity(const Edition *edition);
const EntityID *edition_work_id(const Edition *edition);

bool edition_identity_equals(const Edition *a, const Edition *b);

#endif