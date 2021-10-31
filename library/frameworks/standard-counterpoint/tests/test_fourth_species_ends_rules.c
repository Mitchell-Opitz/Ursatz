#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "fourth_species_ends_rules.h"
#include "fourth_species_test_util.h"
#include "rule_check.h"

#define ENDS(name) STANDARD_COUNTERPOINT_FOURTH_SPECIES_ENDS_##name

static bool holds(StandardCounterpointFourthSpeciesEndsRule rule,
                  const StandardCounterpointFourthSpeciesExercise *exercise) {
    return standard_counterpoint_fourth_species_ends_rule_holds(rule, exercise, P(D, 0, 3));
}

static void test_rules_reference_framework(void) {
    CHECK_RULES_REFERENCE_FRAMEWORK(standard_counterpoint_fourth_species_ends_rule,
                                    standard_counterpoint_fourth_species_ends_constraint,
                                    STANDARD_COUNTERPOINT_FOURTH_SPECIES_ENDS_COUNT);
    for (int i = 0; i < STANDARD_COUNTERPOINT_FOURTH_SPECIES_ENDS_COUNT; i++) {
        assert(constraint_type(standard_counterpoint_fourth_species_ends_constraint(
                   (StandardCounterpointFourthSpeciesEndsRule)i)) == CONSTRAINT_TYPE_HARD);
    }
}

static void test_names(void) {
    assert(strcmp(standard_counterpoint_fourth_species_ends_rule_name(ENDS(BEGIN)), "begin") == 0);
    assert(strcmp(standard_counterpoint_fourth_species_ends_rule_name(ENDS(CANTUS_ENDS_RE_DO)), "cantus-ends-re-do") ==
           0);
    assert(strcmp(standard_counterpoint_fourth_species_ends_rule_name(ENDS(PENULTIMATE_SUSPENSION)),
                  "penultimate-suspension") == 0);
    assert(strcmp(standard_counterpoint_fourth_species_ends_rule_name(ENDS(FINAL_BAR)), "final-bar") == 0);
    assert(standard_counterpoint_fourth_species_ends_rule_name(ENDS(COUNT)) == NULL);
}

static void test_good_endings(void) {
    unsigned violations = 99;

    GOOD_FOURTH_SPECIES_CANTUS(good_cantus);
    GOOD_FOURTH_SPECIES_SLOTS(good_slots);
    EXERCISE(good, good_cantus, good_slots, CANTUS_LOWER);
    assert(standard_counterpoint_fourth_species_ends_check(&good, P(D, 0, 3), &violations));
    assert(violations == 0);

    /* Below the cantus firmus, a 2-3 suspension. */
    const PitchSpelling *cantus[] = {P(D, 0, 4), P(E, 0, 4), P(D, 0, 4)};
    const StandardCounterpointSlot below[] = {REST(), ATK(P(D, 0, 3)), TIE(P(D, 0, 3)), ATK(P(C, 1, 3)),
                                              WHOLE(P(D, 0, 3))};
    EXERCISE(below_exercise, cantus, below, CANTUS_UPPER);
    violations = 99;
    assert(standard_counterpoint_fourth_species_ends_check(&below_exercise, P(D, 0, 3), &violations));
    assert(violations == 0);
}

