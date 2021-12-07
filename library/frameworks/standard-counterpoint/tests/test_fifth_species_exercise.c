#include <assert.h>
#include <stdio.h>

#include "fifth_species_exercise.h"
#include "fifth_species_test_util.h"

static void test_valid_exercises(void) {
    const PitchSpelling *cantus[] = {P(D, 0, 3), P(F, 0, 3), P(E, 0, 3)};
    const StandardCounterpointSlot slots[] = {REST4(), HALF(P(A, 0, 3)), WHOLE(P(A, 0, 3)), WHOLE(P(B, 0, 3))};
    EXERCISE(exercise, cantus, slots, CANTUS_LOWER);
    assert(standard_counterpoint_fifth_species_exercise_valid(&exercise));
    assert(standard_counterpoint_fifth_species_bar_count(&exercise) == 3);

    GOOD_FIFTH_SPECIES_CANTUS(good_notes);
    GOOD_FIFTH_SPECIES_SLOTS(good_pitches);
    EXERCISE(good, good_notes, good_pitches, CANTUS_LOWER);
    assert(standard_counterpoint_fifth_species_exercise_valid(&good));
    assert(standard_counterpoint_fifth_species_bar_count(&good) == 11);

    /* No rest, a quarter rest, three quarter rests. */
    const StandardCounterpointSlot no_rest[] = {WHOLE(P(A, 0, 3)), WHOLE(P(A, 0, 3)), WHOLE(P(B, 0, 3))};
    EXERCISE(no_rest_exercise, cantus, no_rest, CANTUS_LOWER);
    assert(standard_counterpoint_fifth_species_exercise_valid(&no_rest_exercise));

    const StandardCounterpointSlot quarter_rest[] = {REST(), REST(), QUARTER(P(A, 0, 3)), HALF(P(A, 0, 3)),
                                                     WHOLE(P(A, 0, 3)), WHOLE(P(B, 0, 3))};
    EXERCISE(quarter_rest_exercise, cantus, quarter_rest, CANTUS_LOWER);
    assert(standard_counterpoint_fifth_species_exercise_valid(&quarter_rest_exercise));

    const StandardCounterpointSlot three_rests[] = {REST4(), REST(), REST(), QUARTER(P(A, 0, 3)), WHOLE(P(A, 0, 3)),
                                                    WHOLE(P(B, 0, 3))};
    EXERCISE(three_rests_exercise, cantus, three_rests, CANTUS_LOWER);
    assert(standard_counterpoint_fifth_species_exercise_valid(&three_rests_exercise));
}

