#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "rule_check.h"
#include "third_species_ends_rules.h"
#include "third_species_test_util.h"

#define ENDS(name) STANDARD_COUNTERPOINT_THIRD_SPECIES_ENDS_##name

/* Four quarter notes. */
#define BAR(a, b, c, d) ATK(a), ATK(b), ATK(c), ATK(d)

static bool holds(StandardCounterpointThirdSpeciesEndsRule rule, const StandardCounterpointThirdSpeciesExercise *ex) {
    return standard_counterpoint_third_species_ends_rule_holds(rule, ex, P(D, 0, 4));
}

static void test_rules_reference_framework(void) {
    CHECK_RULES_REFERENCE_FRAMEWORK(standard_counterpoint_third_species_ends_rule,
                                    standard_counterpoint_third_species_ends_constraint,
                                    STANDARD_COUNTERPOINT_THIRD_SPECIES_ENDS_COUNT);
    for (int i = 0; i < STANDARD_COUNTERPOINT_THIRD_SPECIES_ENDS_COUNT; i++) {
        assert(constraint_type(standard_counterpoint_third_species_ends_constraint(
                   (StandardCounterpointThirdSpeciesEndsRule)i)) == CONSTRAINT_TYPE_HARD);
    }
}

static void test_names(void) {
    assert(strcmp(standard_counterpoint_third_species_ends_rule_name(ENDS(BEGIN)), "begin") == 0);
    assert(strcmp(standard_counterpoint_third_species_ends_rule_name(ENDS(CLAUSULA_VERA)), "clausula-vera") == 0);
    assert(standard_counterpoint_third_species_ends_rule_name(ENDS(COUNT)) == NULL);
}

static void test_good_ending(void) {
    const PitchSpelling *cantus[] = {P(D, 0, 3), P(E, 0, 3), P(D, 0, 3)};
    const StandardCounterpointSlot slots[] = {BAR(P(A, 0, 3), P(B, 0, 3), P(C, 0, 4), P(B, 0, 3)),
                                              BAR(P(A, 0, 3), P(B, 0, 3), P(A, 0, 3), P(C, 1, 4)),
                                              WHOLE(P(D, 0, 4))};
    EXERCISE(exercise, cantus, slots, CANTUS_LOWER);
    unsigned violations = 99;
    assert(standard_counterpoint_third_species_ends_check(&exercise, P(D, 0, 3), &violations));
    assert(violations == 0);
}

