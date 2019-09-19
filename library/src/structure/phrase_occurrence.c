#include "structure/phrase_occurrence.h"

#include <stdlib.h>

struct PhraseOccurrence {
    Occurrence *occurrence;
    EntityID **region_event_ids;
    size_t region_event_id_count;
};

PhraseOccurrenceResult phrase_occurrence_create(Occurrence *occurrence, EntityID **region_event_ids,
                                                 size_t region_event_id_count, PhraseOccurrence **out) {
    if (occurrence == NULL) {
        return PHRASE_OCCURRENCE_ERR_NULL_OCCURRENCE;
    }
    if (region_event_ids == NULL || region_event_id_count == 0) {
        return PHRASE_OCCURRENCE_ERR_NULL_REGION_EVENT_IDS;
    }
    for (size_t i = 0; i < region_event_id_count; i++) {
        if (region_event_ids[i] == NULL) {
            return PHRASE_OCCURRENCE_ERR_NULL_REGION_EVENT_ID;
        }
    }

    PhraseOccurrence *phrase_occurrence = (PhraseOccurrence *)malloc(sizeof(PhraseOccurrence));
    if (phrase_occurrence == NULL) {
        return PHRASE_OCCURRENCE_ERR_ALLOC;
    }

    EntityID **owned_ids = (EntityID **)malloc(sizeof(EntityID *) * region_event_id_count);
    if (owned_ids == NULL) {
        free(phrase_occurrence);
        return PHRASE_OCCURRENCE_ERR_ALLOC;
    }
    for (size_t i = 0; i < region_event_id_count; i++) {
        owned_ids[i] = region_event_ids[i];
    }

    phrase_occurrence->occurrence = occurrence;
    phrase_occurrence->region_event_ids = owned_ids;
    phrase_occurrence->region_event_id_count = region_event_id_count;

    *out = phrase_occurrence;
    return PHRASE_OCCURRENCE_OK;
}

void phrase_occurrence_destroy(PhraseOccurrence *phrase_occurrence) {
    if (phrase_occurrence == NULL) {
        return;
    }
    occurrence_destroy(phrase_occurrence->occurrence);
    for (size_t i = 0; i < phrase_occurrence->region_event_id_count; i++) {
        entity_id_destroy(phrase_occurrence->region_event_ids[i]);
    }
    free(phrase_occurrence->region_event_ids);
    free(phrase_occurrence);
}

const EntityID *phrase_occurrence_id(const PhraseOccurrence *phrase_occurrence) {
    return occurrence_id(phrase_occurrence->occurrence);
}

const EntityID *phrase_occurrence_phrase_id(const PhraseOccurrence *phrase_occurrence) {
    return occurrence_target_id(phrase_occurrence->occurrence);
}

size_t phrase_occurrence_region_event_id_count(const PhraseOccurrence *phrase_occurrence) {
    return phrase_occurrence->region_event_id_count;
}

const EntityID *phrase_occurrence_region_event_id_at(const PhraseOccurrence *phrase_occurrence, size_t index) {
    return phrase_occurrence->region_event_ids[index];
}