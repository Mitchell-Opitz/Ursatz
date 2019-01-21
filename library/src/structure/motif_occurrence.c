#include "structure/motif_occurrence.h"

#include <stdlib.h>

struct MotifOccurrence {
    Occurrence *occurrence;
    EntityID **region_event_ids;
    size_t region_event_id_count;
};

MotifOccurrenceResult motif_occurrence_create(Occurrence *occurrence, EntityID **region_event_ids,
                                               size_t region_event_id_count, MotifOccurrence **out) {
    if (occurrence == NULL) {
        return MOTIF_OCCURRENCE_ERR_NULL_OCCURRENCE;
    }
    if (region_event_ids == NULL || region_event_id_count == 0) {
        return MOTIF_OCCURRENCE_ERR_NULL_REGION_EVENT_IDS;
    }
    for (size_t i = 0; i < region_event_id_count; i++) {
        if (region_event_ids[i] == NULL) {
            return MOTIF_OCCURRENCE_ERR_NULL_REGION_EVENT_ID;
        }
    }

    MotifOccurrence *motif_occurrence = (MotifOccurrence *)malloc(sizeof(MotifOccurrence));
    if (motif_occurrence == NULL) {
        return MOTIF_OCCURRENCE_ERR_ALLOC;
    }

    EntityID **owned_ids = (EntityID **)malloc(sizeof(EntityID *) * region_event_id_count);
    if (owned_ids == NULL) {
        free(motif_occurrence);
        return MOTIF_OCCURRENCE_ERR_ALLOC;
    }
    for (size_t i = 0; i < region_event_id_count; i++) {
        owned_ids[i] = region_event_ids[i];
    }

    motif_occurrence->occurrence = occurrence;
    motif_occurrence->region_event_ids = owned_ids;
    motif_occurrence->region_event_id_count = region_event_id_count;

    *out = motif_occurrence;
    return MOTIF_OCCURRENCE_OK;
}

void motif_occurrence_destroy(MotifOccurrence *motif_occurrence) {
    if (motif_occurrence == NULL) {
        return;
    }
    occurrence_destroy(motif_occurrence->occurrence);
    for (size_t i = 0; i < motif_occurrence->region_event_id_count; i++) {
        entity_id_destroy(motif_occurrence->region_event_ids[i]);
    }
    free(motif_occurrence->region_event_ids);
    free(motif_occurrence);
}

const EntityID *motif_occurrence_id(const MotifOccurrence *motif_occurrence) {
    return occurrence_id(motif_occurrence->occurrence);
}

const EntityID *motif_occurrence_motif_id(const MotifOccurrence *motif_occurrence) {
    return occurrence_target_id(motif_occurrence->occurrence);
}

size_t motif_occurrence_region_event_id_count(const MotifOccurrence *motif_occurrence) {
    return motif_occurrence->region_event_id_count;
}

const EntityID *motif_occurrence_region_event_id_at(const MotifOccurrence *motif_occurrence, size_t index) {
    return motif_occurrence->region_event_ids[index];
}