static void test_begin(void) {
    const PitchSpelling *cantus[] = {P(D, 0, 3), P(E, 0, 3), P(D, 0, 3)};
    const StandardCounterpointSlot on_do[] = {BAR(P(D, 0, 4), P(B, 0, 3), P(A, 0, 3), P(B, 0, 3)),
                                              BAR(P(A, 0, 3), P(B, 0, 3), P(A, 0, 3), P(C, 1, 4)), WHOLE(P(D, 0, 4))};
    const StandardCounterpointSlot on_sol[] = {BAR(P(A, 0, 3), P(B, 0, 3), P(C, 0, 4), P(B, 0, 3)),
                                               BAR(P(A, 0, 3), P(B, 0, 3), P(A, 0, 3), P(C, 1, 4)), WHOLE(P(D, 0, 4))};
    const StandardCounterpointSlot on_mi[] = {BAR(P(F, 0, 4), P(E, 0, 4), P(D, 0, 4), P(C, 0, 4)),
                                              BAR(P(A, 0, 3), P(B, 0, 3), P(A, 0, 3), P(C, 1, 4)), WHOLE(P(D, 0, 4))};
    const StandardCounterpointSlot after_rest[] = {REST(), ATK(P(D, 0, 4)), ATK(P(C, 0, 4)), ATK(P(B, 0, 3)),
                                                   BAR(P(A, 0, 3), P(B, 0, 3), P(A, 0, 3), P(C, 1, 4)), WHOLE(P(D, 0, 4))};
    EXERCISE(do_exercise, cantus, on_do, CANTUS_LOWER);
    EXERCISE(sol_exercise, cantus, on_sol, CANTUS_LOWER);
    EXERCISE(mi_exercise, cantus, on_mi, CANTUS_LOWER);
    EXERCISE(rest_exercise, cantus, after_rest, CANTUS_LOWER);
    assert(holds(ENDS(BEGIN), &do_exercise));
    assert(holds(ENDS(BEGIN), &sol_exercise));
    assert(!holds(ENDS(BEGIN), &mi_exercise));
    /* After a quarter rest, the first note is the first one that sounds. */
    assert(holds(ENDS(BEGIN), &rest_exercise));

    /* Below the cantus firmus, only do. */
    const PitchSpelling *upper_cantus[] = {P(D, 0, 4), P(E, 0, 4), P(D, 0, 4)};
    const StandardCounterpointSlot below_do[] = {BAR(P(D, 0, 3), P(B, 0, 2), P(A, 0, 2), P(B, 0, 2)),
                                                 BAR(P(A, 0, 2), P(B, 0, 2), P(A, 0, 2), P(C, 1, 3)), WHOLE(P(D, 0, 3))};
    const StandardCounterpointSlot below_sol[] = {BAR(P(A, 0, 2), P(B, 0, 2), P(C, 0, 3), P(B, 0, 2)),
                                                  BAR(P(A, 0, 2), P(B, 0, 2), P(A, 0, 2), P(C, 1, 3)), WHOLE(P(D, 0, 3))};
    EXERCISE(below_do_exercise, upper_cantus, below_do, CANTUS_UPPER);
    EXERCISE(below_sol_exercise, upper_cantus, below_sol, CANTUS_UPPER);
    assert(holds(ENDS(BEGIN), &below_do_exercise));
    assert(!holds(ENDS(BEGIN), &below_sol_exercise));
}

static void test_opening_bar(void) {
    const PitchSpelling *cantus[] = {P(D, 0, 3), P(E, 0, 3), P(D, 0, 3)};
    const StandardCounterpointSlot quarters[] = {BAR(P(A, 0, 3), P(B, 0, 3), P(C, 0, 4), P(B, 0, 3)),
                                                 BAR(P(A, 0, 3), P(B, 0, 3), P(A, 0, 3), P(C, 1, 4)), WHOLE(P(D, 0, 4))};
    const StandardCounterpointSlot rest[] = {REST(), ATK(P(A, 0, 3)), ATK(P(B, 0, 3)), ATK(P(C, 0, 4)),
                                             BAR(P(A, 0, 3), P(B, 0, 3), P(A, 0, 3), P(C, 1, 4)), WHOLE(P(D, 0, 4))};
    const StandardCounterpointSlot whole[] = {WHOLE(P(A, 0, 3)), BAR(P(A, 0, 3), P(B, 0, 3), P(A, 0, 3), P(C, 1, 4)),
                                              WHOLE(P(D, 0, 4))};
    EXERCISE(quarters_exercise, cantus, quarters, CANTUS_LOWER);
    EXERCISE(rest_exercise, cantus, rest, CANTUS_LOWER);
    EXERCISE(whole_exercise, cantus, whole, CANTUS_LOWER);
    assert(holds(ENDS(OPENING_BAR), &quarters_exercise));
    assert(holds(ENDS(OPENING_BAR), &rest_exercise));
    assert(!holds(ENDS(OPENING_BAR), &whole_exercise));
}