static void test_invalid_shapes(void) {
    const PitchSpelling *cantus[] = {P(D, 0, 3), P(F, 0, 3), P(E, 0, 3)};
    const PitchSpelling *two_notes[] = {P(D, 0, 3), P(E, 0, 3)};

    const StandardCounterpointSlot short_line[] = {REST4(), HALF(P(A, 0, 3)), WHOLE(P(A, 0, 3))};
    EXERCISE(wrong_count, cantus, short_line, CANTUS_LOWER);
    assert(!standard_counterpoint_fifth_species_exercise_valid(&wrong_count));

    /* A rest of one eighth. */
    const StandardCounterpointSlot one_eighth_rest[] = {REST(), ATK(P(A, 0, 3)), T2(P(A, 0, 3)), TIE(P(A, 0, 3)),
                                                        T3(P(A, 0, 3)), WHOLE(P(A, 0, 3)), WHOLE(P(B, 0, 3))};
    EXERCISE(eighth_rest, cantus, one_eighth_rest, CANTUS_LOWER);
    assert(!standard_counterpoint_fifth_species_exercise_valid(&eighth_rest));

    /* A rest after the first note. */
    const StandardCounterpointSlot late_rest[] = {WHOLE(P(A, 0, 3)), REST4(), HALF(P(A, 0, 3)), WHOLE(P(B, 0, 3))};
    EXERCISE(rest_in_second_bar, cantus, late_rest, CANTUS_LOWER);
    assert(!standard_counterpoint_fifth_species_exercise_valid(&rest_in_second_bar));

    /* A lone eighth note, and eighths on the first beat. */
    const StandardCounterpointSlot lone_eighth[] = {WHOLE(P(A, 0, 3)), QUARTER(P(A, 0, 3)), EIGHTH(P(B, 0, 3)),
                                                    HALF(P(B, 0, 3)), TIE(P(B, 0, 3)), WHOLE(P(B, 0, 3))};
    EXERCISE(lone_eighth_exercise, cantus, lone_eighth, CANTUS_LOWER);
    assert(!standard_counterpoint_fifth_species_exercise_valid(&lone_eighth_exercise));

    const StandardCounterpointSlot downbeat_eighths[] = {WHOLE(P(A, 0, 3)), EIGHTH(P(A, 0, 3)), EIGHTH(P(B, 0, 3)),
                                                         QUARTER(P(A, 0, 3)), HALF(P(B, 0, 3)), WHOLE(P(B, 0, 3))};
    EXERCISE(first_beat_eighths, cantus, downbeat_eighths, CANTUS_LOWER);
    assert(!standard_counterpoint_fifth_species_exercise_valid(&first_beat_eighths));

    /* A quarter note that starts off the beat. */
    const StandardCounterpointSlot odd_start[] = {WHOLE(P(A, 0, 3)), EIGHTH(P(A, 0, 3)), QUARTER(P(B, 0, 3)),
                                                  HALF(P(A, 0, 3)), EIGHTH(P(B, 0, 3)), WHOLE(P(B, 0, 3))};
    EXERCISE(off_the_beat, cantus, odd_start, CANTUS_LOWER);
    assert(!standard_counterpoint_fifth_species_exercise_valid(&off_the_beat));

    /* A tie to another pitch. */
    const StandardCounterpointSlot other_pitch[] = {REST4(), ATK(P(A, 0, 3)), TIE(P(B, 0, 3)), T2(P(A, 0, 3)),
                                                    WHOLE(P(A, 0, 3)), WHOLE(P(B, 0, 3))};
    EXERCISE(tie_to_other_pitch, cantus, other_pitch, CANTUS_LOWER);
    assert(!standard_counterpoint_fifth_species_exercise_valid(&tie_to_other_pitch));

    /* A note that crosses into the last bar, and a last bar that is not whole. */
    const StandardCounterpointSlot into_last[] = {REST4(), HALF(P(A, 0, 3)), HALF(P(A, 0, 3)), ATK(P(B, 0, 3)),
                                                  T7(P(B, 0, 3)), TIE(P(B, 0, 3)), T3(P(B, 0, 3))};
    EXERCISE(crosses_into_last, cantus, into_last, CANTUS_LOWER);
    assert(!standard_counterpoint_fifth_species_exercise_valid(&crosses_into_last));

    const StandardCounterpointSlot last_halves[] = {REST4(), HALF(P(A, 0, 3)), WHOLE(P(A, 0, 3)), HALF(P(B, 0, 3)),
                                                    HALF(P(A, 0, 3))};
    EXERCISE(last_bar_halves, cantus, last_halves, CANTUS_LOWER);
    assert(!standard_counterpoint_fifth_species_exercise_valid(&last_bar_halves));

    const StandardCounterpointSlot null_pitch[] = {REST4(), {STANDARD_COUNTERPOINT_SLOT_ATTACK, NULL}, T3(P(A, 0, 3)),
                                                   WHOLE(P(A, 0, 3)), WHOLE(P(B, 0, 3))};
    EXERCISE(attack_without_pitch, cantus, null_pitch, CANTUS_LOWER);
    assert(!standard_counterpoint_fifth_species_exercise_valid(&attack_without_pitch));

    const StandardCounterpointSlot slots[] = {REST4(), HALF(P(A, 0, 3)), WHOLE(P(A, 0, 3)), WHOLE(P(B, 0, 3))};
    EXERCISE(valid, cantus, slots, CANTUS_LOWER);
    valid.cantus_part = (StandardCounterpointCantusPart)5;
    assert(!standard_counterpoint_fifth_species_exercise_valid(&valid));
    valid.cantus_part = CANTUS_LOWER;
    assert(standard_counterpoint_fifth_species_exercise_valid(&valid));

    StandardCounterpointLine short_cantus = {two_notes, 2};
    valid.cantus = &short_cantus;
    assert(!standard_counterpoint_fifth_species_exercise_valid(&valid));
    assert(!standard_counterpoint_fifth_species_exercise_valid(NULL));
    assert(standard_counterpoint_fifth_species_bar_count(NULL) == 0);
}

static void test_slot_strength(void) {
    assert(standard_counterpoint_fifth_species_slot_strength(0) == BEAT_STRENGTH_STRONG);
    assert(standard_counterpoint_fifth_species_slot_strength(1) == BEAT_STRENGTH_WEAK);
    assert(standard_counterpoint_fifth_species_slot_strength(2) == BEAT_STRENGTH_WEAK);
    assert(standard_counterpoint_fifth_species_slot_strength(4) == BEAT_STRENGTH_MEDIUM);
    assert(standard_counterpoint_fifth_species_slot_strength(7) == BEAT_STRENGTH_WEAK);
    assert(standard_counterpoint_fifth_species_slot_strength(8) == BEAT_STRENGTH_STRONG);
    assert(standard_counterpoint_fifth_species_slot_strength(20) == BEAT_STRENGTH_MEDIUM);
}

