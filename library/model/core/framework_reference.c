#include "model/core/framework_reference.h"

#include <stdlib.h>

struct FrameworkReference {
    EntityID *target_id;
};

FrameworkReferenceResult framework_reference_create(EntityID *target_id, FrameworkReference **out) {
    if (target_id == NULL) {
        return FRAMEWORK_REFERENCE_ERR_NULL_TARGET_ID;
    }

    FrameworkReference *reference = (FrameworkReference *)malloc(sizeof(FrameworkReference));
    if (reference == NULL) {
        return FRAMEWORK_REFERENCE_ERR_ALLOC;
    }

    reference->target_id = target_id;

    *out = reference;
    return FRAMEWORK_REFERENCE_OK;
}

void framework_reference_destroy(FrameworkReference *reference) {
    if (reference == NULL) {
        return;
    }
    entity_id_destroy(reference->target_id);
    free(reference);
}
