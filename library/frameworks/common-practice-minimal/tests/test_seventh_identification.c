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

/*
 * Regression: two distinct Sonority occurrences of the same seventh-chord
 * quality must produce two distinct, independently-persisted
 * Interpretation ids (see chord_vocabulary_seventh.c history -- the id
 * used to be derived only from the quality type, so every repeated
 * occurrence collided on one id).
 */
static void check_distinct_occurrences_get_distinct_ids(const int *intervals, size_t interval_count) {
    EntityID *sonority_id_a;
    assert(entity_id_create("sonority-occurrence-a", &sonority_id_a) == ENTITY_ID_OK);
    EntityID *sonority_id_b;
    assert(entity_id_create("sonority-occurrence-b", &sonority_id_b) == ENTITY_ID_OK);

    Chord *chord_a;
    assert(common_practice_identify_seventh_chord(intervals, interval_count, sonority_id_a, &chord_a));
    Chord *chord_b;
    assert(common_practice_identify_seventh_chord(intervals, interval_count, sonority_id_b, &chord_b));

    const EntityID *interpretation_id_a = interpretation_id(chord_interpretation(chord_a));
    const EntityID *interpretation_id_b = interpretation_id(chord_interpretation(chord_b));
    assert(!entity_id_equals(interpretation_id_a, interpretation_id_b));

    chord_destroy(chord_a);
    chord_destroy(chord_b);
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

    /* Subset match: a dominant7 with one extra (non-chord-tone) interval
     * still resolves to dominant7 -- the stopgap this branch adds for
     * arpeggiated/multi-voice textures (see seventh_identification.h). */
    int dominant7_plus_passing_tone[] = {0, 4, 7, 10, 2};
    CommonPracticeSeventh subset_seventh;
    assert(common_practice_identify_seventh(dominant7_plus_passing_tone, 5, &subset_seventh));
    assert(strcmp(common_practice_seventh_label(subset_seventh), "dominant7") == 0);

    /* Ambiguous subset match: {0,4,7,10,11} satisfies both dominant7
     * {0,4,7,10} and major7 {0,4,7,11}. Documented scan-order tie-break
     * picks whichever comes first in SEVENTH_INTERVALS (dominant7), and
     * only logs the ambiguity to stderr -- not surfaced through the
     * return value (known limitation). */
    int ambiguous[] = {0, 4, 7, 10, 11};
    CommonPracticeSeventh ambiguous_seventh;
    assert(common_practice_identify_seventh(ambiguous, 5, &ambiguous_seventh));
    assert(strcmp(common_practice_seventh_label(ambiguous_seventh), "dominant7") == 0);

    check_distinct_occurrences_get_distinct_ids(dominant7, 4);

    printf("test_seventh_identification: ok\n");
    return 0;
}
