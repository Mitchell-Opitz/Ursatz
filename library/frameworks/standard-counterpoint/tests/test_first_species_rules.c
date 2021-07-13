#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "counterpoint_test_util.h"
#include "first_species_rules.h"
#include "first_species_test_util.h"

#define BIT(group) (1u << (group))
#define LOWER_PART STANDARD_COUNTERPOINT_CANTUS_IN_LOWER
#define UPPER_PART STANDARD_COUNTERPOINT_CANTUS_IN_UPPER

static void test_group_names(void) {
    assert(strcmp(standard_counterpoint_first_species_group_name(STANDARD_COUNTERPOINT_FIRST_SPECIES_GROUP_CANTUS_FIRMUS),
                  "cantus-firmus") == 0);
    assert(strcmp(standard_counterpoint_first_species_group_name(
                      STANDARD_COUNTERPOINT_FIRST_SPECIES_GROUP_COUNTERPOINT_LINE),
                  "counterpoint-line") == 0);
    assert(strcmp(standard_counterpoint_first_species_group_name(STANDARD_COUNTERPOINT_FIRST_SPECIES_GROUP_ENDS),
                  "ends") == 0);
    assert(strcmp(standard_counterpoint_first_species_group_name(STANDARD_COUNTERPOINT_FIRST_SPECIES_GROUP_MOTION),
                  "motion") == 0);
    assert(standard_counterpoint_first_species_group_name(STANDARD_COUNTERPOINT_FIRST_SPECIES_GROUP_COUNT) == NULL);
}

static void test_common_groups_keep_their_bits(void) {
    assert((int)STANDARD_COUNTERPOINT_FIRST_SPECIES_GROUP_CANTUS_FIRMUS == (int)STANDARD_COUNTERPOINT_GROUP_CANTUS_FIRMUS);
    assert((int)STANDARD_COUNTERPOINT_FIRST_SPECIES_GROUP_MELODIC == (int)STANDARD_COUNTERPOINT_GROUP_MELODIC);
    assert((int)STANDARD_COUNTERPOINT_FIRST_SPECIES_GROUP_HARMONIC == (int)STANDARD_COUNTERPOINT_GROUP_HARMONIC);
    assert((int)STANDARD_COUNTERPOINT_FIRST_SPECIES_GROUP_TENDENCY == (int)STANDARD_COUNTERPOINT_GROUP_TENDENCY);
}

static void test_valid_exercise(void) {
    FIRST_SPECIES_LOWER(lower_notes);
    FIRST_SPECIES_UPPER(upper_notes);
    StandardCounterpointLine lower = LINE_OF(lower_notes);
    StandardCounterpointLine upper = LINE_OF(upper_notes);
    StandardCounterpointPair pair = {&upper, &lower};
    StandardCounterpointInput input = {&pair, LOWER_PART, P(D, 0, 3), NULL};
    StandardCounterpointResult result = {99, 99};
    StandardCounterpointResult common = {0, 0};

    assert(standard_counterpoint_first_species_check(&input, &result));
    assert(result.hard_violations == 0);
    assert(standard_counterpoint_first_species_valid(&input));

    /* The first four groups report what the common check reports. */
    assert(standard_counterpoint_check(&input, &common));
    unsigned common_bits = BIT(STANDARD_COUNTERPOINT_GROUP_COUNT) - 1;
    assert((result.hard_violations & common_bits) == common.hard_violations);
    assert((result.soft_violations & common_bits) == common.soft_violations);
}

static void test_harmony_violation(void) {
    /* Parallel octaves. With no tonic, the groups that need one are skipped. */
    const PitchSpelling *lower_notes[] = {P(C, 0, 3), P(D, 0, 3)};
    const PitchSpelling *upper_notes[] = {P(C, 0, 4), P(D, 0, 4)};
    StandardCounterpointLine lower = LINE_OF(lower_notes);
    StandardCounterpointLine upper = LINE_OF(upper_notes);
    StandardCounterpointPair pair = {&upper, &lower};
    StandardCounterpointInput input = {&pair, LOWER_PART, NULL, NULL};
    StandardCounterpointResult result;

    assert(standard_counterpoint_first_species_check(&input, &result));
    assert(result.hard_violations & BIT(STANDARD_COUNTERPOINT_FIRST_SPECIES_GROUP_HARMONIC));
    assert(result.hard_violations & BIT(STANDARD_COUNTERPOINT_FIRST_SPECIES_GROUP_HARMONY));
    assert(!(result.hard_violations & BIT(STANDARD_COUNTERPOINT_FIRST_SPECIES_GROUP_CANTUS_FIRMUS)));
    assert(!(result.hard_violations & BIT(STANDARD_COUNTERPOINT_FIRST_SPECIES_GROUP_COUNTERPOINT_LINE)));
    assert(!(result.hard_violations & BIT(STANDARD_COUNTERPOINT_FIRST_SPECIES_GROUP_ENDS)));
    assert(result.soft_violations & BIT(STANDARD_COUNTERPOINT_FIRST_SPECIES_GROUP_HARMONY));
    assert(!standard_counterpoint_first_species_valid(&input));
}

