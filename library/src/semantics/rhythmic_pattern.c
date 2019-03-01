#include "semantics/rhythmic_pattern.h"

#include <stdlib.h>

struct RhythmicPattern {
    Duration **durations;
    size_t duration_count;
};

RhythmicPatternResult rhythmic_pattern_create(Duration **durations, size_t duration_count,
                                               RhythmicPattern **out) {
    if (durations == NULL || duration_count == 0) {
        return RHYTHMIC_PATTERN_ERR_NULL_DURATIONS;
    }
    for (size_t i = 0; i < duration_count; i++) {
        if (durations[i] == NULL) {
            return RHYTHMIC_PATTERN_ERR_NULL_DURATION;
        }
    }

    RhythmicPattern *pattern = (RhythmicPattern *)malloc(sizeof(RhythmicPattern));
    if (pattern == NULL) {
        return RHYTHMIC_PATTERN_ERR_ALLOC;
    }

    Duration **owned_durations = (Duration **)malloc(sizeof(Duration *) * duration_count);
    if (owned_durations == NULL) {
        free(pattern);
        return RHYTHMIC_PATTERN_ERR_ALLOC;
    }
    for (size_t i = 0; i < duration_count; i++) {
        owned_durations[i] = durations[i];
    }

    pattern->durations = owned_durations;
    pattern->duration_count = duration_count;

    *out = pattern;
    return RHYTHMIC_PATTERN_OK;
}

void rhythmic_pattern_destroy(RhythmicPattern *pattern) {
    if (pattern == NULL) {
        return;
    }
    for (size_t i = 0; i < pattern->duration_count; i++) {
        duration_destroy(pattern->durations[i]);
    }
    free(pattern->durations);
    free(pattern);
}

size_t rhythmic_pattern_duration_count(const RhythmicPattern *pattern) {
    return pattern->duration_count;
}

const Duration *rhythmic_pattern_duration_at(const RhythmicPattern *pattern, size_t index) {
    return pattern->durations[index];
}