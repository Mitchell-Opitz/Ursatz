#include "core/occurrence.h"

#include <stdlib.h>

struct Occurrence {
    EntityID *id;
    EntityID *target_id;
};

OccurrenceResult occurrence_create(EntityID *id, EntityID *target_id, Occurrence **out) {
    if (id == NULL) {
        return OCCURRENCE_ERR_NULL_ID;
    }
    if (target_id == NULL) {
        return OCCURRENCE_ERR_NULL_TARGET_ID;
    }

    Occurrence *occurrence = (Occurrence *)malloc(sizeof(Occurrence));
    if (occurrence == NULL) {
        return OCCURRENCE_ERR_ALLOC;
    }

    occurrence->id = id;
    occurrence->target_id = target_id;

    *out = occurrence;
    return OCCURRENCE_OK;
}

void occurrence_destroy(Occurrence *occurrence) {
    if (occurrence == NULL) {
        return;
    }
    entity_id_destroy(occurrence->id);
    entity_id_destroy(occurrence->target_id);
    free(occurrence);
}

const EntityID *occurrence_id(const Occurrence *occurrence) {
    return occurrence->id;
}

const EntityID *occurrence_target_id(const Occurrence *occurrence) {
    return occurrence->target_id;
}