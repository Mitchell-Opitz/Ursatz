#include <assert.h>
#include <stdio.h>

#include "fourth_species_exercise.h"
#include "fourth_species_test_util.h"

static void test_valid_exercise(void) {
    const PitchSpelling *cantus[] = {P(D, 0, 3), P(F, 0, 3), P(E, 0, 3)};
    const StandardCounterpointSlot slots[] = {REST(), ATK(P(A, 0, 3)), TIE(P(A, 0, 3)), ATK(P(G, 0, 3)),
                                              WHOLE(P(B, 0, 3))};
    EXERCISE(exercise, cantus, slots, CANTUS_LOWER);
    assert(standard_counterpoint_fourth_species_exercise_valid(&exercise));
    assert(standard_counterpoint_fourth_species_bar_count(&exercise) == 3);

    GOOD_FOURTH_SPECIES_CANTUS(good_cantus);
    GOOD_FOURTH_SPECIES_SLOTS(good_slots);
    EXERCISE(good, good_cantus, good_slots, CANTUS_LOWER);
    assert(standard_counterpoint_fourth_species_exercise_valid(&good));
    assert(standard_counterpoint_fourth_species_bar_count(&good) == 11);
}

static void test_invalid_shapes(void) {
    const PitchSpelling *cantus[] = {P(D, 0, 3), P(F, 0, 3), P(E, 0, 3)};
    const PitchSpelling *two_notes[] = {P(D, 0, 3), P(E, 0, 3)};

    const StandardCounterpointSlot no_rest[] = {ATK(P(A, 0, 3)), ATK(P(A, 0, 3)), TIE(P(A, 0, 3)), ATK(P(G, 0, 3)),
                                                WHOLE(P(B, 0, 3))};
    EXERCISE(first_bar_without_rest, cantus, no_rest, CANTUS_LOWER);
    assert(!standard_counterpoint_fourth_species_exercise_valid(&first_bar_without_rest));

    const StandardCounterpointSlot short_line[] = {REST(), ATK(P(A, 0, 3)), TIE(P(A, 0, 3)), ATK(P(G, 0, 3)),
                                                   WHOLE(P(B, 0, 3)), ATK(P(A, 0, 3))};
    EXERCISE(wrong_count, cantus, short_line, CANTUS_LOWER);
    assert(!standard_counterpoint_fourth_species_exercise_valid(&wrong_count));

    /* A tie must keep the pitch of the note before it. */
    const StandardCounterpointSlot other_pitch[] = {REST(), ATK(P(A, 0, 3)), TIE(P(B, 0, 3)), ATK(P(G, 0, 3)),
                                                    WHOLE(P(B, 0, 3))};
    EXERCISE(tie_to_other_pitch, cantus, other_pitch, CANTUS_LOWER);
    assert(!standard_counterpoint_fourth_species_exercise_valid(&tie_to_other_pitch));

    /* The weak beat of a middle bar is a new note. */
    const StandardCounterpointSlot weak_tie[] = {REST(), ATK(P(A, 0, 3)), TIE(P(A, 0, 3)), TIE(P(A, 0, 3)),
                                                 WHOLE(P(B, 0, 3))};
    EXERCISE(weak_beat_tie, cantus, weak_tie, CANTUS_LOWER);
    assert(!standard_counterpoint_fourth_species_exercise_valid(&weak_beat_tie));

    const StandardCounterpointSlot rest_inside[] = {REST(), ATK(P(A, 0, 3)), REST(), ATK(P(G, 0, 3)),
                                                    WHOLE(P(B, 0, 3))};
    EXERCISE(rest_in_second_bar, cantus, rest_inside, CANTUS_LOWER);
    assert(!standard_counterpoint_fourth_species_exercise_valid(&rest_in_second_bar));

    /* The last bar is a whole note, an attack and a tie. */
    const StandardCounterpointSlot two_notes_last[] = {REST(), ATK(P(A, 0, 3)), TIE(P(A, 0, 3)), ATK(P(G, 0, 3)),
                                                       ATK(P(B, 0, 3)), ATK(P(A, 0, 3))};
    EXERCISE(last_bar_two_notes, cantus, two_notes_last, CANTUS_LOWER);
    assert(!standard_counterpoint_fourth_species_exercise_valid(&last_bar_two_notes));

    const StandardCounterpointSlot null_pitch[] = {REST(), {STANDARD_COUNTERPOINT_SLOT_ATTACK, NULL},
                                                   TIE(P(A, 0, 3)), ATK(P(G, 0, 3)), WHOLE(P(B, 0, 3))};
    EXERCISE(attack_without_pitch, cantus, null_pitch, CANTUS_LOWER);
    assert(!standard_counterpoint_fourth_species_exercise_valid(&attack_without_pitch));

    const StandardCounterpointSlot slots[] = {REST(), ATK(P(A, 0, 3)), TIE(P(A, 0, 3)), ATK(P(G, 0, 3)),
                                              WHOLE(P(B, 0, 3))};
    EXERCISE(valid, cantus, slots, CANTUS_LOWER);
    valid.cantus_part = (StandardCounterpointCantusPart)5;
    assert(!standard_counterpoint_fourth_species_exercise_valid(&valid));
    valid.cantus_part = CANTUS_LOWER;
    assert(standard_counterpoint_fourth_species_exercise_valid(&valid));

    StandardCounterpointLine short_cantus = {two_notes, 2};
    valid.cantus = &short_cantus;
    assert(!standard_counterpoint_fourth_species_exercise_valid(&valid));
    assert(!standard_counterpoint_fourth_species_exercise_valid(NULL));
    assert(standard_counterpoint_fourth_species_bar_count(NULL) == 0);
}

