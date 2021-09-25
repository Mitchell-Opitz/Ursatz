#include <assert.h>
#include <stdio.h>

#include "third_species_exercise.h"
#include "third_species_test_util.h"

static void test_valid_exercise(void) {
    const PitchSpelling *cantus[] = {P(D, 0, 3), P(F, 0, 3)};
    const StandardCounterpointSlot slots[] = {ATK(P(A, 0, 3)), ATK(P(B, 0, 3)), ATK(P(C, 0, 4)), ATK(P(B, 0, 3)),
                                              WHOLE(P(A, 0, 3))};
    EXERCISE(exercise, cantus, slots, CANTUS_LOWER);
    assert(standard_counterpoint_third_species_exercise_valid(&exercise));
    assert(standard_counterpoint_third_species_bar_count(&exercise) == 2);
}

static void test_valid_with_quarter_rest(void) {
    const PitchSpelling *cantus[] = {P(D, 0, 3), P(F, 0, 3)};
    const StandardCounterpointSlot slots[] = {REST(), ATK(P(A, 0, 3)), ATK(P(B, 0, 3)), ATK(P(C, 0, 4)),
                                              WHOLE(P(A, 0, 3))};
    EXERCISE(exercise, cantus, slots, CANTUS_LOWER);
    assert(standard_counterpoint_third_species_exercise_valid(&exercise));
}

static void test_invalid_shapes(void) {
    const PitchSpelling *cantus[] = {P(D, 0, 3), P(F, 0, 3)};
    const PitchSpelling *one_note[] = {P(D, 0, 3)};

    const StandardCounterpointSlot seven[] = {ATK(P(A, 0, 3)), ATK(P(B, 0, 3)), ATK(P(C, 0, 4)), ATK(P(B, 0, 3)),
                                              ATK(P(A, 0, 3)), ATK(P(G, 0, 3)), ATK(P(A, 0, 3))};
    EXERCISE(wrong_count, cantus, seven, CANTUS_LOWER);
    assert(!standard_counterpoint_third_species_exercise_valid(&wrong_count));

    /* A half note then two quarters is not third-species rhythm. */
    const StandardCounterpointSlot half[] = {ATK(P(A, 0, 3)), TIE(P(A, 0, 3)), ATK(P(B, 0, 3)), ATK(P(A, 0, 3)),
                                             WHOLE(P(A, 0, 3))};
    EXERCISE(half_note, cantus, half, CANTUS_LOWER);
    assert(!standard_counterpoint_third_species_exercise_valid(&half_note));

    const StandardCounterpointSlot half_and_half[] = {ATK(P(A, 0, 3)), TIE(P(A, 0, 3)), ATK(P(B, 0, 3)),
                                                      TIE(P(B, 0, 3)), WHOLE(P(A, 0, 3))};
    EXERCISE(two_halves, cantus, half_and_half, CANTUS_LOWER);
    assert(!standard_counterpoint_third_species_exercise_valid(&two_halves));

    const StandardCounterpointSlot other_pitch[] = {ATK(P(A, 0, 3)), TIE(P(B, 0, 3)), TIE(P(B, 0, 3)), TIE(P(B, 0, 3)),
                                                    WHOLE(P(A, 0, 3))};
    EXERCISE(tie_to_other_pitch, cantus, other_pitch, CANTUS_LOWER);
    assert(!standard_counterpoint_third_species_exercise_valid(&tie_to_other_pitch));

    const StandardCounterpointSlot late_rest[] = {ATK(P(A, 0, 3)), ATK(P(B, 0, 3)), ATK(P(C, 0, 4)), ATK(P(B, 0, 3)),
                                                  REST(), ATK(P(A, 0, 3)), ATK(P(G, 0, 3)), ATK(P(A, 0, 3))};
    EXERCISE(rest_in_second_bar, cantus, late_rest, CANTUS_LOWER);
    assert(!standard_counterpoint_third_species_exercise_valid(&rest_in_second_bar));

    const StandardCounterpointSlot rest_and_tie[] = {REST(), TIE(P(A, 0, 3)), ATK(P(B, 0, 3)), ATK(P(C, 0, 4)),
                                                     WHOLE(P(A, 0, 3))};
    EXERCISE(tie_after_rest, cantus, rest_and_tie, CANTUS_LOWER);
    assert(!standard_counterpoint_third_species_exercise_valid(&tie_after_rest));

    const StandardCounterpointSlot null_pitch[] = {{STANDARD_COUNTERPOINT_SLOT_ATTACK, NULL}, ATK(P(B, 0, 3)),
                                                   ATK(P(C, 0, 4)), ATK(P(B, 0, 3)), WHOLE(P(A, 0, 3))};
    EXERCISE(attack_without_pitch, cantus, null_pitch, CANTUS_LOWER);
    assert(!standard_counterpoint_third_species_exercise_valid(&attack_without_pitch));

    const StandardCounterpointSlot slots[] = {ATK(P(A, 0, 3)), ATK(P(B, 0, 3)), ATK(P(C, 0, 4)), ATK(P(B, 0, 3)),
                                              WHOLE(P(A, 0, 3))};
    EXERCISE(valid, cantus, slots, CANTUS_LOWER);
    valid.cantus_part = (StandardCounterpointCantusPart)5;
    assert(!standard_counterpoint_third_species_exercise_valid(&valid));
    valid.cantus_part = CANTUS_LOWER;
    assert(standard_counterpoint_third_species_exercise_valid(&valid));

    StandardCounterpointLine short_cantus = {one_note, 1};
    valid.cantus = &short_cantus;
    assert(!standard_counterpoint_third_species_exercise_valid(&valid));
    assert(!standard_counterpoint_third_species_exercise_valid(NULL));
}

