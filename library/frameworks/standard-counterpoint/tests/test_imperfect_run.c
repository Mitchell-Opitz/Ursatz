#include <assert.h>
#include <stdio.h>

#include "harmonic_rules.h"

/* standard_counterpoint_imperfect_run on MIDI numbers: thirds and sixths, three in a row. */

static void test_runs(void) {
    const int lower[] = {60, 62, 64, 65, 67};
    const int thirds[] = {64, 65, 67, 69, 71}; /* 4, 3, 3, 4, 4 */
    const int major_thirds[] = {64, 66, 68, 69, 71}; /* 4, 4, 4, 4, 4 */
    const int sixths[] = {69, 71, 72, 74, 76}; /* 9, 9, 8, 9, 9 */
    assert(standard_counterpoint_imperfect_run(major_thirds, lower, 5, 3));
    assert(standard_counterpoint_imperfect_run(major_thirds, lower, 3, 3));
    assert(!standard_counterpoint_imperfect_run(thirds, lower, 5, 3)); /* 3, 4, 4: not equal */
    assert(standard_counterpoint_imperfect_run(thirds, lower, 5, 2));  /* 4, 4 */
    assert(!standard_counterpoint_imperfect_run(thirds, lower, 5, 5));
    assert(standard_counterpoint_imperfect_run(sixths, lower, 5, 2));  /* 9, 9 */
    assert(!standard_counterpoint_imperfect_run(sixths, lower, 5, 3)); /* 8, 9, 9 */
    assert(standard_counterpoint_imperfect_run(thirds, lower, 5, 1));  /* a single imperfect interval */
}

static void test_not_imperfect(void) {
    const int lower[] = {60, 62, 64};
    const int fifths[] = {67, 69, 71};   /* 7, 7, 7: perfect */
    const int seconds[] = {62, 64, 66};  /* 2, 2, 2 */
    const int unisons[] = {60, 62, 64};
    assert(!standard_counterpoint_imperfect_run(fifths, lower, 3, 3));
    assert(!standard_counterpoint_imperfect_run(seconds, lower, 3, 3));
    assert(!standard_counterpoint_imperfect_run(unisons, lower, 3, 3));
}

static void test_octave_and_order(void) {
    /* Compound thirds and parts that cross read as the distance between them, mod 12. */
    const int lower[] = {48, 50, 52};
    const int upper[] = {64, 66, 68};  /* 16 = a tenth: 4 */
    const int crossed[] = {44, 46, 48}; /* below the lower part by 4 */
    assert(standard_counterpoint_imperfect_run(upper, lower, 3, 3));
    assert(standard_counterpoint_imperfect_run(crossed, lower, 3, 3));
}

static void test_invalid(void) {
    const int a[] = {64, 66, 68};
    const int b[] = {60, 62, 64};
    assert(!standard_counterpoint_imperfect_run(NULL, b, 3, 3));
    assert(!standard_counterpoint_imperfect_run(a, NULL, 3, 3));
    assert(!standard_counterpoint_imperfect_run(a, b, 3, 0));
    assert(!standard_counterpoint_imperfect_run(a, b, 2, 3));
    assert(!standard_counterpoint_imperfect_run(a, b, 0, 3));
}

int main(void) {
    test_runs();
    test_not_imperfect();
    test_octave_and_order();
    test_invalid();
    printf("standard-counterpoint imperfect run tests passed\n");
    return 0;
}
