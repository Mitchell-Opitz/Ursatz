#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "fourth_species_rules.h"
#include "fourth_species_test_util.h"

#define BIT(group) (1u << (group))
#define GROUP(name) STANDARD_COUNTERPOINT_FOURTH_SPECIES_GROUP_##name

static void test_group_names(void) {
    assert(strcmp(standard_counterpoint_fourth_species_group_name(GROUP(CANTUS_FIRMUS)), "cantus-firmus") == 0);
    assert(strcmp(standard_counterpoint_fourth_species_group_name(GROUP(SPACING)), "spacing") == 0);
    assert(strcmp(standard_counterpoint_fourth_species_group_name(GROUP(SUSPENSIONS)), "suspensions") == 0);
    assert(strcmp(standard_counterpoint_fourth_species_group_name(GROUP(WEAK_BEATS)), "weak-beats") == 0);
    assert(strcmp(standard_counterpoint_fourth_species_group_name(GROUP(SPECIES_BREAKS)), "species-breaks") == 0);
    assert(strcmp(standard_counterpoint_fourth_species_group_name(GROUP(ENDS)), "ends") == 0);
    assert(strcmp(standard_counterpoint_fourth_species_group_name(GROUP(COUNTERPOINT_LINE)), "counterpoint-line") == 0);
    assert(standard_counterpoint_fourth_species_group_name(GROUP(COUNT)) == NULL);
}

static void test_valid_exercise(void) {
    GOOD_FOURTH_SPECIES_CANTUS(cantus);
    GOOD_FOURTH_SPECIES_SLOTS(slots);
    EXERCISE(exercise, cantus, slots, CANTUS_LOWER);
    StandardCounterpointFourthSpeciesInput input = {&exercise, P(D, 0, 3)};
    StandardCounterpointResult result = {99, 99};

    assert(standard_counterpoint_fourth_species_check(&input, &result));
    assert(result.hard_violations == 0);
    assert(standard_counterpoint_fourth_species_valid(&input));
    /* Nothing is left to advise on the suspensions, the weak beats, or the
       ends. */
    unsigned clean = BIT(GROUP(SUSPENSIONS)) | BIT(GROUP(WEAK_BEATS)) | BIT(GROUP(SPECIES_BREAKS)) | BIT(GROUP(ENDS));
    assert(!(result.soft_violations & clean));
}

static void test_suspension_violation(void) {
    /* A tied tritone: B3 against F3. */
    const PitchSpelling *cantus[] = {P(D, 0, 3), P(F, 0, 3), P(E, 0, 3)};
    const StandardCounterpointSlot slots[] = {REST(), ATK(P(B, 0, 3)), TIE(P(B, 0, 3)), ATK(P(A, 0, 3)),
                                              WHOLE(P(D, 0, 4))};
    EXERCISE(exercise, cantus, slots, CANTUS_LOWER);
    StandardCounterpointFourthSpeciesInput input = {&exercise, NULL};
    StandardCounterpointResult result;

    assert(standard_counterpoint_fourth_species_check(&input, &result));
    assert(result.hard_violations & BIT(GROUP(SUSPENSIONS)));
    assert(!standard_counterpoint_fourth_species_valid(&input));
}

static void test_weak_beat_violation(void) {
    /* A second on a weak beat. */
    const PitchSpelling *cantus[] = {P(C, 0, 3), P(C, 0, 3), P(C, 0, 3), P(C, 0, 3)};
    const StandardCounterpointSlot slots[] = {REST(), ATK(P(E, 0, 3)), TIE(P(E, 0, 3)), ATK(P(D, 0, 3)),
                                              TIE(P(D, 0, 3)), ATK(P(E, 0, 3)), WHOLE(P(G, 0, 3))};
    EXERCISE(exercise, cantus, slots, CANTUS_LOWER);
    StandardCounterpointFourthSpeciesInput input = {&exercise, NULL};
    StandardCounterpointResult result;

    assert(standard_counterpoint_fourth_species_check(&input, &result));
    assert(result.hard_violations & BIT(GROUP(WEAK_BEATS)));
}