static void test_slot_strength(void) {
    assert(standard_counterpoint_fourth_species_slot_strength(0) == BEAT_STRENGTH_STRONG);
    assert(standard_counterpoint_fourth_species_slot_strength(1) == BEAT_STRENGTH_WEAK);
    assert(standard_counterpoint_fourth_species_slot_strength(2) == BEAT_STRENGTH_STRONG);
    assert(standard_counterpoint_fourth_species_slot_strength(3) == BEAT_STRENGTH_WEAK);
}

static void test_accessors(void) {
    GOOD_FOURTH_SPECIES_CANTUS(cantus);
    GOOD_FOURTH_SPECIES_SLOTS(slots);
    EXERCISE(exercise, cantus, slots, CANTUS_LOWER);

    assert(standard_counterpoint_fourth_species_cantus(&exercise, 1) == cantus[1]);
    assert(standard_counterpoint_fourth_species_cantus(&exercise, 11) == NULL);

    /* The first bar has a half rest. */
    assert(standard_counterpoint_fourth_species_downbeat(&exercise, 0) == NULL);
    assert(pitch_spelling_equals(standard_counterpoint_fourth_species_weak(&exercise, 0), P(A, 0, 3)));
    assert(standard_counterpoint_fourth_species_weak(&exercise, 11) == NULL);

    /* A tied downbeat sounds the weak beat before it. */
    assert(pitch_spelling_equals(standard_counterpoint_fourth_species_downbeat(&exercise, 2), P(A, 0, 4)));
    assert(standard_counterpoint_fourth_species_downbeat_is_tie(&exercise, 2));
    assert(!standard_counterpoint_fourth_species_downbeat_is_tie(&exercise, 0));

    /* Species is broken in bar 8, whose downbeat is a new note. */
    assert(!standard_counterpoint_fourth_species_downbeat_is_tie(&exercise, 8));
    assert(standard_counterpoint_fourth_species_bar_is_broken(&exercise, 8));
    assert(!standard_counterpoint_fourth_species_bar_is_broken(&exercise, 7));
    assert(!standard_counterpoint_fourth_species_bar_is_broken(&exercise, 0));
    assert(!standard_counterpoint_fourth_species_bar_is_broken(&exercise, 10));

    StandardCounterpointInterval interval;
    assert(!standard_counterpoint_fourth_species_downbeat_interval(&exercise, 0, &interval));
    assert(standard_counterpoint_fourth_species_downbeat_interval(&exercise, 1, &interval));
    assert(interval.size == 3 && interval.quality == STANDARD_COUNTERPOINT_QUALITY_MAJOR);
    assert(standard_counterpoint_fourth_species_weak_interval(&exercise, 9, &interval));
    assert(interval.size == 6 && interval.quality == STANDARD_COUNTERPOINT_QUALITY_MAJOR);
    assert(!standard_counterpoint_fourth_species_weak_interval(&exercise, 11, &interval));
}

static void test_attacks(void) {
    GOOD_FOURTH_SPECIES_CANTUS(cantus);
    GOOD_FOURTH_SPECIES_SLOTS(slots);
    EXERCISE(exercise, cantus, slots, CANTUS_LOWER);
    const PitchSpelling *notes[2 * STANDARD_COUNTERPOINT_FOURTH_SPECIES_MAX_BARS];
    size_t positions[2 * STANDARD_COUNTERPOINT_FOURTH_SPECIES_MAX_BARS];
    size_t count = 0;

    assert(standard_counterpoint_fourth_species_attacks(&exercise, notes, positions, 2 * 64, &count));
    assert(count == 12);
    assert(pitch_spelling_equals(notes[0], P(A, 0, 3)) && positions[0] == 1);
    assert(pitch_spelling_equals(notes[8], P(A, 0, 3)) && positions[8] == 16);
    assert(pitch_spelling_equals(notes[11], P(D, 0, 4)) && positions[11] == 20);

    count = 99;
    assert(!standard_counterpoint_fourth_species_attacks(&exercise, notes, positions, 2, &count));
    assert(count == 99);
    assert(!standard_counterpoint_fourth_species_attacks(NULL, notes, positions, 8, &count));
}

int main(void) {
    test_valid_exercise();
    test_invalid_shapes();
    test_slot_strength();
    test_accessors();
    test_attacks();
    printf("test_fourth_species_exercise (standard-counterpoint): ok\n");
    return 0;
}
