#include "core/relationship_reference.h"

#include <stdlib.h>

struct RelationshipReference {
    EntityID *target_id;
};

RelationshipReferenceResult relationship_reference_create(EntityID *target_id, RelationshipReference **out) {
    if (target_id == NULL) {
        return RELATIONSHIP_REFERENCE_ERR_NULL_TARGET_ID;
    }

    RelationshipReference *reference = (RelationshipReference *)malloc(sizeof(RelationshipReference));
    if (reference == NULL) {
        return RELATIONSHIP_REFERENCE_ERR_ALLOC;
    }

    reference->target_id = target_id;

    *out = reference;
    return RELATIONSHIP_REFERENCE_OK;
}

void relationship_reference_destroy(RelationshipReference *reference) {
    if (reference == NULL) {
        return;
    }
    entity_id_destroy(reference->target_id);
    free(reference);
}

const EntityID *relationship_reference_target_id(const RelationshipReference *reference) {
    return reference->target_id;
}

bool relationship_reference_equals(const RelationshipReference *a, const RelationshipReference *b) {
    if (a == NULL || b == NULL) {
        return false;
    }
    return entity_id_equals(a->target_id, b->target_id);
}