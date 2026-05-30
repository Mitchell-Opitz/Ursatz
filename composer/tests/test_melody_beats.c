#include <assert.h>
#include <stdio.h>
#include <string.h>

#include <ursatz_composer/constraint.h>

/*
 * Melody, accompaniment and voicing over spans that start and end on any beat, and melody pickups (R-168).
 */

/* A pickup is a multiple of 1/32 of a whole note and shorter than a bar. */
static void test_pickup_constraint(void) {
    PlanConstraint pickup;
    memset(&pickup, 0, sizeof(pickup));
    pickup.kind = PLAN_CONSTRAINT_PICKUP;
    assert(plan_time_make(1, 4, &pickup.params.pickup) == PLAN_OK);
    assert(pickup.params.pickup.num == 1 && pickup.params.pickup.den == 4);
}

int main(void) {
    test_pickup_constraint();
    puts("test_melody_beats ok");
    return 0;
}
