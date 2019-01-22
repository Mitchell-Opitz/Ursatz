#include "structure/cadence.h"

#include <stdlib.h>

struct Cadence {
    EntityID *id;
    EntityID **event_ids;
    size_t event_id_count;
    Value *optional_harmonic_context;
};

CadenceResult cadence_create(EntityID *id, EntityID **event_ids, size_t event_id_count,
                              Value *optional_harmonic_context, Cadence **out) {
    if (id == NULL) {
        return CADENCE_ERR_NULL_ID;
    }
    if (event_ids == NULL || event_id_count == 0) {
        return CADENCE_ERR_NULL_EVENT_IDS;
    }
    for (size_t i = 0; i < event_id_count; i++) {
        if (event_ids[i] == NULL) {
            return CADENCE_ERR_NULL_EVENT_ID;
        }
    }

    Cadence *cadence = (Cadence *)malloc(sizeof(Cadence));
    if (cadence == NULL) {
        return CADENCE_ERR_ALLOC;
    }

    EntityID **owned_ids = (EntityID **)malloc(sizeof(EntityID *) * event_id_count);
    if (owned_ids == NULL) {
        free(cadence);
        return CADENCE_ERR_ALLOC;
    }
    for (size_t i = 0; i < event_id_count; i++) {
        owned_ids[i] = event_ids[i];
    }

    cadence->id = id;
    cadence->event_ids = owned_ids;
    cadence->event_id_count = event_id_count;
    cadence->optional_harmonic_context = optional_harmonic_context;

    *out = cadence;
    return CADENCE_OK;
}

void cadence_destroy(Cadence *cadence) {
    if (cadence == NULL) {
        return;
    }
    entity_id_destroy(cadence->id);
    for (size_t i = 0; i < cadence->event_id_count; i++) {
        entity_id_destroy(cadence->event_ids[i]);
    }
    free(cadence->event_ids);
    value_destroy(cadence->optional_harmonic_context);
    free(cadence);
}

const EntityID *cadence_id(const Cadence *cadence) {
    return cadence->id;
}

size_t cadence_event_id_count(const Cadence *cadence) {
    return cadence->event_id_count;
}

const EntityID *cadence_event_id_at(const Cadence *cadence, size_t index) {
    return cadence->event_ids[index];
}

const Value *cadence_optional_harmonic_context(const Cadence *cadence) {
    return cadence->optional_harmonic_context;
}

bool cadence_identity_equals(const Cadence *a, const Cadence *b) {
    if (a == NULL || b == NULL) {
        return false;
    }
    return entity_id_equals(a->id, b->id);
}