static void test_begin(void) {
    const PitchSpelling *cantus[] = {P(D, 0, 3), P(E, 0, 3), P(D, 0, 3)};
    const StandardCounterpointSlot on_do[] = {REST(), ATK(P(D, 0, 4)), TIE(P(D, 0, 4)), ATK(P(C, 1, 4)),
                                              WHOLE(P(D, 0, 4))};
    const StandardCounterpointSlot on_sol[] = {REST(), ATK(P(A, 0, 3)), TIE(P(A, 0, 3)), ATK(P(C, 1, 4)),
                                               WHOLE(P(D, 0, 4))};
    const StandardCounterpointSlot on_mi[] = {REST(), ATK(P(F, 0, 4)), TIE(P(F, 0, 4)), ATK(P(C, 1, 4)),
                                              WHOLE(P(D, 0, 4))};
    EXERCISE(do_exercise, cantus, on_do, CANTUS_LOWER);
    EXERCISE(sol_exercise, cantus, on_sol, CANTUS_LOWER);
    EXERCISE(mi_exercise, cantus, on_mi, CANTUS_LOWER);
    assert(holds(ENDS(BEGIN), &do_exercise));
    assert(holds(ENDS(BEGIN), &sol_exercise));
    assert(!holds(ENDS(BEGIN), &mi_exercise));

    /* Below the cantus firmus, only do. */
    const PitchSpelling *upper[] = {P(D, 0, 4), P(E, 0, 4), P(D, 0, 4)};
    const StandardCounterpointSlot below_do[] = {REST(), ATK(P(D, 0, 3)), TIE(P(D, 0, 3)), ATK(P(C, 1, 3)),
                                                 WHOLE(P(D, 0, 3))};
    const StandardCounterpointSlot below_sol[] = {REST(), ATK(P(A, 0, 2)), TIE(P(A, 0, 2)), ATK(P(C, 1, 3)),
                                                  WHOLE(P(D, 0, 3))};
    EXERCISE(below_do_exercise, upper, below_do, CANTUS_UPPER);
    EXERCISE(below_sol_exercise, upper, below_sol, CANTUS_UPPER);
    assert(holds(ENDS(BEGIN), &below_do_exercise));
    assert(!holds(ENDS(BEGIN), &below_sol_exercise));
}

static void test_cantus_ends_re_do(void) {
    const StandardCounterpointSlot slots[] = {REST(), ATK(P(D, 0, 4)), TIE(P(D, 0, 4)), ATK(P(C, 1, 4)),
                                              WHOLE(P(D, 0, 4))};

    const PitchSpelling *re_do[] = {P(D, 0, 3), P(E, 0, 3), P(D, 0, 3)};
    EXERCISE(re_do_exercise, re_do, slots, CANTUS_LOWER);
    assert(holds(ENDS(CANTUS_ENDS_RE_DO), &re_do_exercise));

    /* Mi to do. */
    const PitchSpelling *mi_do[] = {P(D, 0, 3), P(F, 0, 3), P(D, 0, 3)};
    EXERCISE(mi_do_exercise, mi_do, slots, CANTUS_LOWER);
    assert(!holds(ENDS(CANTUS_ENDS_RE_DO), &mi_do_exercise));

    /* Re to mi: does not end on do. */
    const PitchSpelling *not_do[] = {P(D, 0, 3), P(E, 0, 3), P(F, 0, 3)};
    EXERCISE(not_do_exercise, not_do, slots, CANTUS_LOWER);
    assert(!holds(ENDS(CANTUS_ENDS_RE_DO), &not_do_exercise));
}

static void test_penultimate_suspension(void) {
    const PitchSpelling *cantus[] = {P(D, 0, 3), P(E, 0, 3), P(D, 0, 3)};

    /* Do, then a tied suspension over re, then ti. */
    const StandardCounterpointSlot good[] = {REST(), ATK(P(D, 0, 4)), TIE(P(D, 0, 4)), ATK(P(C, 1, 4)),
                                             WHOLE(P(D, 0, 4))};
    EXERCISE(good_exercise, cantus, good, CANTUS_LOWER);
    assert(holds(ENDS(PENULTIMATE_SUSPENSION), &good_exercise));

    /* The resolution is not the leading tone. */
    const StandardCounterpointSlot whole_step[] = {REST(), ATK(P(D, 0, 4)), TIE(P(D, 0, 4)), ATK(P(C, 0, 4)),
                                                   WHOLE(P(D, 0, 4))};
    EXERCISE(whole_step_exercise, cantus, whole_step, CANTUS_LOWER);
    assert(!holds(ENDS(PENULTIMATE_SUSPENSION), &whole_step_exercise));

    /* A tied consonance is not a suspension. */
    const StandardCounterpointSlot consonant[] = {REST(), ATK(P(A, 0, 3)), TIE(P(A, 0, 3)), ATK(P(C, 1, 4)),
                                                  WHOLE(P(D, 0, 4))};
    EXERCISE(consonant_exercise, cantus, consonant, CANTUS_LOWER);
    assert(!holds(ENDS(PENULTIMATE_SUSPENSION), &consonant_exercise));

    /* A 7-6 on a note that is not do. */
    const PitchSpelling *g_cantus[] = {P(G, 0, 3), P(A, 0, 3), P(G, 0, 3)};
    const StandardCounterpointSlot on_sol[] = {REST(), ATK(P(G, 0, 4)), TIE(P(G, 0, 4)), ATK(P(F, 0, 4)),
                                               WHOLE(P(G, 0, 4))};
    EXERCISE(sol_exercise, g_cantus, on_sol, CANTUS_LOWER);
    assert(!holds(ENDS(PENULTIMATE_SUSPENSION), &sol_exercise));
}

