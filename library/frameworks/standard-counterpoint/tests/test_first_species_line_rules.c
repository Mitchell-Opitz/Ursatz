#include <assert.h>
#include <stdio.h>

#include "counterpoint_test_util.h"
#include "first_species_line_rules.h"
#include "first_species_test_util.h"

static void test_counterpoint_line_of_cantus_quality(void) {
    FIRST_SPECIES_UPPER(notes);
    StandardCounterpointLine line = LINE_OF(notes);
    unsigned hard = 99, soft = 99;
    assert(standard_counterpoint_first_species_line_check(&line, P(D, 0, 4), &hard, &soft));
    assert(hard == 0 && soft == 0);
}

static void test_tonic_start_is_excluded(void) {
    /* The counterpoint begins on sol, so the cantus firmus rule that a line
       begins and ends on the tonic fails for it, and is not applied. */
    FIRST_SPECIES_UPPER(notes);
    StandardCounterpointLine line = LINE_OF(notes);
    assert(!standard_counterpoint_cantus_rule_holds(STANDARD_COUNTERPOINT_CANTUS_RULE_TONIC_ENDS, &line, P(D, 0, 4)));
    assert(standard_counterpoint_first_species_line_rule_holds(STANDARD_COUNTERPOINT_CANTUS_RULE_TONIC_ENDS, &line,
                                                               P(D, 0, 4)));
    assert(STANDARD_COUNTERPOINT_FIRST_SPECIES_LINE_EXCLUDED == STANDARD_COUNTERPOINT_CANTUS_RULE_TONIC_ENDS);
}

static void test_other_cantus_rules_apply(void) {
    FIRST_SPECIES_UPPER(good);
    /* Too short, and ends with a leap into the last note. */
    const PitchSpelling *bad[] = {P(A, 0, 3), P(D, 0, 4), P(C, 0, 4), P(G, 0, 4)};
    StandardCounterpointLine good_line = LINE_OF(good);
    StandardCounterpointLine bad_line = LINE_OF(bad);
    unsigned hard = 0, soft = 0;

    assert(standard_counterpoint_first_species_line_check(&bad_line, P(D, 0, 4), &hard, &soft));
    assert(hard & (1u << STANDARD_COUNTERPOINT_CANTUS_RULE_LENGTH));
    assert(hard & (1u << STANDARD_COUNTERPOINT_CANTUS_RULE_FINAL_STEP));
    assert(!(hard & (1u << STANDARD_COUNTERPOINT_CANTUS_RULE_TONIC_ENDS)));
    assert(!standard_counterpoint_first_species_line_rule_holds(STANDARD_COUNTERPOINT_CANTUS_RULE_LENGTH, &bad_line,
                                                                P(D, 0, 4)));
    assert(standard_counterpoint_first_species_line_rule_holds(STANDARD_COUNTERPOINT_CANTUS_RULE_LENGTH, &good_line,
                                                               P(D, 0, 4)));
}

static void test_soft_rules_stay_soft(void) {
    /* The climax appears twice and the line is not mostly stepwise. */
    const PitchSpelling *notes[] = {P(D, 0, 4), P(A, 0, 4), P(D, 0, 4), P(A, 0, 4), P(D, 0, 4),
                                    P(A, 0, 4), P(D, 0, 4), P(A, 0, 4), P(D, 1, 4), P(E, 0, 4)};
    StandardCounterpointLine line = LINE_OF(notes);
    unsigned hard = 0, soft = 0;
    assert(standard_counterpoint_first_species_line_check(&line, P(D, 0, 4), &hard, &soft));
    assert(soft & (1u << STANDARD_COUNTERPOINT_CANTUS_RULE_SINGLE_CLIMAX));
    assert(soft & (1u << STANDARD_COUNTERPOINT_CANTUS_RULE_MOSTLY_STEPWISE));
    assert(!(hard & (1u << STANDARD_COUNTERPOINT_CANTUS_RULE_SINGLE_CLIMAX)));
}

static void test_invalid_input(void) {
    FIRST_SPECIES_UPPER(notes);
    StandardCounterpointLine line = LINE_OF(notes);
    StandardCounterpointLine empty = {notes, 0};
    unsigned hard = 5, soft = 5;

    assert(!standard_counterpoint_first_species_line_check(NULL, P(D, 0, 4), &hard, &soft));
    assert(!standard_counterpoint_first_species_line_check(&empty, P(D, 0, 4), &hard, &soft));
    assert(!standard_counterpoint_first_species_line_check(&line, NULL, &hard, &soft));
    assert(!standard_counterpoint_first_species_line_check(&line, P(D, 0, 4), NULL, &soft));
    assert(!standard_counterpoint_first_species_line_check(&line, P(D, 0, 4), &hard, NULL));
    assert(hard == 5 && soft == 5);
    assert(!standard_counterpoint_first_species_line_rule_holds(STANDARD_COUNTERPOINT_CANTUS_RULE_COUNT, &line,
                                                                P(D, 0, 4)));
    assert(!standard_counterpoint_first_species_line_rule_holds(STANDARD_COUNTERPOINT_FIRST_SPECIES_LINE_EXCLUDED,
                                                                NULL, P(D, 0, 4)));
}

int main(void) {
    test_counterpoint_line_of_cantus_quality();
    test_tonic_start_is_excluded();
    test_other_cantus_rules_apply();
    test_soft_rules_stay_soft();
    test_invalid_input();
    printf("test_first_species_line_rules (standard-counterpoint): ok\n");
    return 0;
}
