#include <assert.h>
#include <stdio.h>

#include "second_species_exercise.h"
#include "second_species_test_util.h"

static void test_valid_exercise(void) {
    const PitchSpelling *cantus[] = {P(D, 0, 3), P(F, 0, 3)};
    const StandardCounterpointSlot slots[] = {ATK(P(A, 0, 3)), ATK(P(D, 0, 4)), ATK(P(F, 0, 4)), TIE(P(F, 0, 4))};
    EXERCISE(exercise, cantus, slots, CANTUS_LOWER);
    assert(standard_counterpoint_second_species_exercise_valid(&exercise));
    assert(standard_counterpoint_second_species_bar_count(&exercise) == 2);
}

static void test_valid_with_half_rest(void) {
    const PitchSpelling *cantus[] = {P(D, 0, 3), P(F, 0, 3)};
    const StandardCounterpointSlot slots[] = {REST(), ATK(P(A, 0, 3)), ATK(P(F, 0, 4)), TIE(P(F, 0, 4))};
    EXERCISE(exercise, cantus, slots, CANTUS_LOWER);
    assert(standard_counterpoint_second_species_exercise_valid(&exercise));
}

static void test_invalid_shapes(void) {
    const PitchSpelling *cantus[] = {P(D, 0, 3), P(F, 0, 3)};
    const PitchSpelling *one_note[] = {P(D, 0, 3)};

    const StandardCounterpointSlot three[] = {ATK(P(A, 0, 3)), ATK(P(D, 0, 4)), ATK(P(F, 0, 4))};
    EXERCISE(wrong_count, cantus, three, CANTUS_LOWER);
    assert(!standard_counterpoint_second_species_exercise_valid(&wrong_count));

    /* A tie on a strong beat belongs to syncopation, a later species. */
    const StandardCounterpointSlot strong_tie[] = {ATK(P(A, 0, 3)), TIE(P(A, 0, 3)), TIE(P(A, 0, 3)), ATK(P(F, 0, 4))};
    EXERCISE(tie_on_strong, cantus, strong_tie, CANTUS_LOWER);
    assert(!standard_counterpoint_second_species_exercise_valid(&tie_on_strong));

    const StandardCounterpointSlot other_pitch[] = {ATK(P(A, 0, 3)), TIE(P(B, 0, 3)), ATK(P(F, 0, 4)), TIE(P(F, 0, 4))};
    EXERCISE(tie_to_other_pitch, cantus, other_pitch, CANTUS_LOWER);
    assert(!standard_counterpoint_second_species_exercise_valid(&tie_to_other_pitch));

    const StandardCounterpointSlot late_rest[] = {ATK(P(A, 0, 3)), ATK(P(D, 0, 4)), REST(), ATK(P(F, 0, 4))};
    EXERCISE(rest_after_start, cantus, late_rest, CANTUS_LOWER);
    assert(!standard_counterpoint_second_species_exercise_valid(&rest_after_start));

    const StandardCounterpointSlot first_tie[] = {TIE(P(A, 0, 3)), ATK(P(D, 0, 4)), ATK(P(F, 0, 4)), TIE(P(F, 0, 4))};
    EXERCISE(tie_first, cantus, first_tie, CANTUS_LOWER);
    assert(!standard_counterpoint_second_species_exercise_valid(&tie_first));

    const StandardCounterpointSlot tie_after_rest[] = {REST(), TIE(P(A, 0, 3)), ATK(P(F, 0, 4)), TIE(P(F, 0, 4))};
    EXERCISE(tie_after_rest_case, cantus, tie_after_rest, CANTUS_LOWER);
    assert(!standard_counterpoint_second_species_exercise_valid(&tie_after_rest_case));

    const StandardCounterpointSlot null_pitch[] = {{STANDARD_COUNTERPOINT_SLOT_ATTACK, NULL}, ATK(P(D, 0, 4)),
                                                   ATK(P(F, 0, 4)), TIE(P(F, 0, 4))};
    EXERCISE(attack_without_pitch, cantus, null_pitch, CANTUS_LOWER);
    assert(!standard_counterpoint_second_species_exercise_valid(&attack_without_pitch));

    const StandardCounterpointSlot slots[] = {ATK(P(A, 0, 3)), ATK(P(D, 0, 4)), ATK(P(F, 0, 4)), TIE(P(F, 0, 4))};
    EXERCISE(valid, cantus, slots, CANTUS_LOWER);
    valid.cantus_part = (StandardCounterpointCantusPart)5;
    assert(!standard_counterpoint_second_species_exercise_valid(&valid));
    valid.cantus_part = CANTUS_LOWER;
    assert(standard_counterpoint_second_species_exercise_valid(&valid));

    StandardCounterpointLine short_cantus = {one_note, 1};
    valid.cantus = &short_cantus;
    assert(!standard_counterpoint_second_species_exercise_valid(&valid));
    assert(!standard_counterpoint_second_species_exercise_valid(NULL));
}

