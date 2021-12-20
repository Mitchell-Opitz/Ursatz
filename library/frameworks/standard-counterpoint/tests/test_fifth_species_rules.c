#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "fifth_species_rules.h"
#include "fifth_species_test_util.h"

#define BIT(group) (1u << (group))
#define GROUP(name) STANDARD_COUNTERPOINT_FIFTH_SPECIES_GROUP_##name

static void test_group_names(void) {
    assert(strcmp(standard_counterpoint_fifth_species_group_name(GROUP(CANTUS_FIRMUS)), "cantus-firmus") == 0);
    assert(strcmp(standard_counterpoint_fifth_species_group_name(GROUP(SPACING)), "spacing") == 0);
    assert(strcmp(standard_counterpoint_fifth_species_group_name(GROUP(RHYTHM)), "rhythm") == 0);
    assert(strcmp(standard_counterpoint_fifth_species_group_name(GROUP(SUSPENSIONS)), "suspensions") == 0);
    assert(strcmp(standard_counterpoint_fifth_species_group_name(GROUP(HARMONY)), "harmony") == 0);
    assert(strcmp(standard_counterpoint_fifth_species_group_name(GROUP(ENDS)), "ends") == 0);
    assert(strcmp(standard_counterpoint_fifth_species_group_name(GROUP(COUNTERPOINT_LINE)), "counterpoint-line") == 0);
    assert(standard_counterpoint_fifth_species_group_name(GROUP(COUNT)) == NULL);
}

static void test_valid_exercise(void) {
    GOOD_FIFTH_SPECIES_CANTUS(cantus);
    GOOD_FIFTH_SPECIES_SLOTS(slots);
    EXERCISE(exercise, cantus, slots, CANTUS_LOWER);
    StandardCounterpointFifthSpeciesInput input = {&exercise, P(D, 0, 3)};
    StandardCounterpointResult result = {99, 99};

    assert(standard_counterpoint_fifth_species_check(&input, &result));
    assert(result.hard_violations == 0);
    assert(standard_counterpoint_fifth_species_valid(&input));
    /* Nothing is left to advise on the counterpoint itself. */
    unsigned clean = BIT(GROUP(SPACING)) | BIT(GROUP(RHYTHM)) | BIT(GROUP(SUSPENSIONS)) | BIT(GROUP(HARMONY)) |
                     BIT(GROUP(ENDS)) | BIT(GROUP(COUNTERPOINT_LINE));
    assert(!(result.soft_violations & clean));
}

static void test_rhythm_violation(void) {
    /* An eighth reached by a leap of a third. */
    const PitchSpelling *cantus[] = {P(C, 0, 3), P(C, 0, 3), P(C, 0, 3)};
    const StandardCounterpointSlot slots[] = {REST4(), HALF(P(G, 0, 3)), QUARTER(P(E, 0, 4)), EIGHTH(P(C, 0, 4)),
                                              EIGHTH(P(D, 0, 4)), HALF(P(E, 0, 4)), WHOLE(P(C, 0, 4))};
    EXERCISE(exercise, cantus, slots, CANTUS_LOWER);
    StandardCounterpointFifthSpeciesInput input = {&exercise, NULL};
    StandardCounterpointResult result;

    assert(standard_counterpoint_fifth_species_check(&input, &result));
    assert(result.hard_violations & BIT(GROUP(RHYTHM)));
    assert(!standard_counterpoint_fifth_species_valid(&input));
}

static void test_suspension_violation(void) {
    /* F4 tied over C3 and resolved by a leap. */
    const PitchSpelling *cantus[] = {P(D, 0, 3), P(C, 0, 3), P(C, 0, 3)};
    const StandardCounterpointSlot slots[] = {REST4(), HALF(P(F, 0, 4)), T2(P(F, 0, 4)), ATK(P(C, 0, 4)),
                                              T5(P(C, 0, 4)), WHOLE(P(C, 0, 4))};
    EXERCISE(exercise, cantus, slots, CANTUS_LOWER);
    StandardCounterpointFifthSpeciesInput input = {&exercise, NULL};
    StandardCounterpointResult result;

    assert(standard_counterpoint_fifth_species_check(&input, &result));
    assert(result.hard_violations & BIT(GROUP(SUSPENSIONS)));
    assert(!standard_counterpoint_fifth_species_valid(&input));
}

