#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "fourth_species_suspension.h"
#include "fourth_species_test_util.h"
#include "semantics/non_chord_tone.h"

static void test_names_and_kinds(void) {
    assert(strcmp(standard_counterpoint_suspension_name(STANDARD_COUNTERPOINT_SUSPENSION_SEVEN_SIX), "7-6") == 0);
    assert(strcmp(standard_counterpoint_suspension_name(STANDARD_COUNTERPOINT_SUSPENSION_FOUR_THREE), "4-3") == 0);
    assert(strcmp(standard_counterpoint_suspension_name(STANDARD_COUNTERPOINT_SUSPENSION_NINE_EIGHT), "9-8") == 0);
    assert(strcmp(standard_counterpoint_suspension_name(STANDARD_COUNTERPOINT_SUSPENSION_TWO_THREE), "2-3") == 0);
    assert(strcmp(standard_counterpoint_suspension_name(STANDARD_COUNTERPOINT_SUSPENSION_CONSONANT_TIE),
                  "consonant-tie") == 0);
    assert(standard_counterpoint_suspension_name(STANDARD_COUNTERPOINT_SUSPENSION_COUNT) == NULL);

    assert(standard_counterpoint_suspension_is_dissonant(STANDARD_COUNTERPOINT_SUSPENSION_SEVEN_SIX));
    assert(standard_counterpoint_suspension_is_dissonant(STANDARD_COUNTERPOINT_SUSPENSION_TWO_THREE));
    assert(!standard_counterpoint_suspension_is_dissonant(STANDARD_COUNTERPOINT_SUSPENSION_CONSONANT_TIE));
    assert(!standard_counterpoint_suspension_is_dissonant(STANDARD_COUNTERPOINT_SUSPENSION_COUNT));

    assert(standard_counterpoint_suspension_non_chord_tone(STANDARD_COUNTERPOINT_SUSPENSION_NINE_EIGHT) ==
           NON_CHORD_TONE_SUSPENSION);
    assert(standard_counterpoint_suspension_non_chord_tone(STANDARD_COUNTERPOINT_SUSPENSION_CONSONANT_TIE) ==
           NON_CHORD_TONE_COUNT);
    assert(standard_counterpoint_suspension_non_chord_tone(STANDARD_COUNTERPOINT_SUSPENSION_COUNT) ==
           NON_CHORD_TONE_COUNT);
}

static void test_good_exercise_suspensions(void) {
    GOOD_FOURTH_SPECIES_CANTUS(cantus);
    GOOD_FOURTH_SPECIES_SLOTS(slots);
    EXERCISE(exercise, cantus, slots, CANTUS_LOWER);
    StandardCounterpointSuspensionType type;

    /* Bar 1 ties a consonance but does not resolve down by step. */
    assert(!standard_counterpoint_fourth_species_suspension(&exercise, 1, &type));
    assert(standard_counterpoint_fourth_species_suspension(&exercise, 2, &type));
    assert(type == STANDARD_COUNTERPOINT_SUSPENSION_FOUR_THREE);
    assert(standard_counterpoint_fourth_species_suspension(&exercise, 3, &type));
    assert(type == STANDARD_COUNTERPOINT_SUSPENSION_FOUR_THREE);
    assert(standard_counterpoint_fourth_species_suspension(&exercise, 4, &type));
    assert(type == STANDARD_COUNTERPOINT_SUSPENSION_SEVEN_SIX);
    assert(standard_counterpoint_fourth_species_suspension(&exercise, 5, &type));
    assert(type == STANDARD_COUNTERPOINT_SUSPENSION_SEVEN_SIX);
    assert(standard_counterpoint_fourth_species_suspension(&exercise, 6, &type));
    assert(type == STANDARD_COUNTERPOINT_SUSPENSION_FOUR_THREE);
    assert(standard_counterpoint_fourth_species_suspension(&exercise, 7, &type));
    assert(type == STANDARD_COUNTERPOINT_SUSPENSION_FOUR_THREE);
    assert(standard_counterpoint_fourth_species_suspension(&exercise, 9, &type));
    assert(type == STANDARD_COUNTERPOINT_SUSPENSION_SEVEN_SIX);

    /* The first and last bars cannot hold one, and a broken bar has no tie. */
    assert(!standard_counterpoint_fourth_species_suspension(&exercise, 0, &type));
    assert(!standard_counterpoint_fourth_species_suspension(&exercise, 8, &type));
    assert(!standard_counterpoint_fourth_species_suspension(&exercise, 10, &type));
    assert(!standard_counterpoint_fourth_species_suspension(&exercise, 11, &type));
    assert(!standard_counterpoint_fourth_species_suspension(NULL, 3, &type));
    assert(!standard_counterpoint_fourth_species_suspension(&exercise, 3, NULL));
}

static void test_nine_eight_above(void) {
    /* E4 over D3 is a ninth, and resolves down by step to an octave. */
    const PitchSpelling *cantus[] = {P(C, 0, 3), P(D, 0, 3), P(C, 0, 3)};
    const StandardCounterpointSlot slots[] = {REST(), ATK(P(E, 0, 4)), TIE(P(E, 0, 4)), ATK(P(D, 0, 4)),
                                              WHOLE(P(C, 0, 4))};
    EXERCISE(exercise, cantus, slots, CANTUS_LOWER);
    StandardCounterpointSuspensionType type;
    /* Bar 1: E4 over D3 (ninth), resolving to D4 (octave). */
    assert(standard_counterpoint_fourth_species_suspension(&exercise, 1, &type));
    assert(type == STANDARD_COUNTERPOINT_SUSPENSION_NINE_EIGHT);
}

