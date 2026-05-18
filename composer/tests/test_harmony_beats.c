#include <assert.h>
#include <stdio.h>
#include <string.h>

#include <ursatz_composer/harmony_phrase.h>

/*
 * Harmony and bass over spans that start and end on any beat (R-168), run through the fill loop with the checker.
 */

/* The fewest cells a cadence fits in, in a meter the operators support. */
static void test_minimum_cells(void) {
    PlanMeter four;
    memset(&four, 0, sizeof(four));
    four.beats_per_bar = 4;
    four.beat_unit = 4;
    assert(harmony_phrase_min_cells(PLAN_CADENCE_PAC, &four) == 3);
    assert(harmony_phrase_min_cells(PLAN_CADENCE_HC, &four) == 2);
}

int main(void) {
    test_minimum_cells();
    puts("test_harmony_beats ok");
    return 0;
}