static void test_final_bar(void) {
    const PitchSpelling *cantus[] = {P(D, 0, 3), P(E, 0, 3), P(D, 0, 3)};
    const StandardCounterpointSlot whole_do[] = {BAR(P(A, 0, 3), P(B, 0, 3), P(C, 0, 4), P(B, 0, 3)),
                                                 BAR(P(A, 0, 3), P(B, 0, 3), P(A, 0, 3), P(C, 1, 4)), WHOLE(P(D, 0, 4))};
    const StandardCounterpointSlot four_quarters[] = {BAR(P(A, 0, 3), P(B, 0, 3), P(C, 0, 4), P(B, 0, 3)),
                                                      BAR(P(A, 0, 3), P(B, 0, 3), P(A, 0, 3), P(C, 1, 4)),
                                                      BAR(P(D, 0, 4), P(D, 0, 4), P(D, 0, 4), P(D, 0, 4))};
    const StandardCounterpointSlot not_do[] = {BAR(P(A, 0, 3), P(B, 0, 3), P(C, 0, 4), P(B, 0, 3)),
                                               BAR(P(A, 0, 3), P(B, 0, 3), P(A, 0, 3), P(C, 1, 4)), WHOLE(P(E, 0, 4))};
    const StandardCounterpointSlot octave_up[] = {BAR(P(A, 0, 3), P(B, 0, 3), P(C, 0, 4), P(B, 0, 3)),
                                                  BAR(P(A, 0, 3), P(B, 0, 3), P(A, 0, 3), P(C, 1, 4)), WHOLE(P(D, 0, 5))};
    EXERCISE(whole_exercise, cantus, whole_do, CANTUS_LOWER);
    EXERCISE(quarters_exercise, cantus, four_quarters, CANTUS_LOWER);
    EXERCISE(not_do_exercise, cantus, not_do, CANTUS_LOWER);
    EXERCISE(octave_exercise, cantus, octave_up, CANTUS_LOWER);
    assert(holds(ENDS(FINAL_BAR), &whole_exercise));
    assert(!holds(ENDS(FINAL_BAR), &quarters_exercise));
    assert(!holds(ENDS(FINAL_BAR), &not_do_exercise));
    /* Any octave of do will do. */
    assert(holds(ENDS(FINAL_BAR), &octave_exercise));
}

static void test_clausula_vera(void) {
    const PitchSpelling *cantus[] = {P(D, 0, 3), P(E, 0, 3), P(D, 0, 3)};

    /* The last quarter note of the penultimate bar is the leading tone. */
    const StandardCounterpointSlot quarters[] = {BAR(P(A, 0, 3), P(B, 0, 3), P(C, 0, 4), P(B, 0, 3)),
                                                 BAR(P(A, 0, 3), P(B, 0, 3), P(A, 0, 3), P(C, 1, 4)), WHOLE(P(D, 0, 4))};
    EXERCISE(quarters_exercise, cantus, quarters, CANTUS_LOWER);
    assert(holds(ENDS(CLAUSULA_VERA), &quarters_exercise));

    /* A whole note in the penultimate bar is the note before the final do. */
    const StandardCounterpointSlot whole[] = {BAR(P(A, 0, 3), P(B, 0, 3), P(C, 0, 4), P(B, 0, 3)), WHOLE(P(C, 1, 4)),
                                              WHOLE(P(D, 0, 4))};
    EXERCISE(whole_exercise, cantus, whole, CANTUS_LOWER);
    assert(holds(ENDS(CLAUSULA_VERA), &whole_exercise));

    /* Approached by leap. */
    const StandardCounterpointSlot leap[] = {BAR(P(A, 0, 3), P(B, 0, 3), P(C, 0, 4), P(B, 0, 3)),
                                             BAR(P(A, 0, 3), P(B, 0, 3), P(A, 0, 3), P(G, 0, 3)), WHOLE(P(D, 0, 4))};
    EXERCISE(leap_exercise, cantus, leap, CANTUS_LOWER);
    assert(!holds(ENDS(CLAUSULA_VERA), &leap_exercise));

    /* The same direction as the cantus firmus: both parts descend. */
    const StandardCounterpointSlot similar[] = {BAR(P(A, 0, 3), P(B, 0, 3), P(C, 0, 4), P(B, 0, 3)),
                                                BAR(P(A, 0, 3), P(B, 0, 3), P(A, 0, 3), P(E, 0, 4)), WHOLE(P(D, 0, 4))};
    EXERCISE(similar_exercise, cantus, similar, CANTUS_LOWER);
    assert(!holds(ENDS(CLAUSULA_VERA), &similar_exercise));

    /* The cantus firmus does not approach its last note by step. */
    const PitchSpelling *leaping_cantus[] = {P(D, 0, 3), P(G, 0, 3), P(D, 0, 3)};
    EXERCISE(leaping_exercise, leaping_cantus, quarters, CANTUS_LOWER);
    assert(!holds(ENDS(CLAUSULA_VERA), &leaping_exercise));

    /* The cantus firmus rises to its last note: re is the counterpoint's
       penultimate note. */
    const PitchSpelling *rising_cantus[] = {P(D, 0, 3), P(C, 1, 3), P(D, 0, 3)};
    const StandardCounterpointSlot from_above[] = {BAR(P(A, 0, 3), P(B, 0, 3), P(C, 0, 4), P(B, 0, 3)),
                                                   BAR(P(A, 0, 3), P(B, 0, 3), P(A, 0, 3), P(E, 0, 4)), WHOLE(P(D, 0, 4))};
    EXERCISE(rising_exercise, rising_cantus, from_above, CANTUS_LOWER);
    assert(holds(ENDS(CLAUSULA_VERA), &rising_exercise));
}