static void test_consonant_tie(void) {
    /* A3 over C3 is a sixth, and resolves down by step to a fifth. */
    const PitchSpelling *cantus[] = {P(C, 0, 3), P(C, 0, 3), P(C, 0, 3)};
    const StandardCounterpointSlot slots[] = {REST(), ATK(P(A, 0, 3)), TIE(P(A, 0, 3)), ATK(P(G, 0, 3)),
                                              WHOLE(P(C, 0, 4))};
    EXERCISE(exercise, cantus, slots, CANTUS_LOWER);
    StandardCounterpointSuspensionType type;
    assert(standard_counterpoint_fourth_species_suspension(&exercise, 1, &type));
    assert(type == STANDARD_COUNTERPOINT_SUSPENSION_CONSONANT_TIE);
}

static void test_two_three_below(void) {
    /* D2 under E3 is a second (a ninth); resolving down to C#2 gives a third. */
    const PitchSpelling *cantus[] = {P(F, 0, 3), P(E, 0, 3), P(D, 0, 3)};
    const StandardCounterpointSlot slots[] = {REST(), ATK(P(D, 0, 2)), TIE(P(D, 0, 2)), ATK(P(C, 1, 2)),
                                              WHOLE(P(D, 0, 2))};
    EXERCISE(exercise, cantus, slots, CANTUS_UPPER);
    StandardCounterpointSuspensionType type;
    assert(standard_counterpoint_fourth_species_suspension(&exercise, 1, &type));
    assert(type == STANDARD_COUNTERPOINT_SUSPENSION_TWO_THREE);

    /* Below the cantus firmus a seventh is not a suspension. */
    const PitchSpelling *cantus_b[] = {P(F, 0, 3), P(E, 0, 3), P(D, 0, 3)};
    const StandardCounterpointSlot slots_b[] = {REST(), ATK(P(F, 0, 2)), TIE(P(F, 0, 2)), ATK(P(E, 0, 2)),
                                                WHOLE(P(D, 0, 2))};
    EXERCISE(seventh_below, cantus_b, slots_b, CANTUS_UPPER);
    assert(!standard_counterpoint_fourth_species_suspension(&seventh_below, 1, &type));
}

static void test_improper_suspensions(void) {
    StandardCounterpointSuspensionType type;
    const PitchSpelling *cantus[] = {P(D, 0, 3), P(F, 0, 3), P(E, 0, 3)};

    /* A dissonant preparation: G3 sounds a fourth over D3. */
    const StandardCounterpointSlot dissonant_prep[] = {REST(), ATK(P(G, 0, 3)), TIE(P(G, 0, 3)), ATK(P(F, 0, 3)),
                                                       WHOLE(P(D, 0, 4))};
    EXERCISE(prepared_on_dissonance, cantus, dissonant_prep, CANTUS_LOWER);
    assert(!standard_counterpoint_fourth_species_suspension(&prepared_on_dissonance, 1, &type));

    /* A tritone is not one of the suspension intervals. */
    const StandardCounterpointSlot tritone[] = {REST(), ATK(P(B, 0, 3)), TIE(P(B, 0, 3)), ATK(P(A, 0, 3)),
                                                WHOLE(P(D, 0, 4))};
    EXERCISE(tritone_suspension, cantus, tritone, CANTUS_LOWER);
    assert(!standard_counterpoint_fourth_species_suspension(&tritone_suspension, 1, &type));

    /* A seventh that resolves up, and one that resolves by a leap. */
    const PitchSpelling *cantus_s[] = {P(D, 0, 3), P(E, 0, 3), P(D, 0, 3)};
    const StandardCounterpointSlot up[] = {REST(), ATK(P(D, 0, 4)), TIE(P(D, 0, 4)), ATK(P(E, 0, 4)),
                                           WHOLE(P(D, 0, 4))};
    EXERCISE(resolves_up, cantus_s, up, CANTUS_LOWER);
    assert(!standard_counterpoint_fourth_species_suspension(&resolves_up, 1, &type));

    const StandardCounterpointSlot leap[] = {REST(), ATK(P(D, 0, 4)), TIE(P(D, 0, 4)), ATK(P(A, 0, 3)),
                                             WHOLE(P(D, 0, 4))};
    EXERCISE(resolves_by_leap, cantus_s, leap, CANTUS_LOWER);
    assert(!standard_counterpoint_fourth_species_suspension(&resolves_by_leap, 1, &type));

    /* The same seventh resolving down by step is a 7-6. */
    const StandardCounterpointSlot proper[] = {REST(), ATK(P(D, 0, 4)), TIE(P(D, 0, 4)), ATK(P(C, 0, 4)),
                                               WHOLE(P(D, 0, 4))};
    EXERCISE(seven_six, cantus_s, proper, CANTUS_LOWER);
    assert(standard_counterpoint_fourth_species_suspension(&seven_six, 1, &type));
    assert(type == STANDARD_COUNTERPOINT_SUSPENSION_SEVEN_SIX);
}

int main(void) {
    test_names_and_kinds();
    test_good_exercise_suspensions();
    test_nine_eight_above();
    test_consonant_tie();
    test_two_three_below();
    test_improper_suspensions();
    printf("test_fourth_species_suspension (standard-counterpoint): ok\n");
    return 0;
}