static void test_accessors(void) {
    GOOD_FIFTH_SPECIES_CANTUS(cantus);
    GOOD_FIFTH_SPECIES_SLOTS(slots);
    EXERCISE(exercise, cantus, slots, CANTUS_LOWER);

    assert(standard_counterpoint_fifth_species_cantus(&exercise, 1) == cantus[1]);
    assert(standard_counterpoint_fifth_species_cantus(&exercise, 11) == NULL);

    /* A rest has no pitch; a tie sounds the pitch it holds. */
    assert(standard_counterpoint_fifth_species_pitch(&exercise, 0) == NULL);
    assert(pitch_spelling_equals(standard_counterpoint_fifth_species_pitch(&exercise, 4), P(A, 0, 3)));
    assert(pitch_spelling_equals(standard_counterpoint_fifth_species_pitch(&exercise, 8), P(A, 0, 3)));
    assert(standard_counterpoint_fifth_species_pitch(&exercise, 88) == NULL);

    assert(standard_counterpoint_fifth_species_slot_is_attack(&exercise, 4));
    assert(!standard_counterpoint_fifth_species_slot_is_attack(&exercise, 5));
    assert(standard_counterpoint_fifth_species_slot_is_tie(&exercise, 5));
    assert(!standard_counterpoint_fifth_species_slot_is_tie(&exercise, 0));
    assert(!standard_counterpoint_fifth_species_slot_is_attack(&exercise, 0));

    StandardCounterpointInterval interval;
    assert(!standard_counterpoint_fifth_species_interval(&exercise, 0, &interval));
    assert(standard_counterpoint_fifth_species_interval(&exercise, 4, &interval));
    assert(interval.size == 5 && interval.quality == STANDARD_COUNTERPOINT_QUALITY_PERFECT);
    /* The same note held into the next bar is a third over F3. */
    assert(standard_counterpoint_fifth_species_interval(&exercise, 8, &interval));
    assert(interval.size == 3 && interval.quality == STANDARD_COUNTERPOINT_QUALITY_MAJOR);
    assert(!standard_counterpoint_fifth_species_interval(&exercise, 88, &interval));
}

static void test_notes(void) {
    GOOD_FIFTH_SPECIES_CANTUS(cantus);
    GOOD_FIFTH_SPECIES_SLOTS(slots);
    EXERCISE(exercise, cantus, slots, CANTUS_LOWER);
    StandardCounterpointFifthSpeciesNote notes[STANDARD_COUNTERPOINT_FIFTH_SPECIES_MAX_NOTES];
    size_t count = 0;

    assert(standard_counterpoint_fifth_species_notes(&exercise, notes, STANDARD_COUNTERPOINT_FIFTH_SPECIES_MAX_NOTES,
                                                     &count));
    assert(count == 25);
    /* The first note is held across the bar line for a whole note. */
    assert(notes[0].slot == 4 && notes[0].duration == 8 && pitch_spelling_equals(notes[0].pitch, P(A, 0, 3)));
    /* The two eighths of bar 6. */
    assert(notes[15].slot == 54 && notes[15].duration == 1);
    assert(notes[16].slot == 55 && notes[16].duration == 1);
    assert(notes[24].slot == 80 && notes[24].duration == 8);

    count = 99;
    assert(!standard_counterpoint_fifth_species_notes(&exercise, notes, 2, &count));
    assert(count == 99);
    assert(!standard_counterpoint_fifth_species_notes(NULL, notes, 8, &count));
}

static void test_attacks(void) {
    GOOD_FIFTH_SPECIES_CANTUS(cantus);
    GOOD_FIFTH_SPECIES_SLOTS(slots);
    EXERCISE(exercise, cantus, slots, CANTUS_LOWER);
    const PitchSpelling *pitches[STANDARD_COUNTERPOINT_FIFTH_SPECIES_MAX_NOTES];
    size_t positions[STANDARD_COUNTERPOINT_FIFTH_SPECIES_MAX_NOTES];
    size_t count = 0;

    assert(standard_counterpoint_fifth_species_attacks(&exercise, pitches, positions,
                                                       STANDARD_COUNTERPOINT_FIFTH_SPECIES_MAX_NOTES, &count));
    assert(count == 25);
    assert(pitch_spelling_equals(pitches[0], P(A, 0, 3)) && positions[0] == 4);
    assert(pitch_spelling_equals(pitches[3], P(E, 0, 4)) && positions[3] == 16);
    assert(pitch_spelling_equals(pitches[24], P(D, 0, 4)) && positions[24] == 80);

    count = 99;
    assert(!standard_counterpoint_fifth_species_attacks(&exercise, pitches, positions, 2, &count));
    assert(count == 99);
    assert(!standard_counterpoint_fifth_species_attacks(NULL, pitches, positions, 8, &count));
}

int main(void) {
    test_valid_exercises();
    test_invalid_shapes();
    test_slot_strength();
    test_accessors();
    test_notes();
    test_attacks();
    printf("test_fifth_species_exercise (standard-counterpoint): ok\n");
    return 0;
}
