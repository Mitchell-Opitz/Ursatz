#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "fourth_species_test_util.h"
#include "fourth_species_weak_rules.h"
#include "rule_check.h"
#include "eval_check.h"

/* Every call to the group's rule_holds below also runs the rule through constraint_evaluate. */
STANDARD_COUNTERPOINT_EVAL_TABLE(g_eval, "fourth-species-weak", STANDARD_COUNTERPOINT_FOURTH_SPECIES_WEAK_COUNT);
#define standard_counterpoint_fourth_species_weak_rule_holds(which, subject) \
    STANDARD_COUNTERPOINT_EVAL_HOLDS(g_eval, standard_counterpoint_fourth_species_weak_constraint, standard_counterpoint_fourth_species_weak_preference, standard_counterpoint_fourth_species_weak_rule_holds, StandardCounterpointFourthSpeciesExercise, which, subject)

#define WEAK(name) STANDARD_COUNTERPOINT_FOURTH_SPECIES_WEAK_##name

static bool holds(StandardCounterpointFourthSpeciesWeakRule rule,
                  const StandardCounterpointFourthSpeciesExercise *exercise) {
    return standard_counterpoint_fourth_species_weak_rule_holds(rule, exercise);
}

static void test_rules_reference_framework(void) {
    CHECK_RULES_REFERENCE_FRAMEWORK(standard_counterpoint_fourth_species_weak_rule,
                                    standard_counterpoint_fourth_species_weak_constraint,
                                    STANDARD_COUNTERPOINT_FOURTH_SPECIES_WEAK_COUNT);
    for (int i = 0; i < STANDARD_COUNTERPOINT_FOURTH_SPECIES_WEAK_COUNT; i++) {
        ConstraintType type = constraint_type(
            standard_counterpoint_fourth_species_weak_constraint((StandardCounterpointFourthSpeciesWeakRule)i));
        assert(type == (i == WEAK(NO_INTERIOR_UNISON) ? CONSTRAINT_TYPE_SOFT : CONSTRAINT_TYPE_HARD));
    }
}

static void test_names(void) {
    assert(strcmp(standard_counterpoint_fourth_species_weak_rule_name(WEAK(CONSONANT)), "consonant") == 0);
    assert(strcmp(standard_counterpoint_fourth_species_weak_rule_name(WEAK(NO_REPEATED_PERFECT)),
                  "no-repeated-perfect") == 0);
    assert(strcmp(standard_counterpoint_fourth_species_weak_rule_name(WEAK(MAX_SAME_IMPERFECT_RUN)),
                  "max-same-imperfect-run") == 0);
    assert(strcmp(standard_counterpoint_fourth_species_weak_rule_name(WEAK(NO_DIRECT_PERFECT)),
                  "no-direct-perfect") == 0);
    assert(strcmp(standard_counterpoint_fourth_species_weak_rule_name(WEAK(NO_INTERIOR_UNISON)),
                  "no-interior-unison") == 0);
    assert(standard_counterpoint_fourth_species_weak_rule_name(WEAK(COUNT)) == NULL);
}

static void test_good_exercise(void) {
    GOOD_FOURTH_SPECIES_CANTUS(cantus);
    GOOD_FOURTH_SPECIES_SLOTS(slots);
    EXERCISE(exercise, cantus, slots, CANTUS_LOWER);
    for (int i = 0; i < STANDARD_COUNTERPOINT_FOURTH_SPECIES_WEAK_COUNT; i++) {
        assert(holds((StandardCounterpointFourthSpeciesWeakRule)i, &exercise));
    }
}

static void test_consonant(void) {
    const PitchSpelling *cantus[] = {P(C, 0, 3), P(C, 0, 3), P(C, 0, 3), P(C, 0, 3)};

    /* A second on a weak beat. */
    const StandardCounterpointSlot dissonant[] = {REST(), ATK(P(E, 0, 3)), TIE(P(E, 0, 3)), ATK(P(D, 0, 3)),
                                                  TIE(P(D, 0, 3)), ATK(P(E, 0, 3)), WHOLE(P(G, 0, 3))};
    EXERCISE(dissonant_exercise, cantus, dissonant, CANTUS_LOWER);
    assert(!holds(WEAK(CONSONANT), &dissonant_exercise));

    /* A bar in which species is broken follows the species-break rules. */
    const PitchSpelling *three[] = {P(C, 0, 3), P(C, 0, 3), P(C, 0, 3)};
    const StandardCounterpointSlot broken[] = {REST(), ATK(P(G, 0, 3)), ATK(P(E, 0, 3)), ATK(P(D, 0, 3)),
                                               WHOLE(P(C, 0, 4))};
    EXERCISE(broken_exercise, three, broken, CANTUS_LOWER);
    assert(holds(WEAK(CONSONANT), &broken_exercise));
}

