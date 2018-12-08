#ifndef MUSIC_CORE_RELATIONSHIP_REFERENCE_H
#define MUSIC_CORE_RELATIONSHIP_REFERENCE_H

#include <stdbool.h>

#include "identity/entity_id.h"

/* Opaque EntityID-shaped pointer to a Relationship (L6). Carries no
   predicate/scope/context/evidence fields; resolved only at L6+. Value
   equality on the referenced EntityID only. */

typedef struct RelationshipReference RelationshipReference;

typedef enum {
    RELATIONSHIP_REFERENCE_OK = 0,
    RELATIONSHIP_REFERENCE_ERR_NULL_TARGET_ID,
    RELATIONSHIP_REFERENCE_ERR_ALLOC
} RelationshipReferenceResult;

/* Takes ownership of target_id on success. */
RelationshipReferenceResult relationship_reference_create(EntityID *target_id, RelationshipReference **out);
void relationship_reference_destroy(RelationshipReference *reference);

const EntityID *relationship_reference_target_id(const RelationshipReference *reference);
bool relationship_reference_equals(const RelationshipReference *a, const RelationshipReference *b);

#endif