static void test_check_masks(void) {
    /* Begins on a mi, a whole note first bar, four quarters last, and leaps
       into the last note. */
    const PitchSpelling *cantus[] = {P(D, 0, 3), P(E, 0, 3), P(D, 0, 3)};
    const StandardCounterpointSlot slots[] = {WHOLE(P(F, 0, 4)), BAR(P(A, 0, 3), P(B, 0, 3), P(A, 0, 3), P(G, 0, 3)),
                                              BAR(P(E, 0, 4), P(E, 0, 4), P(E, 0, 4), P(E, 0, 4))};
    EXERCISE(exercise, cantus, slots, CANTUS_LOWER);
    unsigned violations = 0;
    assert(standard_counterpoint_third_species_ends_check(&exercise, P(D, 0, 4), &violations));
    assert(violations == ((1u << ENDS(BEGIN)) | (1u << ENDS(OPENING_BAR)) | (1u << ENDS(FINAL_BAR)) |
                          (1u << ENDS(CLAUSULA_VERA))));
}

static void test_invalid_input(void) {
    const PitchSpelling *cantus[] = {P(D, 0, 3), P(E, 0, 3)};
    const StandardCounterpointSlot too_few[] = {WHOLE(P(A, 0, 3))};
    const StandardCounterpointSlot slots[] = {BAR(P(A, 0, 3), P(B, 0, 3), P(C, 0, 4), P(B, 0, 3)), WHOLE(P(C, 1, 4))};
    EXERCISE(invalid, cantus, too_few, CANTUS_LOWER);
    EXERCISE(valid, cantus, slots, CANTUS_LOWER);
    unsigned violations = 5;

    assert(!standard_counterpoint_third_species_ends_check(NULL, P(D, 0, 4), &violations));
    assert(!standard_counterpoint_third_species_ends_check(&invalid, P(D, 0, 4), &violations));
    assert(!standard_counterpoint_third_species_ends_check(&valid, NULL, &violations));
    assert(!standard_counterpoint_third_species_ends_check(&valid, P(D, 0, 4), NULL));
    assert(violations == 5);
    assert(!standard_counterpoint_third_species_ends_rule_holds(ENDS(COUNT), &valid, P(D, 0, 4)));
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
    printf("test_third_species_ends_rules (standard-counterpoint): ok\n");
    return 0;
}
