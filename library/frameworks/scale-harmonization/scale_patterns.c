#include "scale_patterns.h"

#include <stddef.h>

#define I(n) (1u << (n))

static const ScalePattern PATTERNS[SCALE_PATTERN_COUNT] = {
    [SCALE_PATTERN_PARALLEL_CHORDS] = {SCALE_PATTERN_PARALLEL_CHORDS,
                                       "parallel-chords",
                                       SEQUENCE_KIND_PARALLEL_CHORDS,
                                       SCALE_DIRECTION_ASCENDING,
                                       true,
                                       1,
                                       {{true, I(3) | I(6) | I(8), false}}},
    [SCALE_PATTERN_FIVE_SIX_ASCENDING] = {SCALE_PATTERN_FIVE_SIX_ASCENDING,
                                          "five-six-ascending",
                                          SEQUENCE_KIND_FIVE_SIX,
                                          SCALE_DIRECTION_ASCENDING,
                                          false,
                                          2,
                                          {{true, I(5), false}, {false, I(6), false}}},
    [SCALE_PATTERN_FIVE_SIX_DESCENDING] = {SCALE_PATTERN_FIVE_SIX_DESCENDING,
                                           "five-six-descending",
                                           SEQUENCE_KIND_FIVE_SIX,
                                           SCALE_DIRECTION_DESCENDING,
                                           false,
                                           2,
                                           {{true, I(5), false}, {false, I(6), false}}},
    [SCALE_PATTERN_FIVE_SIX_DESCENDING_ALTERNATING] = {SCALE_PATTERN_FIVE_SIX_DESCENDING_ALTERNATING,
                                                       "five-six-descending-alternating",
                                                       SEQUENCE_KIND_FIVE_SIX,
                                                       SCALE_DIRECTION_DESCENDING,
                                                       false,
                                                       2,
                                                       {{true, I(5), false}, {true, I(6), false}}},
    [SCALE_PATTERN_SEVEN_SIX_DESCENDING] = {SCALE_PATTERN_SEVEN_SIX_DESCENDING,
                                            "seven-six-descending",
                                            SEQUENCE_KIND_SEVEN_SIX,
                                            SCALE_DIRECTION_DESCENDING,
                                            false,
                                            2,
                                            {{true, I(7), true}, {false, I(6), false}}},
};

bool scale_pattern_valid(ScalePatternId id) {
    return id >= 0 && id < SCALE_PATTERN_COUNT;
}

const char *scale_pattern_name(ScalePatternId id) {
    return scale_pattern_valid(id) ? PATTERNS[id].name : NULL;
}

const ScalePattern *scale_pattern(ScalePatternId id) {
    return scale_pattern_valid(id) ? &PATTERNS[id] : NULL;
}

bool scale_pattern_allows_direction(const ScalePattern *pattern, ScaleDirection direction) {
    return pattern != NULL && scale_direction_valid(direction) &&
           (pattern->both_directions || pattern->direction == direction);
}