static void test_final_bar(void) {
    const PitchSpelling *cantus[] = {P(D, 0, 3), P(E, 0, 3), P(D, 0, 3)};
    const StandardCounterpointSlot on_do[] = {REST(), ATK(P(D, 0, 4)), TIE(P(D, 0, 4)), ATK(P(C, 1, 4)),
                                              WHOLE(P(D, 0, 4))};
    const StandardCounterpointSlot on_mi[] = {REST(), ATK(P(D, 0, 4)), TIE(P(D, 0, 4)), ATK(P(C, 1, 4)),
                                              WHOLE(P(E, 0, 4))};
    EXERCISE(do_exercise, cantus, on_do, CANTUS_LOWER);
    EXERCISE(mi_exercise, cantus, on_mi, CANTUS_LOWER);
    assert(holds(ENDS(FINAL_BAR), &do_exercise));
    assert(!holds(ENDS(FINAL_BAR), &mi_exercise));
}

static void test_check_mask(void) {
    const PitchSpelling *cantus[] = {P(D, 0, 3), P(E, 0, 3), P(D, 0, 3)};
    const StandardCounterpointSlot slots[] = {REST(), ATK(P(D, 0, 4)), TIE(P(D, 0, 4)), ATK(P(C, 1, 4)),
                                              WHOLE(P(E, 0, 4))};
    EXERCISE(exercise, cantus, slots, CANTUS_LOWER);
    unsigned violations = 0;
    assert(standard_counterpoint_fourth_species_ends_check(&exercise, P(D, 0, 3), &violations));
    assert(violations == (1u << ENDS(FINAL_BAR)));
}

static void test_invalid_input(void) {
    const PitchSpelling *cantus[] = {P(D, 0, 3), P(E, 0, 3)};
    const StandardCounterpointSlot too_few[] = {WHOLE(P(E, 0, 3))};
    EXERCISE(invalid, cantus, too_few, CANTUS_LOWER);
    const PitchSpelling *good_cantus[] = {P(D, 0, 3), P(E, 0, 3), P(D, 0, 3)};
    const StandardCounterpointSlot good_slots[] = {REST(), ATK(P(D, 0, 4)), TIE(P(D, 0, 4)), ATK(P(C, 1, 4)),
                                                   WHOLE(P(D, 0, 4))};
    EXERCISE(good, good_cantus, good_slots, CANTUS_LOWER);
    unsigned violations = 5;

    assert(!standard_counterpoint_fourth_species_ends_check(NULL, P(D, 0, 3), &violations));
    assert(!standard_counterpoint_fourth_species_ends_check(&invalid, P(D, 0, 3), &violations));
    assert(!standard_counterpoint_fourth_species_ends_check(&good, NULL, &violations));
    assert(!standard_counterpoint_fourth_species_ends_check(&good, P(D, 0, 3), NULL));
    assert(violations == 5);
    assert(!standard_counterpoint_fourth_species_ends_rule_holds(ENDS(BEGIN), &good, NULL));
    assert(!standard_counterpoint_fourth_species_ends_rule_holds(ENDS(COUNT), &good, P(D, 0, 3)));
}

int main(void) {
    test_rules_reference_framework();
    test_names();
    test_good_endings();
    test_begin();
    test_cantus_ends_re_do();
    test_penultimate_suspension();
    test_final_bar();
    test_check_mask();
    test_invalid_input();
    printf("test_fourth_species_ends_rules (standard-counterpoint): ok\n");
    return 0;
}
