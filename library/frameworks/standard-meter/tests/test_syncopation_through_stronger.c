#include <assert.h>
#include <stdio.h>

#include "syncopation_rules.h"

static void test_levels(void) {
    unsigned strength = 0;
    /* an off-beat note held over the beat: one level */
    assert(standard_meter_syncopated_through_stronger(STANDARD_METER_LEVEL_OFFBEAT, STANDARD_METER_LEVEL_BEAT, &strength));
    assert(strength == 1);
    /* a note on a beat held across the barline: two levels (beat to downbeat) */
    assert(standard_meter_syncopated_through_stronger(STANDARD_METER_LEVEL_BEAT, STANDARD_METER_LEVEL_DOWNBEAT, &strength));
    assert(strength == 2);
    /* an off-beat note held across the barline: three */
    assert(standard_meter_syncopated_through_stronger(STANDARD_METER_LEVEL_OFFBEAT, STANDARD_METER_LEVEL_DOWNBEAT, &strength));
    assert(strength == 3);
    assert(standard_meter_syncopated_through_stronger(STANDARD_METER_LEVEL_FINER, STANDARD_METER_LEVEL_DOWNBEAT, &strength));
    assert(strength == 4);
    /* beat 2 of a quadruple meter held through the secondary beat */
    assert(standard_meter_syncopated_through_stronger(STANDARD_METER_LEVEL_BEAT, STANDARD_METER_LEVEL_SECONDARY_BEAT, &strength));
    assert(strength == 1);
}

static void test_not_syncopated(void) {
    unsigned strength = 7;
    /* the same level, a weaker one, or none held through is not syncopation, and the output stays */
    assert(!standard_meter_syncopated_through_stronger(STANDARD_METER_LEVEL_BEAT, STANDARD_METER_LEVEL_BEAT, &strength));
    assert(!standard_meter_syncopated_through_stronger(STANDARD_METER_LEVEL_DOWNBEAT, STANDARD_METER_LEVEL_BEAT, &strength));
    assert(!standard_meter_syncopated_through_stronger(STANDARD_METER_LEVEL_DOWNBEAT, STANDARD_METER_LEVEL_DOWNBEAT, &strength));
    assert(!standard_meter_syncopated_through_stronger(STANDARD_METER_LEVEL_OFFBEAT, STANDARD_METER_LEVEL_FINER, &strength));
    assert(strength == 7);
    assert(!standard_meter_syncopated_through_stronger((StandardMeterLevel)9, STANDARD_METER_LEVEL_BEAT, &strength));
    assert(!standard_meter_syncopated_through_stronger(STANDARD_METER_LEVEL_BEAT, (StandardMeterLevel)-1, &strength));
    assert(standard_meter_syncopated_through_stronger(STANDARD_METER_LEVEL_OFFBEAT, STANDARD_METER_LEVEL_BEAT, NULL));
}

int main(void) {
    test_levels();
    test_not_syncopated();
    printf("test_syncopation_through_stronger (standard-meter): ok\n");
    return 0;
}
