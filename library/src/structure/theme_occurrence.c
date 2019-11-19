#include "structure/theme_occurrence.h"

#include <stdlib.h>

struct ThemeOccurrence {
    Occurrence *occurrence;
    EntityID **region_event_ids;
    size_t region_event_id_count;
};

ThemeOccurrenceResult theme_occurrence_create(Occurrence *occurrence, EntityID **region_event_ids,
                                               size_t region_event_id_count, ThemeOccurrence **out) {
    if (occurrence == NULL) {
        return THEME_OCCURRENCE_ERR_NULL_OCCURRENCE;
    }
    if (region_event_ids == NULL || region_event_id_count == 0) {
        return THEME_OCCURRENCE_ERR_NULL_REGION_EVENT_IDS;
    }
    for (size_t i = 0; i < region_event_id_count; i++) {
        if (region_event_ids[i] == NULL) {
            return THEME_OCCURRENCE_ERR_NULL_REGION_EVENT_ID;
        }
    }

    ThemeOccurrence *theme_occurrence = (ThemeOccurrence *)malloc(sizeof(ThemeOccurrence));
    if (theme_occurrence == NULL) {
        return THEME_OCCURRENCE_ERR_ALLOC;
    }

    EntityID **owned_ids = (EntityID **)malloc(sizeof(EntityID *) * region_event_id_count);
    if (owned_ids == NULL) {
        free(theme_occurrence);
        return THEME_OCCURRENCE_ERR_ALLOC;
    }
    for (size_t i = 0; i < region_event_id_count; i++) {
        owned_ids[i] = region_event_ids[i];
    }

    theme_occurrence->occurrence = occurrence;
    theme_occurrence->region_event_ids = owned_ids;
    theme_occurrence->region_event_id_count = region_event_id_count;

    *out = theme_occurrence;
    return THEME_OCCURRENCE_OK;
}

void theme_occurrence_destroy(ThemeOccurrence *theme_occurrence) {
    if (theme_occurrence == NULL) {
        return;
    }
    occurrence_destroy(theme_occurrence->occurrence);
    for (size_t i = 0; i < theme_occurrence->region_event_id_count; i++) {
        entity_id_destroy(theme_occurrence->region_event_ids[i]);
    }
    free(theme_occurrence->region_event_ids);
    free(theme_occurrence);
}

const EntityID *theme_occurrence_id(const ThemeOccurrence *theme_occurrence) {
    return occurrence_id(theme_occurrence->occurrence);
}

const EntityID *theme_occurrence_theme_id(const ThemeOccurrence *theme_occurrence) {
    return occurrence_target_id(theme_occurrence->occurrence);
}

size_t theme_occurrence_region_event_id_count(const ThemeOccurrence *theme_occurrence) {
    return theme_occurrence->region_event_id_count;
}

const EntityID *theme_occurrence_region_event_id_at(const ThemeOccurrence *theme_occurrence, size_t index) {
    return theme_occurrence->region_event_ids[index];
}