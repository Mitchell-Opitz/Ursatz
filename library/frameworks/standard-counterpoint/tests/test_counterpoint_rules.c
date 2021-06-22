#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "counterpoint_rules.h"
#include "counterpoint_test_util.h"

#define BIT(group) (1u << (group))

/* Fux's Dorian cantus firmus below, a first-species counterpoint above it.
   The pair breaks no hard rule. */
#define LOWER(name)                                                                                       \
    const PitchSpelling *name[] = {P(D, 0, 3), P(F, 0, 3), P(E, 0, 3), P(D, 0, 3), P(G, 0, 3), P(F, 0, 3), \
                                   P(A, 0, 3), P(G, 0, 3), P(F, 0, 3), P(E, 0, 3), P(D, 0, 3)}
#define UPPER(name)                                                                                       \
    const PitchSpelling *name[] = {P(A, 0, 3), P(D, 0, 4), P(C, 0, 4), P(B, 0, 3), P(E, 0, 4), P(D, 0, 4), \
                                   P(C, 0, 4), P(B, 0, 3), P(A, 0, 3), P(C, 1, 4), P(D, 0, 4)}

static void test_group_names(void) {
    assert(strcmp(standard_counterpoint_group_name(STANDARD_COUNTERPOINT_GROUP_CANTUS_FIRMUS), "cantus-firmus") == 0);
    assert(strcmp(standard_counterpoint_group_name(STANDARD_COUNTERPOINT_GROUP_MELODIC), "melodic") == 0);
    assert(strcmp(standard_counterpoint_group_name(STANDARD_COUNTERPOINT_GROUP_HARMONIC), "harmonic") == 0);
    assert(strcmp(standard_counterpoint_group_name(STANDARD_COUNTERPOINT_GROUP_TENDENCY), "tendency") == 0);
    assert(standard_counterpoint_group_name(STANDARD_COUNTERPOINT_GROUP_COUNT) == NULL);
}

static void test_valid_exercise(void) {
    LOWER(lower_notes);
    UPPER(upper_notes);
    StandardCounterpointLine lower = LINE_OF(lower_notes);
    StandardCounterpointLine upper = LINE_OF(upper_notes);
    StandardCounterpointPair pair = {&upper, &lower};
    StandardCounterpointInput input = {&pair, STANDARD_COUNTERPOINT_CANTUS_IN_LOWER, P(D, 0, 3), NULL};
    StandardCounterpointResult result = {99, 99};

    assert(standard_counterpoint_check(&input, &result));
    assert(result.hard_violations == 0);
    assert(standard_counterpoint_valid(&input));
    /* With the default limits, runs of four seconds in both parts are a
       soft melodic violation. */
    assert(result.soft_violations & BIT(STANDARD_COUNTERPOINT_GROUP_MELODIC));

    /* Allow longer runs of one generic interval and nothing is left. */
    StandardCounterpointLimits generous = standard_counterpoint_default_limits();
    generous.max_same_generic_run = 10;
    input.limits = &generous;
    assert(standard_counterpoint_check(&input, &result));
    assert(result.hard_violations == 0);
    assert(!(result.soft_violations & BIT(STANDARD_COUNTERPOINT_GROUP_MELODIC)));
}

static void test_harmonic_violation(void) {
    /* Parallel octaves. The tonic is unknown, so the cantus firmus group is
       skipped. */
    const PitchSpelling *lower_notes[] = {P(C, 0, 3), P(D, 0, 3)};
    const PitchSpelling *upper_notes[] = {P(C, 0, 4), P(D, 0, 4)};
    StandardCounterpointLine lower = LINE_OF(lower_notes);
    StandardCounterpointLine upper = LINE_OF(upper_notes);
    StandardCounterpointPair pair = {&upper, &lower};
    StandardCounterpointInput input = {&pair, STANDARD_COUNTERPOINT_CANTUS_IN_LOWER, NULL, NULL};
    StandardCounterpointResult result;

    assert(standard_counterpoint_check(&input, &result));
    assert(result.hard_violations == BIT(STANDARD_COUNTERPOINT_GROUP_HARMONIC));
    assert(result.soft_violations & BIT(STANDARD_COUNTERPOINT_GROUP_HARMONIC));
    assert(!standard_counterpoint_valid(&input));
}

