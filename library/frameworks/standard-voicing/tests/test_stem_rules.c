#include <assert.h>
#include <stdio.h>

#include "stem_rules.h"

static void test_staff_for_part(void) {
    StandardVoicingStaff staff;
    assert(standard_voicing_staff_for_part(VOICE_PART_SOPRANO, &staff) && staff == STANDARD_VOICING_STAFF_TREBLE);
    assert(standard_voicing_staff_for_part(VOICE_PART_ALTO, &staff) && staff == STANDARD_VOICING_STAFF_TREBLE);
    assert(standard_voicing_staff_for_part(VOICE_PART_TENOR, &staff) && staff == STANDARD_VOICING_STAFF_BASS);
    assert(standard_voicing_staff_for_part(VOICE_PART_BASS, &staff) && staff == STANDARD_VOICING_STAFF_BASS);

    staff = STANDARD_VOICING_STAFF_BASS;
    assert(!standard_voicing_staff_for_part(VOICE_PART_COUNT, &staff));
    assert(staff == STANDARD_VOICING_STAFF_BASS);
    assert(!standard_voicing_staff_for_part(VOICE_PART_SOPRANO, NULL));
}

static void test_stem_direction_for_part(void) {
    StandardVoicingStemDirection direction;
    assert(standard_voicing_stem_direction_for_part(VOICE_PART_SOPRANO, &direction) &&
           direction == STANDARD_VOICING_STEM_UP);
    assert(standard_voicing_stem_direction_for_part(VOICE_PART_ALTO, &direction) &&
           direction == STANDARD_VOICING_STEM_DOWN);
    assert(standard_voicing_stem_direction_for_part(VOICE_PART_TENOR, &direction) &&
           direction == STANDARD_VOICING_STEM_UP);
    assert(standard_voicing_stem_direction_for_part(VOICE_PART_BASS, &direction) &&
           direction == STANDARD_VOICING_STEM_DOWN);

    assert(!standard_voicing_stem_direction_for_part(VOICE_PART_COUNT, &direction));
    assert(!standard_voicing_stem_direction_for_part(VOICE_PART_ALTO, NULL));
}

static void test_direction_valid(void) {
    assert(standard_voicing_stem_direction_valid(VOICE_PART_SOPRANO, STANDARD_VOICING_STEM_UP));
    assert(!standard_voicing_stem_direction_valid(VOICE_PART_SOPRANO, STANDARD_VOICING_STEM_DOWN));
    assert(standard_voicing_stem_direction_valid(VOICE_PART_ALTO, STANDARD_VOICING_STEM_DOWN));
    assert(!standard_voicing_stem_direction_valid(VOICE_PART_ALTO, STANDARD_VOICING_STEM_UP));
    assert(!standard_voicing_stem_direction_valid(VOICE_PART_COUNT, STANDARD_VOICING_STEM_UP));
    assert(!standard_voicing_stem_direction_valid(VOICE_PART_BASS, (StandardVoicingStemDirection)9));
}

/* Example 1: a chord with crossed stem directions, then the corrected one. */
static void test_chord_stems(void) {
    const StandardVoicingStemDirection correct[VOICE_PART_COUNT] = {
        STANDARD_VOICING_STEM_UP, STANDARD_VOICING_STEM_DOWN, STANDARD_VOICING_STEM_UP,
        STANDARD_VOICING_STEM_DOWN};
    assert(standard_voicing_stems_valid(correct));

    /* The alto and tenor stems are swapped. */
    const StandardVoicingStemDirection crossed[VOICE_PART_COUNT] = {
        STANDARD_VOICING_STEM_UP, STANDARD_VOICING_STEM_UP, STANDARD_VOICING_STEM_DOWN,
        STANDARD_VOICING_STEM_DOWN};
    assert(!standard_voicing_stems_valid(crossed));

    assert(!standard_voicing_stems_valid(NULL));
}

int main(void) {
    test_staff_for_part();
    test_stem_direction_for_part();
    test_direction_valid();
    test_chord_stems();
    printf("test_stem_rules (standard-voicing): ok\n");
    return 0;
}
