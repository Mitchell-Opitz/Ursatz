#include <assert.h>
#include <stdio.h>

#include "theory/harmony/harmony_steps.h"

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

int main(void) {
    test_degree();
    printf("test_harmony_steps: ok\n");
    return 0;
}