static void test_slot_strength(void) {
    assert(standard_counterpoint_third_species_slot_strength(0) == BEAT_STRENGTH_STRONG);
    assert(standard_counterpoint_third_species_slot_strength(1) == BEAT_STRENGTH_WEAK);
    assert(standard_counterpoint_third_species_slot_strength(2) == BEAT_STRENGTH_MEDIUM);
    assert(standard_counterpoint_third_species_slot_strength(3) == BEAT_STRENGTH_WEAK);
    assert(standard_counterpoint_third_species_slot_strength(4) == BEAT_STRENGTH_STRONG);
    assert(standard_counterpoint_third_species_slot_strength(6) == BEAT_STRENGTH_MEDIUM);
}

static void test_accessors(void) {
    const PitchSpelling *cantus[] = {P(D, 0, 3), P(F, 0, 3)};
    const StandardCounterpointSlot slots[] = {ATK(P(A, 0, 3)), ATK(P(B, 0, 3)), ATK(P(C, 0, 4)), ATK(P(B, 0, 3)),
                                              WHOLE(P(A, 0, 4))};
    EXERCISE(exercise, cantus, slots, CANTUS_LOWER);

    assert(standard_counterpoint_third_species_cantus(&exercise, 1) == cantus[1]);
    assert(standard_counterpoint_third_species_cantus(&exercise, 2) == NULL);
    assert(pitch_spelling_equals(standard_counterpoint_third_species_beat(&exercise, 0, 2), P(C, 0, 4)));
    assert(standard_counterpoint_third_species_beat(&exercise, 0, 4) == NULL);
    assert(standard_counterpoint_third_species_beat(&exercise, 2, 0) == NULL);

    assert(standard_counterpoint_third_species_beat_is_attack(&exercise, 0, 3));
    /* A tie sounds the pitch but is not a new note. */
    assert(!standard_counterpoint_third_species_beat_is_attack(&exercise, 1, 2));
    assert(pitch_spelling_equals(standard_counterpoint_third_species_beat(&exercise, 1, 2), P(A, 0, 4)));

    StandardCounterpointInterval interval;
    assert(standard_counterpoint_third_species_beat_interval(&exercise, 0, 0, &interval));
    assert(interval.size == 5 && interval.quality == STANDARD_COUNTERPOINT_QUALITY_PERFECT);
    assert(standard_counterpoint_third_species_beat_interval(&exercise, 1, 3, &interval));
    assert(interval.size == 10 && interval.simple_size == 3);
    assert(!standard_counterpoint_third_species_beat_interval(&exercise, 2, 0, &interval));
}

static void test_rest_has_no_pitch(void) {
    const PitchSpelling *cantus[] = {P(D, 0, 3), P(F, 0, 3)};
    const StandardCounterpointSlot slots[] = {REST(), ATK(P(A, 0, 3)), ATK(P(B, 0, 3)), ATK(P(C, 0, 4)),
                                              WHOLE(P(A, 0, 3))};
    EXERCISE(exercise, cantus, slots, CANTUS_LOWER);
    StandardCounterpointInterval interval;
    assert(standard_counterpoint_third_species_beat(&exercise, 0, 0) == NULL);
    assert(!standard_counterpoint_third_species_beat_is_attack(&exercise, 0, 0));
    assert(!standard_counterpoint_third_species_beat_interval(&exercise, 0, 0, &interval));
    assert(pitch_spelling_equals(standard_counterpoint_third_species_beat(&exercise, 0, 1), P(A, 0, 3)));
}

static void test_attacks(void) {
    const PitchSpelling *cantus[] = {P(D, 0, 3), P(F, 0, 3)};
    const StandardCounterpointSlot slots[] = {REST(), ATK(P(A, 0, 3)), ATK(P(B, 0, 3)), ATK(P(C, 0, 4)),
                                              WHOLE(P(A, 0, 4))};
    EXERCISE(exercise, cantus, slots, CANTUS_LOWER);
    const PitchSpelling *notes[8];
    size_t positions[8];
    size_t count = 0;

    assert(standard_counterpoint_third_species_attacks(&exercise, notes, positions, 8, &count));
    assert(count == 4);
    assert(pitch_spelling_equals(notes[0], P(A, 0, 3)) && positions[0] == 1);
    assert(pitch_spelling_equals(notes[3], P(A, 0, 4)) && positions[3] == 4);

    count = 99;
    assert(!standard_counterpoint_third_species_attacks(&exercise, notes, positions, 2, &count));
    assert(count == 99);
    assert(!standard_counterpoint_third_species_attacks(NULL, notes, positions, 8, &count));
}

int main(void) {
    test_valid_exercise();
    test_valid_with_quarter_rest();
    test_invalid_shapes();
    test_slot_strength();
    test_accessors();
    test_rest_has_no_pitch();
    test_attacks();
    printf("test_third_species_exercise (standard-counterpoint): ok\n");
    return 0;
}