static void test_no_repeated_perfect(void) {
    const PitchSpelling *cantus[] = {P(C, 0, 3), P(C, 0, 3), P(C, 0, 3), P(C, 0, 3)};

    /* Two fifths in a row. */
    const StandardCounterpointSlot fifths[] = {REST(), ATK(P(G, 0, 3)), TIE(P(G, 0, 3)), ATK(P(G, 0, 3)),
                                               TIE(P(G, 0, 3)), ATK(P(E, 0, 3)), WHOLE(P(C, 0, 4))};
    EXERCISE(fifths_exercise, cantus, fifths, CANTUS_LOWER);
    assert(!holds(WEAK(NO_REPEATED_PERFECT), &fifths_exercise));

    /* A fifth and then an octave are different. */
    const StandardCounterpointSlot mixed[] = {REST(), ATK(P(G, 0, 3)), TIE(P(G, 0, 3)), ATK(P(C, 0, 4)),
                                              TIE(P(C, 0, 4)), ATK(P(E, 0, 3)), WHOLE(P(C, 0, 4))};
    EXERCISE(mixed_exercise, cantus, mixed, CANTUS_LOWER);
    assert(holds(WEAK(NO_REPEATED_PERFECT), &mixed_exercise));

    /* A unison counts as an octave. */
    const StandardCounterpointSlot unison[] = {REST(), ATK(P(C, 0, 3)), TIE(P(C, 0, 3)), ATK(P(C, 0, 4)),
                                               TIE(P(C, 0, 4)), ATK(P(E, 0, 3)), WHOLE(P(C, 0, 4))};
    EXERCISE(unison_exercise, cantus, unison, CANTUS_LOWER);
    assert(!holds(WEAK(NO_REPEATED_PERFECT), &unison_exercise));
}

static void test_max_same_imperfect_run(void) {
    const PitchSpelling *cantus[] = {P(C, 0, 3), P(C, 0, 3), P(C, 0, 3), P(C, 0, 3), P(C, 0, 3), P(C, 0, 3)};

    /* Four thirds in a row. */
    const StandardCounterpointSlot four[] = {REST(), ATK(P(E, 0, 3)), TIE(P(E, 0, 3)), ATK(P(E, 0, 3)),
                                             TIE(P(E, 0, 3)), ATK(P(E, 0, 3)), TIE(P(E, 0, 3)), ATK(P(E, 0, 3)),
                                             TIE(P(E, 0, 3)), ATK(P(G, 0, 3)), WHOLE(P(C, 0, 4))};
    EXERCISE(four_exercise, cantus, four, CANTUS_LOWER);
    assert(!holds(WEAK(MAX_SAME_IMPERFECT_RUN), &four_exercise));

    /* Three, and then a fifth, and then a third again. */
    const StandardCounterpointSlot three[] = {REST(), ATK(P(E, 0, 3)), TIE(P(E, 0, 3)), ATK(P(E, 0, 3)),
                                              TIE(P(E, 0, 3)), ATK(P(E, 0, 3)), TIE(P(E, 0, 3)), ATK(P(G, 0, 3)),
                                              TIE(P(G, 0, 3)), ATK(P(E, 0, 3)), WHOLE(P(C, 0, 4))};
    EXERCISE(three_exercise, cantus, three, CANTUS_LOWER);
    assert(holds(WEAK(MAX_SAME_IMPERFECT_RUN), &three_exercise));
}

static void test_no_direct_perfect(void) {
    const PitchSpelling *cantus[] = {P(C, 0, 3), P(D, 0, 3), P(C, 0, 3)};

    /* The cantus firmus and the counterpoint both rise into a fifth. */
    const StandardCounterpointSlot similar[] = {REST(), ATK(P(E, 0, 3)), TIE(P(E, 0, 3)), ATK(P(A, 0, 3)),
                                                WHOLE(P(C, 0, 4))};
    EXERCISE(similar_exercise, cantus, similar, CANTUS_LOWER);
    assert(!holds(WEAK(NO_DIRECT_PERFECT), &similar_exercise));

    /* Contrary motion into a fifth. */
    const StandardCounterpointSlot contrary[] = {REST(), ATK(P(C, 0, 4)), TIE(P(C, 0, 4)), ATK(P(A, 0, 3)),
                                                 WHOLE(P(C, 0, 4))};
    EXERCISE(contrary_exercise, cantus, contrary, CANTUS_LOWER);
    assert(holds(WEAK(NO_DIRECT_PERFECT), &contrary_exercise));

    /* The same, with the cantus firmus above. */
    const PitchSpelling *upper_cantus[] = {P(C, 0, 5), P(B, 0, 4), P(C, 0, 5)};
    const StandardCounterpointSlot below[] = {REST(), ATK(P(F, 0, 4)), TIE(P(F, 0, 4)), ATK(P(E, 0, 4)),
                                              WHOLE(P(C, 0, 4))};
    EXERCISE(below_exercise, upper_cantus, below, CANTUS_UPPER);
    assert(!holds(WEAK(NO_DIRECT_PERFECT), &below_exercise));
}

