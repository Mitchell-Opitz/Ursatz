#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "identity/entity_id.h"
#include "interpretation/chord.h"
#include "seventh_identification.h"

/*
 * Identification tests: one per diatonic seventh-chord quality
 * (dominant7, major7, minor7, half-diminished7), confirming correct
 * chord identification given known root-relative semitone intervals
 * (see seventh_identification.h for why intervals rather than scale
 * degrees). Each also checks a reordered input resolves to the same
 * quality label, exercising the documented tie-break rule.
 */

static void check_identification(const int *intervals, size_t interval_count, const char *expected_label,
                                  const char *sonority_id_value) {
    CommonPracticeSeventh seventh;
    assert(common_practice_identify_seventh(intervals, interval_count, &seventh));
    assert(strcmp(common_practice_seventh_label(seventh), expected_label) == 0);

    EntityID *sonority_id;
    assert(entity_id_create(sonority_id_value, &sonority_id) == ENTITY_ID_OK);

    Chord *chord;
    assert(common_practice_identify_seventh_chord(intervals, interval_count, sonority_id, &chord));
    assert(strcmp(chord_label(chord), expected_label) == 0);
    chord_destroy(chord);
}

int main(void) {
    int dominant7[] = {0, 4, 7, 10};
    check_identification(dominant7, 4, "dominant7", "sonority-identify-dominant7");

    int major7[] = {0, 4, 7, 11};
    check_identification(major7, 4, "major7", "sonority-identify-major7");

    int minor7[] = {0, 3, 7, 10};
    check_identification(minor7, 4, "minor7", "sonority-identify-minor7");

    int half_diminished7[] = {0, 3, 6, 10};
    check_identification(half_diminished7, 4, "half-diminished7", "sonority-identify-half-diminished7");

    /* Tie-break rule: reordering the intervals does not change the label. */
    int dominant7_reordered[] = {10, 0, 7, 4};
    CommonPracticeSeventh reordered_seventh;
    assert(common_practice_identify_seventh(dominant7_reordered, 4, &reordered_seventh));
    assert(strcmp(common_practice_seventh_label(reordered_seventh), "dominant7") == 0);

    /* No match: fewer than 4 distinct intervals. */
    int incomplete[] = {0, 4, 7};
    CommonPracticeSeventh unused_seventh;
    assert(!common_practice_identify_seventh(incomplete, 3, &unused_seventh));

    /* No match: fully-diminished7 is not a diatonic quality this framework knows. */
    int fully_diminished7[] = {0, 3, 6, 9};
    assert(!common_practice_identify_seventh(fully_diminished7, 4, &unused_seventh));

    printf("test_seventh_identification: ok\n");
    return 0;
}
