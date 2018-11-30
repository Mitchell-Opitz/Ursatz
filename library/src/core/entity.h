#ifndef MUSIC_CORE_ENTITY_H
#define MUSIC_CORE_ENTITY_H

#include <stdbool.h>

#include "identity/entity_id.h"
#include "identity/schema_version.h"

/*
 * Entity: base type for identifiable conceptual objects (Work, Instrument,
 * Voice, Motif, Framework, Rule, etc. live at higher layers as concrete
 * entities; this is only the base contract). Identity equality is
 * primary: same EntityID + SchemaVersion means the same entity,
 * regardless of instance.
 *
 * The Entity/Occurrence/Event category boundary is contested and
 * unresolved upstream (Domain Spec, System Registry) — this type does
 * not assume a resolution.
 *
 * provenance_reference is listed as a standard Entity field in the
 * System Registry but is intentionally not implemented here.
 * ProvenanceReference is a proposed L1 reference type not yet added to
 * the Registry (see the music-core plan's action item) — it is deferred
 * until that is resolved.
 */

typedef struct Entity Entity;

typedef enum {
    ENTITY_OK = 0,
    ENTITY_ERR_NULL_ID,
    ENTITY_ERR_NULL_TYPE,
    ENTITY_ERR_NULL_SCHEMA_VERSION,
    ENTITY_ERR_ALLOC
} EntityResult;

/*
 * Takes ownership of id and schema_version on success (they are
 * destroyed by entity_destroy). metadata may be NULL; if non-NULL it is
 * copied.
 */
EntityResult entity_create(EntityID *id, const char *type, SchemaVersion *schema_version,
                            const char *metadata, Entity **out);
void entity_destroy(Entity *entity);

const EntityID *entity_id(const Entity *entity);
const char *entity_type(const Entity *entity);
const SchemaVersion *entity_schema_version(const Entity *entity);
const char *entity_metadata(const Entity *entity);

/*
 * Identity equality: same EntityID + same SchemaVersion, regardless of
 * instance. Two entities with identical type/metadata but a different
 * EntityID are NOT identity-equal.
 */
bool entity_identity_equals(const Entity *a, const Entity *b);

#endif