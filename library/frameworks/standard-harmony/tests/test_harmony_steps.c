#include <assert.h>
#include <stdio.h>

#include "harmony_steps.h"

#define NONE HARMONY_PERFECT_NONE
#define FIFTH HARMONY_PERFECT_FIFTH
#define OCTAVE HARMONY_PERFECT_OCTAVE

static void test_degree(void) {
    const int expected[7] = {1, 2, 3, 4, 5, 6, 7};
    for (int step = 0; step < 7; step++) {
        assert(harmony_step_degree(step) == expected[step]);
        assert(harmony_step_degree(step + 7) == expected[step]);
        assert(harmony_step_degree(step - 7) == expected[step]);
        assert(harmony_step_degree(step + 70) == expected[step]);
    }
    assert(harmony_step_degree(-1) == 7); /* ti below do */
    assert(harmony_step_degree(-3) == 5); /* sol below do */
    assert(harmony_step_degree(-14) == 1);
}

static void test_direction(void) {
    assert(harmony_step_direction(0, 1) == 1);
    assert(harmony_step_direction(5, -2) == -1);
    assert(harmony_step_direction(3, 3) == 0);
}

static void test_perfect_interval(void) {
    /* Octaves and unisons, in any order and across octaves. */
    assert(harmony_step_perfect_interval(0, 0) == OCTAVE);
    assert(harmony_step_perfect_interval(0, 7) == OCTAVE);
    assert(harmony_step_perfect_interval(7, -7) == OCTAVE);
    assert(harmony_step_perfect_interval(3, 17) == OCTAVE);
    /* Fifths above the lower voice, whichever voice is given first. */
    assert(harmony_step_perfect_interval(0, 4) == FIFTH);
    assert(harmony_step_perfect_interval(4, 0) == FIFTH);
    assert(harmony_step_perfect_interval(1, 12) == FIFTH); /* re up to la, a twelfth */
    assert(harmony_step_perfect_interval(3, 7) == FIFTH);  /* fa up to do */
    assert(harmony_step_perfect_interval(2, 6) == FIFTH);  /* mi up to ti */
    /* Anything else. */
    assert(harmony_step_perfect_interval(0, 1) == NONE);
    assert(harmony_step_perfect_interval(0, 2) == NONE);
    assert(harmony_step_perfect_interval(0, 3) == NONE); /* a fourth */
    assert(harmony_step_perfect_interval(0, 5) == NONE);
    assert(harmony_step_perfect_interval(0, 6) == NONE);
}

static void test_ti_to_fa_is_not_perfect(void) {
    /* ti with fa above it (a diminished fifth), in any octave. */
    assert(harmony_step_perfect_interval(6, 10) == NONE);
    assert(harmony_step_perfect_interval(-1, 3) == NONE);
    assert(harmony_step_perfect_interval(10, 6) == NONE);
    /* fa with ti above it is a fourth, also not perfect. */
    assert(harmony_step_perfect_interval(3, 6) == NONE);
}

static void test_parallels(void) {
    /* Parallel octaves: both up a step from an octave. */
    assert(harmony_steps_parallel(0, 1, 7, 8));
    /* Parallel fifths: both up a step from a fifth (do-sol to re-la). */
    assert(harmony_steps_parallel(0, 1, 4, 5));
    /* Both down. */
    assert(harmony_steps_parallel(5, 4, 1, 0));
    /* A unison counts as an octave. */
    assert(harmony_steps_parallel(0, 1, 0, 1));
    /* Compound intervals count the same. */
    assert(harmony_steps_parallel(0, 1, 11, 12));
}

static void test_not_parallels(void) {
    /* Contrary motion. */
    assert(!harmony_steps_parallel(0, 1, 7, 6));
    /* Oblique motion: only one voice moves. */
    assert(!harmony_steps_parallel(0, 0, 4, 5));
    assert(!harmony_steps_parallel(0, 1, 7, 7));
    /* Neither voice moves. */
    assert(!harmony_steps_parallel(0, 0, 7, 7));
    /* Similar motion into a perfect interval is hidden, not parallel. */
    assert(!harmony_steps_parallel(0, 1, 3, 5));
    /* A perfect fifth moving to a different kind of interval. */
    assert(!harmony_steps_parallel(0, 1, 4, 6));
    /* A fifth to an octave. */
    assert(!harmony_steps_parallel(0, 2, 4, 7));
    /* Parallel fifths that include a diminished fifth: ti-fa then do-sol. */
    assert(!harmony_steps_parallel(6, 7, 10, 11));
    /* A diminished fifth moving to a perfect one is not parallel either. */
    assert(!harmony_steps_parallel(-1, 0, 3, 4));
    /* Parallel fourths are allowed. */
    assert(!harmony_steps_parallel(0, 1, 3, 4));
}

int main(void) {
    test_degree();
    test_direction();
    test_perfect_interval();
    test_ti_to_fa_is_not_perfect();
    test_parallels();
    test_not_parallels();
    printf("test_harmony_steps: ok\n");
    return 0;
}
