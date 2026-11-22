#include <assert.h>
#include <stdio.h>

#include "counterpoint_test_util.h"

/*
 * The melodic line and the two-part pair of strict counterpoint (counterpoint_line.h): what makes a line or a pair valid, the interval
 * and direction from a note to the next, and the first highest and lowest note of a line.
 */

static void test_valid_line_and_pair(void) {
    const PitchSpelling *up[] = {P(C, 0, 4), P(E, 0, 4), P(G, 0, 4)};
    const PitchSpelling *two[] = {P(C, 0, 3), P(G, 0, 3)};
    const PitchSpelling *with_hole[] = {P(C, 0, 4), NULL, P(G, 0, 4)};
    const StandardCounterpointLine line = LINE_OF(up);
    const StandardCounterpointLine short_line = LINE_OF(two);
    const StandardCounterpointLine holed = LINE_OF(with_hole);
    const StandardCounterpointLine empty = {up, 0};

    assert(standard_counterpoint_line_valid(&line));
    assert(!standard_counterpoint_line_valid(NULL));
    assert(!standard_counterpoint_line_valid(&empty));
    assert(!standard_counterpoint_line_valid(&holed));

    const StandardCounterpointPair pair = {&line, &line};
    assert(standard_counterpoint_pair_valid(&pair));
    assert(!standard_counterpoint_pair_valid(NULL));
    const StandardCounterpointPair different_lengths = {&line, &short_line};
    assert(!standard_counterpoint_pair_valid(&different_lengths));
    const StandardCounterpointPair invalid_lower = {&line, &holed};
    assert(!standard_counterpoint_pair_valid(&invalid_lower));
    const StandardCounterpointPair missing_upper = {NULL, &line};
    assert(!standard_counterpoint_pair_valid(&missing_upper));
}

static void test_intervals_and_directions(void) {
    const PitchSpelling *notes[] = {P(C, 0, 4), P(E, 0, 4), P(G, 0, 4), P(G, 0, 3)};
    const StandardCounterpointLine line = LINE_OF(notes);
    StandardTonalityInterval interval;

    /* C4 E4 is a major third, E4 G4 a minor third; the last index has no next note */
    assert(standard_counterpoint_line_interval_at(&line, 0, &interval));
    assert(interval.size == 3 && interval.quality == STANDARD_TONALITY_INTERVAL_QUALITY_MAJOR);
    assert(standard_counterpoint_line_interval_at(&line, 1, &interval));
    assert(interval.size == 3 && interval.quality == STANDARD_TONALITY_INTERVAL_QUALITY_MINOR);
    interval.size = 99;
    assert(!standard_counterpoint_line_interval_at(&line, 3, &interval) && interval.size == 99);
    assert(!standard_counterpoint_line_interval_at(NULL, 0, &interval));

    assert(standard_counterpoint_line_direction_at(&line, 0) == 1);
    assert(standard_counterpoint_line_direction_at(&line, 2) == -1);
    assert(standard_counterpoint_line_direction_at(&line, 3) == 0); /* no next note */
    assert(standard_counterpoint_line_direction_at(NULL, 0) == 0);

    const PitchSpelling *repeated[] = {P(C, 0, 4), P(E, 0, 4), P(E, 0, 4), P(C, 0, 4)};
    const StandardCounterpointLine repeat_line = LINE_OF(repeated);
    assert(standard_counterpoint_line_direction_at(&repeat_line, 0) == 1);
    assert(standard_counterpoint_line_direction_at(&repeat_line, 1) == 0);
    assert(standard_counterpoint_line_direction_at(&repeat_line, 2) == -1);
}

static void test_highest_and_lowest(void) {
    const PitchSpelling *high[] = {P(C, 0, 4), P(G, 0, 4), P(E, 0, 4), P(G, 0, 4)}; /* the first G is the climax */
    const StandardCounterpointLine high_line = LINE_OF(high);
    assert(standard_counterpoint_line_highest_index(&high_line) == 1);
    assert(standard_counterpoint_line_lowest_index(&high_line) == 0);

    const PitchSpelling *low[] = {P(E, 0, 4), P(C, 0, 4), P(G, 0, 4), P(C, 0, 4)}; /* the first C is the lowest */
    const StandardCounterpointLine low_line = LINE_OF(low);
    assert(standard_counterpoint_line_lowest_index(&low_line) == 1);
    assert(standard_counterpoint_line_highest_index(&low_line) == 2);

    const PitchSpelling *with_hole[] = {P(C, 0, 4), NULL};
    const StandardCounterpointLine invalid = LINE_OF(with_hole);
    assert(standard_counterpoint_line_highest_index(&invalid) == 0);
    assert(standard_counterpoint_line_lowest_index(&invalid) == 0);
    assert(standard_counterpoint_line_highest_index(NULL) == 0);
}

int main(void) {
    test_valid_line_and_pair();
    test_intervals_and_directions();
    test_highest_and_lowest();
    printf("test_counterpoint_line: ok\n");
    return 0;
}
