#include "generation/conflict_detection.h"

#include <stdlib.h>
#include <string.h>

struct ConflictDetection {
    EntityID **conflicting_ids;
    size_t conflicting_id_count;
    char *description;
    unsigned long priority;
    ConflictSeverity severity;
    unsigned long weight_numerator;
    unsigned long weight_denominator;
};

static char *copy_string(const char *value) {
    size_t len = strlen(value);
    char *copy = (char *)malloc(len + 1);
    if (copy != NULL) {
        memcpy(copy, value, len + 1);
    }
    return copy;
}

ConflictDetectionResult conflict_detection_create(EntityID **conflicting_ids, size_t conflicting_id_count,
                                                   const char *description, unsigned long priority,
                                                   ConflictSeverity severity, unsigned long weight_numerator,
                                                   unsigned long weight_denominator, ConflictDetection **out) {
    if (conflicting_ids == NULL || conflicting_id_count < 2) {
        return CONFLICT_DETECTION_ERR_NULL_CONFLICTING_IDS;
    }
    for (size_t i = 0; i < conflicting_id_count; i++) {
        if (conflicting_ids[i] == NULL) {
            return CONFLICT_DETECTION_ERR_NULL_CONFLICTING_ID;
        }
    }
    if (description == NULL || description[0] == '\0') {
        return CONFLICT_DETECTION_ERR_NULL_DESCRIPTION;
    }
    if (weight_denominator == 0) {
        return CONFLICT_DETECTION_ERR_NON_POSITIVE_WEIGHT_DENOMINATOR;
    }

    ConflictDetection *conflict = (ConflictDetection *)malloc(sizeof(ConflictDetection));
    if (conflict == NULL) {
        return CONFLICT_DETECTION_ERR_ALLOC;
    }

    EntityID **owned_ids = (EntityID **)malloc(sizeof(EntityID *) * conflicting_id_count);
    if (owned_ids == NULL) {
        free(conflict);
        return CONFLICT_DETECTION_ERR_ALLOC;
    }
    for (size_t i = 0; i < conflicting_id_count; i++) {
        owned_ids[i] = conflicting_ids[i];
    }

    char *copied_description = copy_string(description);
    if (copied_description == NULL) {
        free(owned_ids);
        free(conflict);
        return CONFLICT_DETECTION_ERR_ALLOC;
    }

    conflict->conflicting_ids = owned_ids;
    conflict->conflicting_id_count = conflicting_id_count;
    conflict->description = copied_description;
    conflict->priority = priority;
    conflict->severity = severity;
    conflict->weight_numerator = weight_numerator;
    conflict->weight_denominator = weight_denominator;

    *out = conflict;
    return CONFLICT_DETECTION_OK;
}

void conflict_detection_destroy(ConflictDetection *conflict) {
    if (conflict == NULL) {
        return;
    }
    for (size_t i = 0; i < conflict->conflicting_id_count; i++) {
        entity_id_destroy(conflict->conflicting_ids[i]);
    }
    free(conflict->conflicting_ids);
    free(conflict->description);
    free(conflict);
}

size_t conflict_detection_conflicting_id_count(const ConflictDetection *conflict) {
    return conflict->conflicting_id_count;
}

const EntityID *conflict_detection_conflicting_id_at(const ConflictDetection *conflict, size_t index) {
    return conflict->conflicting_ids[index];
}

const char *conflict_detection_description(const ConflictDetection *conflict) {
    return conflict->description;
}

unsigned long conflict_detection_priority(const ConflictDetection *conflict) {
    return conflict->priority;
}

ConflictSeverity conflict_detection_severity(const ConflictDetection *conflict) {
    return conflict->severity;
}

unsigned long conflict_detection_weight_numerator(const ConflictDetection *conflict) {
    return conflict->weight_numerator;
}

unsigned long conflict_detection_weight_denominator(const ConflictDetection *conflict) {
    return conflict->weight_denominator;
}