#include "core/provenance_reference.h"

#include <stdlib.h>

struct ProvenanceReference {
    ProvenanceReferenceState state;
    EntityID *target_id;
};

ProvenanceReferenceResult provenance_reference_create_present(EntityID *target_id, ProvenanceReference **out) {
    if (target_id == NULL) {
        return PROVENANCE_REFERENCE_ERR_NULL_TARGET_ID;
    }

    ProvenanceReference *reference = (ProvenanceReference *)malloc(sizeof(ProvenanceReference));
    if (reference == NULL) {
        return PROVENANCE_REFERENCE_ERR_ALLOC;
    }

    reference->state = PROVENANCE_REFERENCE_PRESENT;
    reference->target_id = target_id;

    *out = reference;
    return PROVENANCE_REFERENCE_OK;
}

ProvenanceReferenceResult provenance_reference_create_absent(ProvenanceReference **out) {
    ProvenanceReference *reference = (ProvenanceReference *)malloc(sizeof(ProvenanceReference));
    if (reference == NULL) {
        return PROVENANCE_REFERENCE_ERR_ALLOC;
    }

    reference->state = PROVENANCE_REFERENCE_ABSENT;
    reference->target_id = NULL;

    *out = reference;
    return PROVENANCE_REFERENCE_OK;
}

void provenance_reference_destroy(ProvenanceReference *reference) {
    if (reference == NULL) {
        return;
    }
    entity_id_destroy(reference->target_id);
    free(reference);
}

ProvenanceReferenceState provenance_reference_state(const ProvenanceReference *reference) {
    return reference->state;
}

const EntityID *provenance_reference_target_id(const ProvenanceReference *reference) {
    return reference->target_id;
}

bool provenance_reference_equals(const ProvenanceReference *a, const ProvenanceReference *b) {
    if (a == NULL || b == NULL) {
        return false;
    }
    if (a->state != b->state) {
        return false;
    }
    if (a->state == PROVENANCE_REFERENCE_ABSENT) {
        return true;
    }
    return entity_id_equals(a->target_id, b->target_id);
}