static void test_no_interior_unison(void) {
    const PitchSpelling *cantus[] = {P(C, 0, 3), P(C, 0, 3), P(C, 0, 3), P(C, 0, 3)};

    const StandardCounterpointSlot interior[] = {REST(), ATK(P(E, 0, 3)), TIE(P(E, 0, 3)), ATK(P(C, 0, 3)),
                                                 TIE(P(C, 0, 3)), ATK(P(E, 0, 3)), WHOLE(P(C, 0, 4))};
    EXERCISE(interior_exercise, cantus, interior, CANTUS_LOWER);
    assert(!holds(WEAK(NO_INTERIOR_UNISON), &interior_exercise));

    /* A unison in the first bar is allowed. */
    const StandardCounterpointSlot first[] = {REST(), ATK(P(C, 0, 3)), TIE(P(C, 0, 3)), ATK(P(E, 0, 3)),
                                              TIE(P(E, 0, 3)), ATK(P(G, 0, 3)), WHOLE(P(C, 0, 4))};
    EXERCISE(first_exercise, cantus, first, CANTUS_LOWER);
    assert(holds(WEAK(NO_INTERIOR_UNISON), &first_exercise));
}

static void test_check_masks(void) {
    const PitchSpelling *cantus[] = {P(C, 0, 3), P(C, 0, 3), P(C, 0, 3), P(C, 0, 3)};
    unsigned hard = 0, soft = 0;

    const StandardCounterpointSlot dissonant[] = {REST(), ATK(P(E, 0, 3)), TIE(P(E, 0, 3)), ATK(P(D, 0, 3)),
                                                  TIE(P(D, 0, 3)), ATK(P(E, 0, 3)), WHOLE(P(G, 0, 3))};
    EXERCISE(dissonant_exercise, cantus, dissonant, CANTUS_LOWER);
    assert(standard_counterpoint_fourth_species_weak_check(&dissonant_exercise, &hard, &soft));
    assert(hard == (1u << WEAK(CONSONANT)));
    assert(soft == 0);

    const StandardCounterpointSlot interior[] = {REST(), ATK(P(E, 0, 3)), TIE(P(E, 0, 3)), ATK(P(C, 0, 3)),
                                                 TIE(P(C, 0, 3)), ATK(P(E, 0, 3)), WHOLE(P(C, 0, 4))};
    EXERCISE(interior_exercise, cantus, interior, CANTUS_LOWER);
    assert(standard_counterpoint_fourth_species_weak_check(&interior_exercise, &hard, &soft));
    assert(hard == 0);
    assert(soft == (1u << WEAK(NO_INTERIOR_UNISON)));
}

static void test_invalid_input(void) {
    const PitchSpelling *cantus[] = {P(C, 0, 3), P(D, 0, 3)};
    const StandardCounterpointSlot too_few[] = {WHOLE(P(E, 0, 3))};
    EXERCISE(invalid, cantus, too_few, CANTUS_LOWER);
    unsigned hard = 5, soft = 5;

    assert(!standard_counterpoint_fourth_species_weak_check(NULL, &hard, &soft));
    assert(!standard_counterpoint_fourth_species_weak_check(&invalid, &hard, &soft));
    assert(!standard_counterpoint_fourth_species_weak_check(&invalid, &hard, NULL));
    assert(hard == 5 && soft == 5);
    assert(!holds(WEAK(CONSONANT), &invalid));
    assert(!holds(WEAK(COUNT), &invalid));
}

int main(void) {
    test_rules_reference_framework();
    test_names();
    test_good_exercise();
    test_consonant();
    test_no_repeated_perfect();
    test_max_same_imperfect_run();
    test_no_direct_perfect();
    test_no_interior_unison();
    test_check_masks();
    test_invalid_input();
    printf("test_fourth_species_weak_rules (standard-counterpoint): ok\n");
    return 0;
}