static void test_slot_strength(void) {
    assert(standard_counterpoint_second_species_slot_strength(0) == BEAT_STRENGTH_STRONG);
    assert(standard_counterpoint_second_species_slot_strength(1) == BEAT_STRENGTH_WEAK);
    assert(standard_counterpoint_second_species_slot_strength(2) == BEAT_STRENGTH_STRONG);
    assert(standard_counterpoint_second_species_slot_strength(7) == BEAT_STRENGTH_WEAK);
}

static void test_accessors(void) {
    const PitchSpelling *cantus[] = {P(D, 0, 3), P(F, 0, 3)};
    const StandardCounterpointSlot slots[] = {ATK(P(A, 0, 3)), ATK(P(D, 0, 4)), ATK(P(F, 0, 4)), TIE(P(F, 0, 4))};
    EXERCISE(exercise, cantus, slots, CANTUS_LOWER);

    assert(standard_counterpoint_second_species_cantus(&exercise, 1) == cantus[1]);
    assert(standard_counterpoint_second_species_cantus(&exercise, 2) == NULL);
    assert(pitch_spelling_equals(standard_counterpoint_second_species_downbeat(&exercise, 0), P(A, 0, 3)));
    assert(pitch_spelling_equals(standard_counterpoint_second_species_weak(&exercise, 0), P(D, 0, 4)));
    assert(standard_counterpoint_second_species_downbeat(&exercise, 2) == NULL);

    assert(standard_counterpoint_second_species_weak_is_attack(&exercise, 0));
    /* A tie sounds the pitch but is not a new note. */
    assert(!standard_counterpoint_second_species_weak_is_attack(&exercise, 1));
    assert(pitch_spelling_equals(standard_counterpoint_second_species_weak(&exercise, 1), P(F, 0, 4)));

    StandardCounterpointInterval interval;
    assert(standard_counterpoint_second_species_downbeat_interval(&exercise, 0, &interval));
    assert(interval.size == 5 && interval.quality == STANDARD_COUNTERPOINT_QUALITY_PERFECT);
    assert(standard_counterpoint_second_species_weak_interval(&exercise, 0, &interval));
    assert(interval.size == 8 && interval.quality == STANDARD_COUNTERPOINT_QUALITY_PERFECT);
    assert(!standard_counterpoint_second_species_downbeat_interval(&exercise, 2, &interval));
}

static void test_rest_has_no_downbeat(void) {
    const PitchSpelling *cantus[] = {P(D, 0, 3), P(F, 0, 3)};
    const StandardCounterpointSlot slots[] = {REST(), ATK(P(A, 0, 3)), ATK(P(F, 0, 4)), TIE(P(F, 0, 4))};
    EXERCISE(exercise, cantus, slots, CANTUS_LOWER);
    StandardCounterpointInterval interval;
    assert(standard_counterpoint_second_species_downbeat(&exercise, 0) == NULL);
    assert(!standard_counterpoint_second_species_downbeat_interval(&exercise, 0, &interval));
    assert(pitch_spelling_equals(standard_counterpoint_second_species_weak(&exercise, 0), P(A, 0, 3)));
    assert(standard_counterpoint_second_species_weak_is_attack(&exercise, 0));
}

static void test_attacks(void) {
    const PitchSpelling *cantus[] = {P(D, 0, 3), P(F, 0, 3)};
    const StandardCounterpointSlot slots[] = {REST(), ATK(P(A, 0, 3)), ATK(P(F, 0, 4)), TIE(P(F, 0, 4))};
    EXERCISE(exercise, cantus, slots, CANTUS_LOWER);
    const PitchSpelling *notes[4];
    size_t positions[4];
    size_t count = 0;

    assert(standard_counterpoint_second_species_attacks(&exercise, notes, positions, 4, &count));
    assert(count == 2);
    assert(pitch_spelling_equals(notes[0], P(A, 0, 3)) && positions[0] == 1);
    assert(pitch_spelling_equals(notes[1], P(F, 0, 4)) && positions[1] == 2);

    count = 99;
    assert(!standard_counterpoint_second_species_attacks(&exercise, notes, positions, 1, &count));
    assert(count == 99);
    assert(!standard_counterpoint_second_species_attacks(NULL, notes, positions, 4, &count));
}

int main(void) {
    test_valid_exercise();
    test_valid_with_half_rest();
    test_invalid_shapes();
    test_slot_strength();
    test_accessors();
    test_rest_has_no_downbeat();
    test_attacks();
    printf("test_second_species_exercise (standard-counterpoint): ok\n");
    return 0;
}
