#ifndef MUSIC_SEMANTICS_RHYTHMIC_PATTERN_H
#define MUSIC_SEMANTICS_RHYTHMIC_PATTERN_H

#include <stddef.h>

#include "time/duration.h"

/*
 * RhythmicPattern (Value/Structural): an independently representable
 * rhythm sequence -- an ordered sequence of Durations
 * (time/duration.h).
 *
 * A rhythmic query MUST NOT require pitch information: this header
 * takes no dependency on pitch/pitch_identity.h or
 * pitch/pitch_spelling.h, and no field here carries pitch data. A
 * RhythmicPattern is fully constructible and queryable from rhythm
 * alone.
 */

typedef struct RhythmicPattern RhythmicPattern;

typedef enum {
    RHYTHMIC_PATTERN_OK = 0,
    RHYTHMIC_PATTERN_ERR_NULL_DURATIONS,
    RHYTHMIC_PATTERN_ERR_NULL_DURATION,
    RHYTHMIC_PATTERN_ERR_ALLOC
} RhythmicPatternResult;

/*
 * Takes ownership of durations (and each Duration it points to) on
 * success -- all destroyed by rhythmic_pattern_destroy. durations must
 * be non-NULL with duration_count > 0; no entry may be NULL.
 */
RhythmicPatternResult rhythmic_pattern_create(Duration **durations, size_t duration_count,
                                               RhythmicPattern **out);
void rhythmic_pattern_destroy(RhythmicPattern *pattern);

size_t rhythmic_pattern_duration_count(const RhythmicPattern *pattern);
const Duration *rhythmic_pattern_duration_at(const RhythmicPattern *pattern, size_t index);

#endif