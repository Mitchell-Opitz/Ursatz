#ifndef MUSIC_CONTAINERS_REVISION_H
#define MUSIC_CONTAINERS_REVISION_H

#include <stdbool.h>

#include "core/entity.h"
#include "identity/entity_id.h"
#include "identity/version.h"

/*
 * Revision (Entity): a version of a Work (containers/work.h) or Edition
 * (containers/edition.h), using Version (identity/version.h, L0) for
 * its versioning semantics rather than inventing a parallel one.
 * Composes an Entity (core/entity.h, for identity) with target_id (an
 * EntityID reference to the Work or Edition being revised) and version
 * (the Version this Revision represents).
 *
 * target_id is a KNOWN GAP, flagged rather than silently resolved:
 * nothing in this codebase yet distinguishes "this EntityID names a
 * Work" from "this EntityID names an Edition" at the type level (no
 * discriminated/tagged reference exists for that, the same situation
 * Measure's meter_map_region_ref flags for MeterMap). target_id is
 * therefore a bare opaque EntityID that may resolve to either, exactly
 * as the Registry's own "Work/Edition" phrasing leaves it. Resolving
 * which is a registry/repository concern (L9) that also needs the
 * target's own type tag to disambiguate, not something this type can
 * decide on its own.
 *
 * The caller builds the Entity (id, type, schema_version, metadata,
 * provenance) and the Version beforehand and hands them to
 * revision_create; Revision does not construct or second-guess either.
 *
 * Revision does not itself resolve target_id to the actual Work or
 * Edition instance -- that lookup is a registry/repository concern (L9,
 * out of scope here).
 *
 * Revision has no mutator: a revision of a Revision is itself a new
 * Revision construction, not an in-place field change (Law C2) --
 * consistent with why Work/Edition/Arrangement have no mutator either.
 *
 * Identity equality: Entity-category, so same EntityID + SchemaVersion,
 * regardless of instance (core/entity.h's entity_identity_equals,
 * delegated to directly). Referenced target_id and version do not
 * factor in.
 *
 * Serialization: {entity, target_id, version}.
 */

typedef struct Revision Revision;

typedef enum {
    REVISION_OK = 0,
    REVISION_ERR_NULL_ENTITY,
    REVISION_ERR_NULL_TARGET_ID,
    REVISION_ERR_NULL_VERSION,
    REVISION_ERR_ALLOC
} RevisionResult;

/*
 * Takes ownership of entity, target_id, and version on success (all
 * destroyed by revision_destroy).
 */
RevisionResult revision_create(Entity *entity, EntityID *target_id, Version *version, Revision **out);
void revision_destroy(Revision *revision);

const Entity *revision_entity(const Revision *revision);
const EntityID *revision_target_id(const Revision *revision);
const Version *revision_version(const Revision *revision);

bool revision_identity_equals(const Revision *a, const Revision *b);

#endif