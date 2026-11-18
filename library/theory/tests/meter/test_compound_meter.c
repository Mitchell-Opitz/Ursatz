#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "theory/meter/beat_grid.h"
#include "theory/meter/meter_class_rules.h"
#include "theory/tests/meter/meter_test_util.h"

static void test_beat_grid(void) {
    TimeSignature *six_eight = make_signature(6, 3);
    Duration start;
    assert(standard_meter_beat_start(six_eight, 0, &start));
    assert_duration(&start, 0, 1);
    assert(standard_meter_beat_start(six_eight, 1, &start));
    assert_duration(&start, 3, 8);
    assert(standard_meter_beat_start(six_eight, 2, &start)); /* the end of the measure */
    assert_duration(&start, 3, 4);
    start = (Duration){{7, 9}};
    assert(!standard_meter_beat_start(six_eight, 3, &start));
    assert_duration(&start, 7, 9); /* untouched */

    TimeSignature *five_eight = make_signature(5, 3);
    assert(!standard_meter_beat_start(five_eight, 0, &start));
    time_signature_destroy(five_eight);
    time_signature_destroy(six_eight);
}

/* Beats per measure feed beat roles: a 6/8 measure has two beats. */
static void test_beat_roles_in_compound(void) {
    TimeSignature *six_eight = make_signature(6, 3);
    unsigned beats = 0;
    assert(standard_meter_beats_per_measure(six_eight, &beats) && beats == 2);
    time_signature_destroy(six_eight);
}

int main(void) {
    test_beat_grid();
    test_beat_roles_in_compound();
    printf("test_compound_meter (standard-meter): ok\n");
    return 0;
}