static void test_ending_violation(void) {
    /* The same exercise, but the counterpoint ends on E4 instead of D4. */
    FIRST_SPECIES_LOWER(lower_notes);
    const PitchSpelling *upper_notes[] = {P(A, 0, 3), P(D, 0, 4), P(C, 0, 4), P(F, 0, 4), P(E, 0, 4), P(D, 0, 4),
                                          P(C, 0, 4), P(E, 0, 4), P(D, 0, 4), P(C, 1, 4), P(E, 0, 4)};
    StandardCounterpointLine lower = LINE_OF(lower_notes);
    StandardCounterpointLine upper = LINE_OF(upper_notes);
    StandardCounterpointPair pair = {&upper, &lower};
    StandardCounterpointInput input = {&pair, LOWER_PART, P(D, 0, 3), NULL};
    StandardCounterpointResult result;

    assert(standard_counterpoint_first_species_check(&input, &result));
    assert(result.hard_violations & BIT(STANDARD_COUNTERPOINT_FIRST_SPECIES_GROUP_ENDS));
    assert(!standard_counterpoint_first_species_valid(&input));
}

static void test_cantus_part_selects_lines(void) {
    FIRST_SPECIES_LOWER(lower_notes);
    FIRST_SPECIES_UPPER(upper_notes);
    StandardCounterpointLine lower = LINE_OF(lower_notes);
    StandardCounterpointLine upper = LINE_OF(upper_notes);
    StandardCounterpointPair pair = {&upper, &lower};
    StandardCounterpointInput input = {&pair, UPPER_PART, P(D, 0, 3), NULL};
    StandardCounterpointResult result;

    /* With the upper part as the cantus firmus, it must begin on the tonic;
       it begins on A. */
    assert(standard_counterpoint_first_species_check(&input, &result));
    assert(result.hard_violations & BIT(STANDARD_COUNTERPOINT_FIRST_SPECIES_GROUP_CANTUS_FIRMUS));
    assert(!standard_counterpoint_first_species_valid(&input));
}

static void test_no_tonic_skips_groups_that_need_it(void) {
    FIRST_SPECIES_LOWER(lower_notes);
    FIRST_SPECIES_UPPER(upper_notes);
    StandardCounterpointLine lower = LINE_OF(lower_notes);
    StandardCounterpointLine upper = LINE_OF(upper_notes);
    StandardCounterpointPair pair = {&upper, &lower};
    StandardCounterpointInput input = {&pair, LOWER_PART, NULL, NULL};
    StandardCounterpointResult result;
    unsigned skipped = BIT(STANDARD_COUNTERPOINT_FIRST_SPECIES_GROUP_CANTUS_FIRMUS) |
                       BIT(STANDARD_COUNTERPOINT_FIRST_SPECIES_GROUP_COUNTERPOINT_LINE) |
                       BIT(STANDARD_COUNTERPOINT_FIRST_SPECIES_GROUP_ENDS);

    assert(standard_counterpoint_first_species_check(&input, &result));
    assert(!(result.hard_violations & skipped));
    assert(!(result.soft_violations & skipped));
}

static void test_invalid_input(void) {
    FIRST_SPECIES_LOWER(lower_notes);
    const PitchSpelling *short_notes[] = {P(A, 0, 3), P(D, 0, 4)};
    StandardCounterpointLine lower = LINE_OF(lower_notes);
    StandardCounterpointLine short_upper = LINE_OF(short_notes);
    StandardCounterpointPair mismatched = {&short_upper, &lower};
    StandardCounterpointInput input = {&mismatched, LOWER_PART, P(D, 0, 3), NULL};
    StandardCounterpointResult result = {7, 7};

    assert(!standard_counterpoint_first_species_check(NULL, &result));
    assert(!standard_counterpoint_first_species_check(&input, NULL));
    assert(!standard_counterpoint_first_species_check(&input, &result));
    input.cantus_part = (StandardCounterpointCantusPart)5;
    assert(!standard_counterpoint_first_species_check(&input, &result));
    assert(result.hard_violations == 7 && result.soft_violations == 7);
    assert(!standard_counterpoint_first_species_valid(&input));
    assert(!standard_counterpoint_first_species_valid(NULL));
}

int main(void) {
    test_group_names();
    test_common_groups_keep_their_bits();
    test_valid_exercise();
    test_harmony_violation();
    test_ending_violation();
    test_cantus_part_selects_lines();
    test_no_tonic_skips_groups_that_need_it();
    test_invalid_input();
    printf("test_first_species_rules (standard-counterpoint): ok\n");
    return 0;
}