static void test_harmony_violation(void) {
    /* A new note, D4, a ninth over C3, on a downbeat. */
    const PitchSpelling *cantus[] = {P(C, 0, 3), P(C, 0, 3), P(C, 0, 3)};
    const StandardCounterpointSlot slots[] = {REST4(), HALF(P(E, 0, 4)), HALF(P(D, 0, 4)), HALF(P(C, 0, 4)),
                                              WHOLE(P(C, 0, 4))};
    EXERCISE(exercise, cantus, slots, CANTUS_LOWER);
    StandardCounterpointFifthSpeciesInput input = {&exercise, NULL};
    StandardCounterpointResult result;

    assert(standard_counterpoint_fifth_species_check(&input, &result));
    assert(result.hard_violations & BIT(GROUP(HARMONY)));
    assert(!(result.hard_violations & BIT(GROUP(SUSPENSIONS))));
}

static void test_spacing_violation(void) {
    /* The counterpoint is above the cantus firmus; with the cantus firmus in
       the upper part, the parts are crossed. */
    GOOD_FIFTH_SPECIES_CANTUS(cantus);
    GOOD_FIFTH_SPECIES_SLOTS(slots);
    EXERCISE(exercise, cantus, slots, CANTUS_UPPER);
    StandardCounterpointFifthSpeciesInput input = {&exercise, P(D, 0, 3)};
    StandardCounterpointResult result;

    assert(standard_counterpoint_fifth_species_check(&input, &result));
    assert(result.hard_violations & BIT(GROUP(SPACING)));
    assert(!standard_counterpoint_fifth_species_valid(&input));
}

static void test_cantus_firmus_and_tonic_groups(void) {
    /* Three bars is too short for a cantus firmus, and the counterpoint begins
       on a third. */
    const PitchSpelling *cantus[] = {P(D, 0, 3), P(E, 0, 3), P(D, 0, 3)};
    const StandardCounterpointSlot slots[] = {REST4(), HALF(P(F, 0, 4)), ATK(P(D, 0, 4)), T5(P(D, 0, 4)),
                                              QUARTER(P(C, 1, 4)), WHOLE(P(D, 0, 4))};
    EXERCISE(exercise, cantus, slots, CANTUS_LOWER);
    StandardCounterpointFifthSpeciesInput input = {&exercise, P(D, 0, 3)};
    StandardCounterpointResult result;

    assert(standard_counterpoint_fifth_species_check(&input, &result));
    assert(result.hard_violations & BIT(GROUP(CANTUS_FIRMUS)));
    assert(result.hard_violations & BIT(GROUP(ENDS)));

    /* Without the tonic, the groups that need it are skipped. */
    input.tonic = NULL;
    assert(standard_counterpoint_fifth_species_check(&input, &result));
    assert(!(result.hard_violations & (BIT(GROUP(ENDS)) | BIT(GROUP(COUNTERPOINT_LINE)))));
    assert(!(result.soft_violations & (BIT(GROUP(ENDS)) | BIT(GROUP(COUNTERPOINT_LINE)))));
}

static void test_invalid_input(void) {
    const PitchSpelling *cantus[] = {P(C, 0, 3), P(D, 0, 3)};
    const StandardCounterpointSlot too_few[] = {WHOLE(P(E, 0, 3)), WHOLE(P(E, 0, 3))};
    EXERCISE(invalid, cantus, too_few, CANTUS_LOWER);
    StandardCounterpointFifthSpeciesInput input = {&invalid, P(D, 0, 3)};
    StandardCounterpointFifthSpeciesInput no_exercise = {NULL, P(D, 0, 3)};
    StandardCounterpointResult result = {7, 7};

    assert(!standard_counterpoint_fifth_species_check(NULL, &result));
    assert(!standard_counterpoint_fifth_species_check(&input, NULL));
    assert(!standard_counterpoint_fifth_species_check(&input, &result));
    assert(!standard_counterpoint_fifth_species_check(&no_exercise, &result));
    assert(result.hard_violations == 7 && result.soft_violations == 7);
    assert(!standard_counterpoint_fifth_species_valid(&input));
    assert(!standard_counterpoint_fifth_species_valid(NULL));
}

int main(void) {
    test_group_names();
    test_valid_exercise();
    test_rhythm_violation();
    test_suspension_violation();
    test_harmony_violation();
    test_spacing_violation();
    test_cantus_firmus_and_tonic_groups();
    test_invalid_input();
    printf("test_fifth_species_rules (standard-counterpoint): ok\n");
    return 0;
}
