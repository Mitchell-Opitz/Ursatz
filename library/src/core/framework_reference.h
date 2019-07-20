#ifndef MUSIC_CORE_FRAMEWORK_REFERENCE_H
#define MUSIC_CORE_FRAMEWORK_REFERENCE_H

#include <stdbool.h>

#include "identity/entity_id.h"

/* Opaque EntityID-shaped pointer to a Framework (L7). Carries no
   Framework fields/semantics; resolved only at L7+. Value equality on
   the referenced EntityID only. */

typedef struct FrameworkReference FrameworkReference;

typedef enum {
    FRAMEWORK_REFERENCE_OK = 0,
    FRAMEWORK_REFERENCE_ERR_NULL_TARGET_ID,
    FRAMEWORK_REFERENCE_ERR_ALLOC
} FrameworkReferenceResult;

/* Takes ownership of target_id on success. */
FrameworkReferenceResult framework_reference_create(EntityID *target_id, FrameworkReference **out);
void framework_reference_destroy(FrameworkReference *reference);

const EntityID *framework_reference_target_id(const FrameworkReference *reference);
bool framework_reference_equals(const FrameworkReference *a, const FrameworkReference *b);

#endif
