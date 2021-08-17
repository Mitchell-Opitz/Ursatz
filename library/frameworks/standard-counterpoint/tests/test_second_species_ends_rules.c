#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "rule_check.h"
#include "second_species_ends_rules.h"
#include "second_species_test_util.h"

#define ENDS(name) STANDARD_COUNTERPOINT_SECOND_SPECIES_ENDS_##name

static bool holds(StandardCounterpointSecondSpeciesEndsRule rule, const StandardCounterpointSecondSpeciesExercise *ex) {
    return standard_counterpoint_second_species_ends_rule_holds(rule, ex, P(D, 0, 4));
}

static void test_rules_reference_framework(void) {
    CHECK_RULES_REFERENCE_FRAMEWORK(standard_counterpoint_second_species_ends_rule,
                                    standard_counterpoint_second_species_ends_constraint,
                                    STANDARD_COUNTERPOINT_SECOND_SPECIES_ENDS_COUNT);
    for (int i = 0; i < STANDARD_COUNTERPOINT_SECOND_SPECIES_ENDS_COUNT; i++) {
        assert(constraint_type(standard_counterpoint_second_species_ends_constraint(
                   (StandardCounterpointSecondSpeciesEndsRule)i)) == CONSTRAINT_TYPE_HARD);
    }
}

static void test_names(void) {
    assert(strcmp(standard_counterpoint_second_species_ends_rule_name(ENDS(BEGIN)), "begin") == 0);
    assert(strcmp(standard_counterpoint_second_species_ends_rule_name(ENDS(CLAUSULA_VERA)), "clausula-vera") == 0);
    assert(standard_counterpoint_second_species_ends_rule_name(ENDS(COUNT)) == NULL);
}

static void test_good_ending(void) {
    const PitchSpelling *cantus[] = {P(D, 0, 3), P(E, 0, 3), P(D, 0, 3)};
    const StandardCounterpointSlot slots[] = {ATK(P(A, 0, 3)), ATK(P(B, 0, 3)), ATK(P(A, 0, 3)),
                                              ATK(P(C, 1, 4)), ATK(P(D, 0, 4)), TIE(P(D, 0, 4))};
    EXERCISE(exercise, cantus, slots, CANTUS_LOWER);
    unsigned violations = 99;
    assert(standard_counterpoint_second_species_ends_check(&exercise, P(D, 0, 3), &violations));
    assert(violations == 0);
}

static void test_begin(void) {
    const PitchSpelling *cantus[] = {P(D, 0, 3), P(E, 0, 3), P(D, 0, 3)};
    const StandardCounterpointSlot on_do[] = {ATK(P(D, 0, 4)), ATK(P(B, 0, 3)), ATK(P(A, 0, 3)),
                                              ATK(P(C, 1, 4)), ATK(P(D, 0, 4)), TIE(P(D, 0, 4))};
    const StandardCounterpointSlot on_sol[] = {ATK(P(A, 0, 3)), ATK(P(B, 0, 3)), ATK(P(A, 0, 3)),
                                               ATK(P(C, 1, 4)), ATK(P(D, 0, 4)), TIE(P(D, 0, 4))};
    const StandardCounterpointSlot on_mi[] = {ATK(P(F, 0, 4)), ATK(P(B, 0, 3)), ATK(P(A, 0, 3)),
                                              ATK(P(C, 1, 4)), ATK(P(D, 0, 4)), TIE(P(D, 0, 4))};
    const StandardCounterpointSlot after_rest[] = {REST(), ATK(P(D, 0, 4)), ATK(P(A, 0, 3)),
                                                   ATK(P(C, 1, 4)), ATK(P(D, 0, 4)), TIE(P(D, 0, 4))};
    EXERCISE(do_exercise, cantus, on_do, CANTUS_LOWER);
    EXERCISE(sol_exercise, cantus, on_sol, CANTUS_LOWER);
    EXERCISE(mi_exercise, cantus, on_mi, CANTUS_LOWER);
    EXERCISE(rest_exercise, cantus, after_rest, CANTUS_LOWER);
    assert(holds(ENDS(BEGIN), &do_exercise));
    assert(holds(ENDS(BEGIN), &sol_exercise));
    assert(!holds(ENDS(BEGIN), &mi_exercise));
    /* After a half rest, the first note is the first one that sounds. */
    assert(holds(ENDS(BEGIN), &rest_exercise));

    /* Below the cantus firmus, only do. */
    const PitchSpelling *upper_cantus[] = {P(D, 0, 4), P(E, 0, 4), P(D, 0, 4)};
    const StandardCounterpointSlot below_do[] = {ATK(P(D, 0, 3)), ATK(P(B, 0, 2)), ATK(P(A, 0, 2)),
                                                 ATK(P(C, 1, 3)), ATK(P(D, 0, 3)), TIE(P(D, 0, 3))};
    const StandardCounterpointSlot below_sol[] = {ATK(P(A, 0, 2)), ATK(P(B, 0, 2)), ATK(P(A, 0, 2)),
                                                  ATK(P(C, 1, 3)), ATK(P(D, 0, 3)), TIE(P(D, 0, 3))};
    EXERCISE(below_do_exercise, upper_cantus, below_do, CANTUS_UPPER);
    EXERCISE(below_sol_exercise, upper_cantus, below_sol, CANTUS_UPPER);
    assert(holds(ENDS(BEGIN), &below_do_exercise));
    assert(!holds(ENDS(BEGIN), &below_sol_exercise));
}