static void test_species_break_violation(void) {
    /* A broken bar whose downbeat is a fourth above the cantus firmus. */
    const PitchSpelling *cantus[] = {P(C, 0, 3), P(C, 0, 3), P(C, 0, 3), P(C, 0, 3)};
    const StandardCounterpointSlot slots[] = {REST(), ATK(P(G, 0, 3)), ATK(P(F, 0, 3)), ATK(P(G, 0, 3)),
                                              TIE(P(G, 0, 3)), ATK(P(E, 0, 3)), WHOLE(P(C, 0, 4))};
    EXERCISE(exercise, cantus, slots, CANTUS_LOWER);
    StandardCounterpointFourthSpeciesInput input = {&exercise, NULL};
    StandardCounterpointResult result;

    assert(standard_counterpoint_fourth_species_check(&input, &result));
    assert(result.hard_violations & BIT(GROUP(SPECIES_BREAKS)));
    assert(!(result.hard_violations & BIT(GROUP(WEAK_BEATS))));
}

static void test_spacing_violation(void) {
    /* The counterpoint is above the cantus firmus; with the cantus firmus in
       the upper part, the parts are crossed. */
    GOOD_FOURTH_SPECIES_CANTUS(cantus);
    GOOD_FOURTH_SPECIES_SLOTS(slots);
    EXERCISE(exercise, cantus, slots, CANTUS_UPPER);
    StandardCounterpointFourthSpeciesInput input = {&exercise, P(D, 0, 3)};
    StandardCounterpointResult result;

    assert(standard_counterpoint_fourth_species_check(&input, &result));
    assert(result.hard_violations & BIT(GROUP(SPACING)));
    assert(!standard_counterpoint_fourth_species_valid(&input));
}

static void test_cantus_firmus_and_tonic_groups(void) {
    /* Three bars is too short for a cantus firmus, and the counterpoint begins
       on a mi. */
    const PitchSpelling *cantus[] = {P(D, 0, 3), P(E, 0, 3), P(D, 0, 3)};
    const StandardCounterpointSlot slots[] = {REST(), ATK(P(F, 0, 4)), TIE(P(F, 0, 4)), ATK(P(C, 1, 4)),
                                              WHOLE(P(D, 0, 4))};
    EXERCISE(exercise, cantus, slots, CANTUS_LOWER);
    StandardCounterpointFourthSpeciesInput input = {&exercise, P(D, 0, 3)};
    StandardCounterpointResult result;

    assert(standard_counterpoint_fourth_species_check(&input, &result));
    assert(result.hard_violations & BIT(GROUP(CANTUS_FIRMUS)));
    assert(result.hard_violations & BIT(GROUP(ENDS)));

    /* Without the tonic, the groups that need it are skipped. */
    input.tonic = NULL;
    assert(standard_counterpoint_fourth_species_check(&input, &result));
    assert(!(result.hard_violations & (BIT(GROUP(ENDS)) | BIT(GROUP(COUNTERPOINT_LINE)))));
    assert(!(result.soft_violations & (BIT(GROUP(ENDS)) | BIT(GROUP(COUNTERPOINT_LINE)))));
}

static void test_invalid_input(void) {
    const PitchSpelling *cantus[] = {P(C, 0, 3), P(D, 0, 3)};
    const StandardCounterpointSlot too_few[] = {WHOLE(P(E, 0, 3))};
    EXERCISE(invalid, cantus, too_few, CANTUS_LOWER);
    StandardCounterpointFourthSpeciesInput input = {&invalid, P(D, 0, 3)};
    StandardCounterpointFourthSpeciesInput no_exercise = {NULL, P(D, 0, 3)};
    StandardCounterpointResult result = {7, 7};

    assert(!standard_counterpoint_fourth_species_check(NULL, &result));
    assert(!standard_counterpoint_fourth_species_check(&input, NULL));
    assert(!standard_counterpoint_fourth_species_check(&input, &result));
    assert(!standard_counterpoint_fourth_species_check(&no_exercise, &result));
    assert(result.hard_violations == 7 && result.soft_violations == 7);
    assert(!standard_counterpoint_fourth_species_valid(&input));
    assert(!standard_counterpoint_fourth_species_valid(NULL));
}

int main(void) {
    test_group_names();
    test_valid_exercise();
    test_suspension_violation();
    test_weak_beat_violation();
    test_species_break_violation();
    test_spacing_violation();
    test_cantus_firmus_and_tonic_groups();
    test_invalid_input();
    printf("test_fourth_species_rules (standard-counterpoint): ok\n");
    return 0;
}
