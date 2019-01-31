#include <assert.h>
#include <stdio.h>

#include "semantics/rhythmic_pattern.h"
#include "time/duration.h"

/*
 * Unit tests: construction and basic sequence access.
 */

int main(void) {
    Duration *quarter;
    assert(duration_create(1, 4, &quarter) == DURATION_OK);
    Duration *quarter_2;
    assert(duration_create(1, 4, &quarter_2) == DURATION_OK);
    Duration *half;
    assert(duration_create(1, 2, &half) == DURATION_OK);

    Duration *durations[3] = {quarter, quarter_2, half};

    RhythmicPattern *pattern;
    RhythmicPatternResult result = rhythmic_pattern_create(durations, 3, &pattern);
    assert(result == RHYTHMIC_PATTERN_OK);

    assert(rhythmic_pattern_duration_count(pattern) == 3);
    assert(duration_equals(rhythmic_pattern_duration_at(pattern, 0), rhythmic_pattern_duration_at(pattern, 1)));
    assert(duration_numerator(rhythmic_pattern_duration_at(pattern, 2)) == 1);
    assert(duration_denominator(rhythmic_pattern_duration_at(pattern, 2)) == 2);

    rhythmic_pattern_destroy(pattern);

    /* Empty sequence is rejected. */
    RhythmicPattern *empty;
    assert(rhythmic_pattern_create(NULL, 0, &empty) == RHYTHMIC_PATTERN_ERR_NULL_DURATIONS);

    /* A NULL entry within an otherwise non-empty sequence is rejected. */
    Duration *eighth;
    assert(duration_create(1, 8, &eighth) == DURATION_OK);
    Duration *with_null[2] = {eighth, NULL};
    RhythmicPattern *rejected;
    assert(rhythmic_pattern_create(with_null, 2, &rejected) == RHYTHMIC_PATTERN_ERR_NULL_DURATION);
    duration_destroy(eighth);

    printf("test_rhythmic_pattern: ok\n");
    return 0;
}