static void test_opening_bar(void) {
    const PitchSpelling *cantus[] = {P(D, 0, 3), P(E, 0, 3), P(D, 0, 3)};
    const StandardCounterpointSlot halves[] = {ATK(P(A, 0, 3)), ATK(P(B, 0, 3)), ATK(P(A, 0, 3)),
                                               ATK(P(C, 1, 4)), ATK(P(D, 0, 4)), TIE(P(D, 0, 4))};
    const StandardCounterpointSlot rest[] = {REST(), ATK(P(A, 0, 3)), ATK(P(A, 0, 3)),
                                             ATK(P(C, 1, 4)), ATK(P(D, 0, 4)), TIE(P(D, 0, 4))};
    const StandardCounterpointSlot whole[] = {ATK(P(A, 0, 3)), TIE(P(A, 0, 3)), ATK(P(A, 0, 3)),
                                              ATK(P(C, 1, 4)), ATK(P(D, 0, 4)), TIE(P(D, 0, 4))};
    EXERCISE(halves_exercise, cantus, halves, CANTUS_LOWER);
    EXERCISE(rest_exercise, cantus, rest, CANTUS_LOWER);
    EXERCISE(whole_exercise, cantus, whole, CANTUS_LOWER);
    assert(holds(ENDS(OPENING_BAR), &halves_exercise));
    assert(holds(ENDS(OPENING_BAR), &rest_exercise));
    assert(!holds(ENDS(OPENING_BAR), &whole_exercise));
}

static void test_final_bar(void) {
    const PitchSpelling *cantus[] = {P(D, 0, 3), P(E, 0, 3), P(D, 0, 3)};
    const StandardCounterpointSlot whole_do[] = {ATK(P(A, 0, 3)), ATK(P(B, 0, 3)), ATK(P(A, 0, 3)),
                                                 ATK(P(C, 1, 4)), ATK(P(D, 0, 4)), TIE(P(D, 0, 4))};
    const StandardCounterpointSlot two_halves[] = {ATK(P(A, 0, 3)), ATK(P(B, 0, 3)), ATK(P(A, 0, 3)),
                                                   ATK(P(C, 1, 4)), ATK(P(D, 0, 4)), ATK(P(D, 0, 4))};
    const StandardCounterpointSlot not_do[] = {ATK(P(A, 0, 3)), ATK(P(B, 0, 3)), ATK(P(A, 0, 3)),
                                               ATK(P(C, 1, 4)), ATK(P(E, 0, 4)), TIE(P(E, 0, 4))};
    const StandardCounterpointSlot low_do[] = {ATK(P(A, 0, 3)), ATK(P(B, 0, 3)), ATK(P(A, 0, 3)),
                                               ATK(P(C, 1, 4)), ATK(P(D, 0, 5)), TIE(P(D, 0, 5))};
    EXERCISE(whole_exercise, cantus, whole_do, CANTUS_LOWER);
    EXERCISE(halves_exercise, cantus, two_halves, CANTUS_LOWER);
    EXERCISE(not_do_exercise, cantus, not_do, CANTUS_LOWER);
    EXERCISE(octave_exercise, cantus, low_do, CANTUS_LOWER);
    assert(holds(ENDS(FINAL_BAR), &whole_exercise));
    assert(!holds(ENDS(FINAL_BAR), &halves_exercise));
    assert(!holds(ENDS(FINAL_BAR), &not_do_exercise));
    /* Any octave of do will do. */
    assert(holds(ENDS(FINAL_BAR), &octave_exercise));
}

