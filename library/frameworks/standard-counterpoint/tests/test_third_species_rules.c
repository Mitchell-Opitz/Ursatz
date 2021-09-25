#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "third_species_rules.h"
#include "third_species_test_util.h"

#define BIT(group) (1u << (group))
#define GROUP(name) STANDARD_COUNTERPOINT_THIRD_SPECIES_GROUP_##name

/* Four quarter notes. */
#define BAR(a, b, c, d) ATK(a), ATK(b), ATK(c), ATK(d)

static void test_group_names(void) {
    assert(strcmp(standard_counterpoint_third_species_group_name(GROUP(CANTUS_FIRMUS)), "cantus-firmus") == 0);
    assert(strcmp(standard_counterpoint_third_species_group_name(GROUP(SPACING)), "spacing") == 0);
    assert(strcmp(standard_counterpoint_third_species_group_name(GROUP(STRONG_BEATS)), "strong-beats") == 0);
    assert(strcmp(standard_counterpoint_third_species_group_name(GROUP(OTHER_BEATS)), "other-beats") == 0);
    assert(strcmp(standard_counterpoint_third_species_group_name(GROUP(ENDS)), "ends") == 0);
    assert(strcmp(standard_counterpoint_third_species_group_name(GROUP(COUNTERPOINT_LINE)), "counterpoint-line") == 0);
    assert(standard_counterpoint_third_species_group_name(GROUP(COUNT)) == NULL);
}

static void test_valid_exercise(void) {
    GOOD_THIRD_SPECIES_CANTUS(cantus);
    GOOD_THIRD_SPECIES_SLOTS(slots);
    EXERCISE(exercise, cantus, slots, CANTUS_LOWER);
    StandardCounterpointThirdSpeciesInput input = {&exercise, P(D, 0, 3)};
    StandardCounterpointResult result = {99, 99};

    assert(standard_counterpoint_third_species_check(&input, &result));
    assert(result.hard_violations == 0);
    assert(standard_counterpoint_third_species_valid(&input));
    /* Nothing is left to advise on the downbeats or the ends. */
    unsigned clean = BIT(GROUP(STRONG_BEATS)) | BIT(GROUP(ENDS));
    assert(!(result.soft_violations & clean));
}

static void test_strong_beat_violation(void) {
    /* A dissonance, E3 against D3, on the downbeat of the second bar. */
    const PitchSpelling *cantus[] = {P(C, 0, 3), P(D, 0, 3)};
    const StandardCounterpointSlot slots[] = {WHOLE(P(E, 0, 3)), WHOLE(P(E, 0, 3))};
    EXERCISE(exercise, cantus, slots, CANTUS_LOWER);
    StandardCounterpointThirdSpeciesInput input = {&exercise, NULL};
    StandardCounterpointResult result;

    assert(standard_counterpoint_third_species_check(&input, &result));
    assert(result.hard_violations & BIT(GROUP(STRONG_BEATS)));
    assert(!standard_counterpoint_third_species_valid(&input));
}

static void test_other_beat_violation(void) {
    /* A dissonance reached by leap. */
    const PitchSpelling *cantus[] = {P(C, 0, 3), P(C, 0, 3)};
    const StandardCounterpointSlot slots[] = {BAR(P(E, 0, 3), P(B, 0, 3), P(A, 0, 3), P(G, 0, 3)), WHOLE(P(E, 0, 3))};
    EXERCISE(exercise, cantus, slots, CANTUS_LOWER);
    StandardCounterpointThirdSpeciesInput input = {&exercise, NULL};
    StandardCounterpointResult result;

    assert(standard_counterpoint_third_species_check(&input, &result));
    assert(result.hard_violations & BIT(GROUP(OTHER_BEATS)));
    assert(!(result.hard_violations & BIT(GROUP(STRONG_BEATS))));
}

static void test_spacing_violation(void) {
    /* The counterpoint is above the cantus firmus; with the cantus firmus in
       the upper part, the parts are crossed. */
    GOOD_THIRD_SPECIES_CANTUS(cantus);
    GOOD_THIRD_SPECIES_SLOTS(slots);
    EXERCISE(exercise, cantus, slots, CANTUS_UPPER);
    StandardCounterpointThirdSpeciesInput input = {&exercise, P(D, 0, 3)};
    StandardCounterpointResult result;

    assert(standard_counterpoint_third_species_check(&input, &result));
    assert(result.hard_violations & BIT(GROUP(SPACING)));
    assert(!standard_counterpoint_third_species_valid(&input));
}

static void test_cantus_firmus_and_tonic_groups(void) {
    /* Two bars is too short for a cantus firmus, and the counterpoint begins
       on a mi. */
    const PitchSpelling *cantus[] = {P(D, 0, 3), P(E, 0, 3)};
    const StandardCounterpointSlot slots[] = {BAR(P(F, 0, 4), P(E, 0, 4), P(D, 0, 4), P(E, 0, 4)), WHOLE(P(E, 0, 4))};
    EXERCISE(exercise, cantus, slots, CANTUS_LOWER);
    StandardCounterpointThirdSpeciesInput input = {&exercise, P(D, 0, 3)};
    StandardCounterpointResult result;

    assert(standard_counterpoint_third_species_check(&input, &result));
    assert(result.hard_violations & BIT(GROUP(CANTUS_FIRMUS)));
    assert(result.hard_violations & BIT(GROUP(ENDS)));

    /* Without the tonic, the groups that need it are skipped. */
    input.tonic = NULL;
    assert(standard_counterpoint_third_species_check(&input, &result));
    assert(!(result.hard_violations & (BIT(GROUP(ENDS)) | BIT(GROUP(COUNTERPOINT_LINE)))));
    assert(!(result.soft_violations & (BIT(GROUP(ENDS)) | BIT(GROUP(COUNTERPOINT_LINE)))));
}

static void test_invalid_input(void) {
    const PitchSpelling *cantus[] = {P(C, 0, 3), P(D, 0, 3)};
    const StandardCounterpointSlot too_few[] = {WHOLE(P(E, 0, 3))};
    EXERCISE(invalid, cantus, too_few, CANTUS_LOWER);
    StandardCounterpointThirdSpeciesInput input = {&invalid, P(D, 0, 3)};
    StandardCounterpointThirdSpeciesInput no_exercise = {NULL, P(D, 0, 3)};
    StandardCounterpointResult result = {7, 7};

    assert(!standard_counterpoint_third_species_check(NULL, &result));
    assert(!standard_counterpoint_third_species_check(&input, NULL));
    assert(!standard_counterpoint_third_species_check(&input, &result));
    assert(!standard_counterpoint_third_species_check(&no_exercise, &result));
    assert(result.hard_violations == 7 && result.soft_violations == 7);
    assert(!standard_counterpoint_third_species_valid(&input));
    assert(!standard_counterpoint_third_species_valid(NULL));
}

int main(void) {
    test_group_names();
    test_valid_exercise();
    test_strong_beat_violation();
    test_other_beat_violation();
    test_spacing_violation();
    test_cantus_firmus_and_tonic_groups();
    test_invalid_input();
    printf("test_third_species_rules (standard-counterpoint): ok\n");
    return 0;
}
