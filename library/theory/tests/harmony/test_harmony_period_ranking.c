#include <assert.h>
#include <stdbool.h>
#include <stdio.h>

#include "theory/harmony/harmony_cadence_rules.h"
#include "theory/vocabulary/cadence_type.h"

/*
 * The period order of standard-harmony: two cadences in a row make a period when both are ranked (a rank above 0:
 * an unknown, invalid or non-closing cadence is not) and the first is weaker than the second under
 * harmony_cadence_strength.
 */

static bool is_period(CadenceType first, CadenceType second) {
    int first_rank = harmony_cadence_strength(first);
    int second_rank = harmony_cadence_strength(second);
    return first_rank > 0 && second_rank > 0 && first_rank < second_rank;
}

int main(void) {
    /* Weaker then stronger is a period, at every step of the order. */
    assert(is_period(CADENCE_TYPE_HALF, CADENCE_TYPE_DECEPTIVE));
    assert(is_period(CADENCE_TYPE_DECEPTIVE, CADENCE_TYPE_PLAGAL));
    assert(is_period(CADENCE_TYPE_PLAGAL, CADENCE_TYPE_AUTHENTIC_UNSPECIFIED));
    assert(is_period(CADENCE_TYPE_AUTHENTIC_UNSPECIFIED, CADENCE_TYPE_IMPERFECT_AUTHENTIC));
    assert(is_period(CADENCE_TYPE_IMPERFECT_AUTHENTIC, CADENCE_TYPE_PERFECT_AUTHENTIC));
    assert(is_period(CADENCE_TYPE_HALF, CADENCE_TYPE_PERFECT_AUTHENTIC));

    /* Stronger then weaker, and equal strength, are not. */
    assert(!is_period(CADENCE_TYPE_PERFECT_AUTHENTIC, CADENCE_TYPE_HALF));
    assert(!is_period(CADENCE_TYPE_PLAGAL, CADENCE_TYPE_DECEPTIVE));
    assert(!is_period(CADENCE_TYPE_HALF, CADENCE_TYPE_HALF));

    /* Unranked or non-closing cadences are skipped, on either side. */
    assert(!is_period(CADENCE_TYPE_UNKNOWN, CADENCE_TYPE_PERFECT_AUTHENTIC));
    assert(!is_period(CADENCE_TYPE_HALF, CADENCE_TYPE_UNKNOWN));
    assert(!is_period(CADENCE_TYPE_SUBVERTED, CADENCE_TYPE_PERFECT_AUTHENTIC));
    assert(!is_period(CADENCE_TYPE_HALF, CADENCE_TYPE_COUNT));

    printf("test_harmony_period_ranking: ok\n");
    return 0;
}