static void test_clausula_vera(void) {
    const PitchSpelling *cantus[] = {P(D, 0, 3), P(E, 0, 3), P(D, 0, 3)};

    /* Two half notes in the penultimate bar: the weak beat is the leading
       tone, against the cantus firmus' re. */
    const StandardCounterpointSlot halves[] = {ATK(P(A, 0, 3)), ATK(P(B, 0, 3)), ATK(P(A, 0, 3)),
                                               ATK(P(C, 1, 4)), ATK(P(D, 0, 4)), TIE(P(D, 0, 4))};
    EXERCISE(halves_exercise, cantus, halves, CANTUS_LOWER);
    assert(holds(ENDS(CLAUSULA_VERA), &halves_exercise));

    /* A whole note in the penultimate bar, as in first species. */
    const StandardCounterpointSlot whole[] = {ATK(P(A, 0, 3)), ATK(P(B, 0, 3)), ATK(P(C, 1, 4)),
                                              TIE(P(C, 1, 4)), ATK(P(D, 0, 4)), TIE(P(D, 0, 4))};
    EXERCISE(whole_exercise, cantus, whole, CANTUS_LOWER);
    assert(holds(ENDS(CLAUSULA_VERA), &whole_exercise));

    /* Approached by leap. */
    const StandardCounterpointSlot leap[] = {ATK(P(A, 0, 3)), ATK(P(B, 0, 3)), ATK(P(A, 0, 3)),
                                             ATK(P(G, 0, 3)), ATK(P(D, 0, 4)), TIE(P(D, 0, 4))};
    EXERCISE(leap_exercise, cantus, leap, CANTUS_LOWER);
    assert(!holds(ENDS(CLAUSULA_VERA), &leap_exercise));

    /* The same direction as the cantus firmus: both parts descend. */
    const StandardCounterpointSlot similar[] = {ATK(P(A, 0, 3)), ATK(P(B, 0, 3)), ATK(P(A, 0, 3)),
                                                ATK(P(E, 0, 4)), ATK(P(D, 0, 4)), TIE(P(D, 0, 4))};
    EXERCISE(similar_exercise, cantus, similar, CANTUS_LOWER);
    assert(!holds(ENDS(CLAUSULA_VERA), &similar_exercise));

    /* The cantus firmus does not approach its last note by step. */
    const PitchSpelling *leaping_cantus[] = {P(D, 0, 3), P(G, 0, 3), P(D, 0, 3)};
    EXERCISE(leaping_exercise, leaping_cantus, halves, CANTUS_LOWER);
    assert(!holds(ENDS(CLAUSULA_VERA), &leaping_exercise));

    /* The cantus firmus rises to its last note: re is the counterpoint's
       penultimate, a tenth (a third) against the cantus firmus' ti. */
    const PitchSpelling *rising_cantus[] = {P(D, 0, 3), P(C, 1, 3), P(D, 0, 3)};
    const StandardCounterpointSlot from_above[] = {ATK(P(A, 0, 3)), ATK(P(B, 0, 3)), ATK(P(A, 0, 3)),
                                                   ATK(P(E, 0, 4)), ATK(P(D, 0, 4)), TIE(P(D, 0, 4))};
    EXERCISE(rising_exercise, rising_cantus, from_above, CANTUS_LOWER);
    assert(holds(ENDS(CLAUSULA_VERA), &rising_exercise));
}

static void test_check_masks(void) {
    /* Begins on a mi, has a whole note first bar, ends on re with two halves,
       and leaps into it. */
    const PitchSpelling *cantus[] = {P(D, 0, 3), P(E, 0, 3), P(D, 0, 3)};
    const StandardCounterpointSlot slots[] = {ATK(P(F, 0, 4)), TIE(P(F, 0, 4)), ATK(P(A, 0, 3)),
                                              ATK(P(G, 0, 3)), ATK(P(E, 0, 4)), ATK(P(E, 0, 4))};
    EXERCISE(exercise, cantus, slots, CANTUS_LOWER);
    unsigned violations = 0;
    assert(standard_counterpoint_second_species_ends_check(&exercise, P(D, 0, 4), &violations));
    assert(violations == ((1u << ENDS(BEGIN)) | (1u << ENDS(OPENING_BAR)) | (1u << ENDS(FINAL_BAR)) |
                          (1u << ENDS(CLAUSULA_VERA))));
}

static void test_invalid_input(void) {
    const PitchSpelling *cantus[] = {P(D, 0, 3), P(E, 0, 3)};
    const StandardCounterpointSlot too_few[] = {ATK(P(A, 0, 3))};
    const StandardCounterpointSlot slots[] = {ATK(P(A, 0, 3)), ATK(P(B, 0, 3)), ATK(P(C, 1, 4)), TIE(P(C, 1, 4))};
    EXERCISE(invalid, cantus, too_few, CANTUS_LOWER);
    EXERCISE(valid, cantus, slots, CANTUS_LOWER);
    unsigned violations = 5;

    assert(!standard_counterpoint_second_species_ends_check(NULL, P(D, 0, 4), &violations));
    assert(!standard_counterpoint_second_species_ends_check(&invalid, P(D, 0, 4), &violations));
    assert(!standard_counterpoint_second_species_ends_check(&valid, NULL, &violations));
    assert(!standard_counterpoint_second_species_ends_check(&valid, P(D, 0, 4), NULL));
    assert(violations == 5);
    assert(!standard_counterpoint_second_species_ends_rule_holds(ENDS(COUNT), &valid, P(D, 0, 4)));
}

int main(void) {
    test_rules_reference_framework();
    test_names();
    test_good_ending();
    test_begin();
    test_opening_bar();
    test_final_bar();
    test_clausula_vera();
    test_check_masks();
    test_invalid_input();
    printf("test_second_species_ends_rules (standard-counterpoint): ok\n");
    return 0;
}