static void test_cantus_firmus_violation(void) {
    /* The same two-note exercise is too short to be a cantus firmus. */
    const PitchSpelling *lower_notes[] = {P(C, 0, 3), P(D, 0, 3)};
    const PitchSpelling *upper_notes[] = {P(G, 0, 3), P(A, 0, 3)};
    StandardCounterpointLine lower = LINE_OF(lower_notes);
    StandardCounterpointLine upper = LINE_OF(upper_notes);
    StandardCounterpointPair pair = {&upper, &lower};
    StandardCounterpointInput input = {&pair, STANDARD_COUNTERPOINT_CANTUS_IN_LOWER, P(C, 0, 3), NULL};
    StandardCounterpointResult result;

    assert(standard_counterpoint_check(&input, &result));
    assert(result.hard_violations & BIT(STANDARD_COUNTERPOINT_GROUP_CANTUS_FIRMUS));

    input.tonic = NULL;
    assert(standard_counterpoint_check(&input, &result));
    assert(!(result.hard_violations & BIT(STANDARD_COUNTERPOINT_GROUP_CANTUS_FIRMUS)));
}

static void test_cantus_part_selects_line(void) {
    LOWER(lower_notes);
    UPPER(upper_notes);
    StandardCounterpointLine lower = LINE_OF(lower_notes);
    StandardCounterpointLine upper = LINE_OF(upper_notes);
    StandardCounterpointPair pair = {&upper, &lower};
    StandardCounterpointInput input = {&pair, STANDARD_COUNTERPOINT_CANTUS_IN_LOWER, P(D, 0, 3), NULL};
    StandardCounterpointResult result;

    assert(standard_counterpoint_check(&input, &result));
    assert(!(result.hard_violations & BIT(STANDARD_COUNTERPOINT_GROUP_CANTUS_FIRMUS)));

    /* The upper part begins on A, not the tonic, so it is no cantus firmus. */
    input.cantus_part = STANDARD_COUNTERPOINT_CANTUS_IN_UPPER;
    assert(standard_counterpoint_check(&input, &result));
    assert(result.hard_violations & BIT(STANDARD_COUNTERPOINT_GROUP_CANTUS_FIRMUS));
}

static void test_melodic_violation(void) {
    /* The upper part leaps a major seventh. */
    const PitchSpelling *lower_notes[] = {P(C, 0, 3), P(D, 0, 3), P(C, 0, 3)};
    const PitchSpelling *upper_notes[] = {P(C, 0, 4), P(B, 0, 4), P(C, 0, 4)};
    StandardCounterpointLine lower = LINE_OF(lower_notes);
    StandardCounterpointLine upper = LINE_OF(upper_notes);
    StandardCounterpointPair pair = {&upper, &lower};
    StandardCounterpointInput input = {&pair, STANDARD_COUNTERPOINT_CANTUS_IN_LOWER, NULL, NULL};
    StandardCounterpointResult result;

    assert(standard_counterpoint_check(&input, &result));
    assert(result.hard_violations & BIT(STANDARD_COUNTERPOINT_GROUP_MELODIC));
    assert(!standard_counterpoint_valid(&input));
}

static void test_invalid_input(void) {
    LOWER(lower_notes);
    const PitchSpelling *short_notes[] = {P(D, 0, 4), P(E, 0, 4)};
    StandardCounterpointLine lower = LINE_OF(lower_notes);
    StandardCounterpointLine short_upper = LINE_OF(short_notes);
    StandardCounterpointPair mismatched = {&short_upper, &lower};
    StandardCounterpointPair no_upper = {NULL, &lower};
    StandardCounterpointInput input = {&mismatched, STANDARD_COUNTERPOINT_CANTUS_IN_LOWER, P(D, 0, 3), NULL};
    StandardCounterpointResult result = {7, 7};

    assert(!standard_counterpoint_check(NULL, &result));
    assert(!standard_counterpoint_check(&input, NULL));
    assert(!standard_counterpoint_check(&input, &result));
    input.texture = &no_upper;
    assert(!standard_counterpoint_check(&input, &result));
    input.texture = NULL;
    assert(!standard_counterpoint_check(&input, &result));
    input.texture = &mismatched;
    input.cantus_part = (StandardCounterpointCantusPart)5;
    assert(!standard_counterpoint_check(&input, &result));
    assert(result.hard_violations == 7 && result.soft_violations == 7);
    assert(!standard_counterpoint_valid(&input));
    assert(!standard_counterpoint_valid(NULL));
}

int main(void) {
    test_group_names();
    test_valid_exercise();
    test_harmonic_violation();
    test_cantus_firmus_violation();
    test_cantus_part_selects_line();
    test_melodic_violation();
    test_invalid_input();
    printf("test_counterpoint_rules (standard-counterpoint): ok\n");
    return 0;
}
