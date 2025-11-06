#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "rule_check.h"
#include "third_species_strong_rules.h"
#include "third_species_test_util.h"
#include "eval_check.h"

/* Every call to the group's rule_holds below also runs the rule through constraint_evaluate. */
STANDARD_COUNTERPOINT_EVAL_TABLE(g_eval, "third-species-strong", STANDARD_COUNTERPOINT_THIRD_SPECIES_STRONG_COUNT);
#define standard_counterpoint_third_species_strong_rule_holds(which, subject) \
    STANDARD_COUNTERPOINT_EVAL_HOLDS(g_eval, standard_counterpoint_third_species_strong_constraint, standard_counterpoint_third_species_strong_preference, standard_counterpoint_third_species_strong_rule_holds, StandardCounterpointThirdSpeciesExercise, which, subject)

#define STRONG(name) STANDARD_COUNTERPOINT_THIRD_SPECIES_STRONG_##name

/* Four quarter notes. */
#define BAR(a, b, c, d) ATK(a), ATK(b), ATK(c), ATK(d)

static bool is_soft(int rule) {
    return rule == STRONG(IMPERFECT_PREFERRED) || rule == STRONG(NO_STRONG_UNISON);
}

static bool holds(StandardCounterpointThirdSpeciesStrongRule rule,
                  const StandardCounterpointThirdSpeciesExercise *exercise) {
    return standard_counterpoint_third_species_strong_rule_holds(rule, exercise);
}

static void test_rules_reference_framework(void) {
    CHECK_RULES_REFERENCE_FRAMEWORK(standard_counterpoint_third_species_strong_rule,
                                    standard_counterpoint_third_species_strong_constraint,
                                    STANDARD_COUNTERPOINT_THIRD_SPECIES_STRONG_COUNT);
    for (int i = 0; i < STANDARD_COUNTERPOINT_THIRD_SPECIES_STRONG_COUNT; i++) {
        ConstraintType type = constraint_type(
            standard_counterpoint_third_species_strong_constraint((StandardCounterpointThirdSpeciesStrongRule)i));
        assert(type == (is_soft(i) ? CONSTRAINT_TYPE_SOFT : CONSTRAINT_TYPE_HARD));
    }
}

static void test_names(void) {
    assert(strcmp(standard_counterpoint_third_species_strong_rule_name(STRONG(CONSONANT)), "consonant") == 0);
    assert(strcmp(standard_counterpoint_third_species_strong_rule_name(STRONG(BAR_LINE_DIRECT_PERFECT)),
                  "bar-line-direct-perfect") == 0);
    assert(standard_counterpoint_third_species_strong_rule_name(STRONG(COUNT)) == NULL);
}

static void test_consonant(void) {
    const PitchSpelling *cantus[] = {P(C, 0, 3), P(D, 0, 3)};
    const StandardCounterpointSlot good[] = {WHOLE(P(E, 0, 3)), WHOLE(P(F, 0, 3))};
    const StandardCounterpointSlot second[] = {WHOLE(P(E, 0, 3)), WHOLE(P(E, 0, 3))};
    const StandardCounterpointSlot rest[] = {REST(), ATK(P(E, 0, 3)), ATK(P(F, 0, 3)), ATK(P(G, 0, 3)),
                                             WHOLE(P(F, 0, 3))};
    EXERCISE(good_exercise, cantus, good, CANTUS_LOWER);
    EXERCISE(second_exercise, cantus, second, CANTUS_LOWER);
    EXERCISE(rest_exercise, cantus, rest, CANTUS_LOWER);
    assert(holds(STRONG(CONSONANT), &good_exercise));
    assert(!holds(STRONG(CONSONANT), &second_exercise));
    /* A quarter rest on the first downbeat has no interval to judge. */
    assert(holds(STRONG(CONSONANT), &rest_exercise));
}

static void test_imperfect_preferred_and_unison(void) {
    const PitchSpelling *cantus[] = {P(C, 0, 3), P(C, 0, 3), P(C, 0, 3)};
    const StandardCounterpointSlot perfect_inside[] = {WHOLE(P(E, 0, 3)), WHOLE(P(G, 0, 3)), WHOLE(P(E, 0, 3))};
    const StandardCounterpointSlot imperfect_inside[] = {WHOLE(P(G, 0, 3)), WHOLE(P(E, 0, 3)), WHOLE(P(C, 0, 4))};
    const StandardCounterpointSlot unison_inside[] = {WHOLE(P(E, 0, 3)), WHOLE(P(C, 0, 3)), WHOLE(P(E, 0, 3))};
    const StandardCounterpointSlot unison_ends[] = {WHOLE(P(C, 0, 3)), WHOLE(P(E, 0, 3)), WHOLE(P(C, 0, 3))};
    EXERCISE(perfect_exercise, cantus, perfect_inside, CANTUS_LOWER);
    EXERCISE(imperfect_exercise, cantus, imperfect_inside, CANTUS_LOWER);
    EXERCISE(unison_exercise, cantus, unison_inside, CANTUS_LOWER);
    EXERCISE(ends_exercise, cantus, unison_ends, CANTUS_LOWER);
    assert(!holds(STRONG(IMPERFECT_PREFERRED), &perfect_exercise));
    /* The first and last downbeats are free to be perfect. */
    assert(holds(STRONG(IMPERFECT_PREFERRED), &imperfect_exercise));
    assert(!holds(STRONG(NO_STRONG_UNISON), &unison_exercise));
    assert(holds(STRONG(NO_STRONG_UNISON), &ends_exercise));
}

static void test_no_three_perfect(void) {
    const PitchSpelling *cantus[] = {P(C, 0, 3), P(D, 0, 3), P(E, 0, 3)};
    /* Fifths over each cantus firmus note: three in a row. */
    const StandardCounterpointSlot three[] = {WHOLE(P(G, 0, 3)), WHOLE(P(A, 0, 3)), WHOLE(P(B, 0, 3))};
    /* Two in a row are fine. */
    const StandardCounterpointSlot two[] = {WHOLE(P(G, 0, 3)), WHOLE(P(A, 0, 3)), WHOLE(P(G, 0, 3))};
    /* A fifth, an octave, and a fifth are different perfect intervals. */
    const StandardCounterpointSlot varied[] = {WHOLE(P(G, 0, 3)), WHOLE(P(D, 0, 4)), WHOLE(P(B, 0, 3))};
    EXERCISE(three_exercise, cantus, three, CANTUS_LOWER);
    EXERCISE(two_exercise, cantus, two, CANTUS_LOWER);
    EXERCISE(varied_exercise, cantus, varied, CANTUS_LOWER);
    assert(!holds(STRONG(NO_THREE_PERFECT), &three_exercise));
    assert(holds(STRONG(NO_THREE_PERFECT), &two_exercise));
    assert(holds(STRONG(NO_THREE_PERFECT), &varied_exercise));
}

static void test_max_same_imperfect_run(void) {
    const PitchSpelling *cantus[] = {P(C, 0, 3), P(D, 0, 3), P(E, 0, 3), P(F, 0, 3)};
    const StandardCounterpointSlot four[] = {WHOLE(P(E, 0, 3)), WHOLE(P(F, 0, 3)), WHOLE(P(G, 0, 3)), WHOLE(P(A, 0, 3))};
    const StandardCounterpointSlot three[] = {WHOLE(P(E, 0, 3)), WHOLE(P(F, 0, 3)), WHOLE(P(G, 0, 3)), WHOLE(P(D, 0, 4))};
    EXERCISE(four_exercise, cantus, four, CANTUS_LOWER);
    EXERCISE(three_exercise, cantus, three, CANTUS_LOWER);
    assert(!holds(STRONG(MAX_SAME_IMPERFECT_RUN), &four_exercise));
    assert(holds(STRONG(MAX_SAME_IMPERFECT_RUN), &three_exercise));
}

static void test_no_dissonant_outline(void) {
    const PitchSpelling *cantus[] = {P(C, 0, 3), P(D, 0, 3)};
    /* G3 to F4 is a seventh between consonant downbeats. */
    const StandardCounterpointSlot seventh[] = {WHOLE(P(G, 0, 3)), WHOLE(P(F, 0, 4))};
    /* An octave leap and a step back does not excuse it in third species. */
    const StandardCounterpointSlot leap[] = {BAR(P(G, 0, 3), P(G, 0, 4), P(G, 0, 4), P(G, 0, 4)), WHOLE(P(F, 0, 4))};
    const StandardCounterpointSlot third[] = {WHOLE(P(E, 0, 3)), WHOLE(P(G, 0, 3))};
    EXERCISE(seventh_exercise, cantus, seventh, CANTUS_LOWER);
    EXERCISE(leap_exercise, cantus, leap, CANTUS_LOWER);
    EXERCISE(third_exercise, cantus, third, CANTUS_LOWER);
    assert(!holds(STRONG(NO_DISSONANT_OUTLINE), &seventh_exercise));
    assert(!holds(STRONG(NO_DISSONANT_OUTLINE), &leap_exercise));
    assert(holds(STRONG(NO_DISSONANT_OUTLINE), &third_exercise));
}

static void test_previous_bar_perfects(void) {
    const PitchSpelling *cantus[] = {P(C, 0, 3), P(D, 0, 3)};

    /* A fifth on beat 3 before a downbeat fifth. */
    const StandardCounterpointSlot fifth_on_three[] = {BAR(P(E, 0, 3), P(F, 0, 3), P(G, 0, 3), P(A, 0, 3)),
                                                      WHOLE(P(A, 0, 3))};
    /* A fifth on beat 2 is allowed. */
    const StandardCounterpointSlot fifth_on_two[] = {BAR(P(E, 0, 3), P(G, 0, 3), P(A, 0, 3), P(C, 0, 4)),
                                                    WHOLE(P(A, 0, 3))};
    EXERCISE(three_exercise, cantus, fifth_on_three, CANTUS_LOWER);
    EXERCISE(two_exercise, cantus, fifth_on_two, CANTUS_LOWER);
    assert(!holds(STRONG(PREVIOUS_BAR_PERFECTS), &three_exercise));
    assert(holds(STRONG(PREVIOUS_BAR_PERFECTS), &two_exercise));

    /* An octave on beat 2 before a downbeat octave; on beat 1 it is allowed. */
    const StandardCounterpointSlot octave_on_two[] = {BAR(P(E, 0, 3), P(C, 0, 4), P(A, 0, 3), P(G, 0, 3)),
                                                     WHOLE(P(D, 0, 4))};
    const StandardCounterpointSlot octave_on_one[] = {BAR(P(C, 0, 4), P(E, 0, 3), P(A, 0, 3), P(G, 0, 3)),
                                                     WHOLE(P(D, 0, 4))};
    EXERCISE(octave_two_exercise, cantus, octave_on_two, CANTUS_LOWER);
    EXERCISE(octave_one_exercise, cantus, octave_on_one, CANTUS_LOWER);
    assert(!holds(STRONG(PREVIOUS_BAR_PERFECTS), &octave_two_exercise));
    assert(holds(STRONG(PREVIOUS_BAR_PERFECTS), &octave_one_exercise));
}

static void test_bar_line_repeated_perfect(void) {
    const PitchSpelling *cantus[] = {P(C, 0, 3), P(D, 0, 3)};
    const StandardCounterpointSlot repeated[] = {BAR(P(E, 0, 3), P(F, 0, 3), P(A, 0, 3), P(G, 0, 3)),
                                                 WHOLE(P(A, 0, 3))};
    const StandardCounterpointSlot different[] = {BAR(P(E, 0, 3), P(F, 0, 3), P(A, 0, 3), P(C, 0, 4)),
                                                  WHOLE(P(A, 0, 3))};
    EXERCISE(repeated_exercise, cantus, repeated, CANTUS_LOWER);
    EXERCISE(different_exercise, cantus, different, CANTUS_LOWER);
    assert(!holds(STRONG(BAR_LINE_REPEATED_PERFECT), &repeated_exercise));
    assert(holds(STRONG(BAR_LINE_REPEATED_PERFECT), &different_exercise));
}

static void test_bar_line_direct_perfect(void) {
    /* The cantus firmus rises, C3 to D3. */
    const PitchSpelling *cantus[] = {P(C, 0, 3), P(D, 0, 3)};
    /* G3 to A3: the fifth over D3 is reached by similar motion. */
    const StandardCounterpointSlot direct[] = {BAR(P(E, 0, 3), P(F, 0, 3), P(A, 0, 3), P(G, 0, 3)),
                                               WHOLE(P(A, 0, 3))};
    /* B3 to A3: contrary motion. */
    const StandardCounterpointSlot contrary[] = {BAR(P(E, 0, 3), P(F, 0, 3), P(A, 0, 3), P(B, 0, 3)),
                                                 WHOLE(P(A, 0, 3))};
    EXERCISE(direct_exercise, cantus, direct, CANTUS_LOWER);
    EXERCISE(contrary_exercise, cantus, contrary, CANTUS_LOWER);
    assert(!holds(STRONG(BAR_LINE_DIRECT_PERFECT), &direct_exercise));
    assert(holds(STRONG(BAR_LINE_DIRECT_PERFECT), &contrary_exercise));

    /* The same with the cantus firmus above the counterpoint. */
    const PitchSpelling *upper_cantus[] = {P(C, 0, 4), P(D, 0, 4)};
    const StandardCounterpointSlot below_direct[] = {BAR(P(E, 0, 3), P(D, 0, 3), P(E, 0, 3), P(F, 0, 3)),
                                                     WHOLE(P(G, 0, 3))};
    const StandardCounterpointSlot below_contrary[] = {BAR(P(E, 0, 3), P(D, 0, 3), P(E, 0, 3), P(A, 0, 3)),
                                                       WHOLE(P(G, 0, 3))};
    EXERCISE(below_exercise, upper_cantus, below_direct, CANTUS_UPPER);
    EXERCISE(below_contrary_exercise, upper_cantus, below_contrary, CANTUS_UPPER);
    assert(!holds(STRONG(BAR_LINE_DIRECT_PERFECT), &below_exercise));
    assert(holds(STRONG(BAR_LINE_DIRECT_PERFECT), &below_contrary_exercise));
}

static void test_check_masks(void) {
    /* The second downbeat, E3 against D3, is a second. */
    const PitchSpelling *cantus[] = {P(C, 0, 3), P(D, 0, 3)};
    const StandardCounterpointSlot slots[] = {WHOLE(P(E, 0, 3)), WHOLE(P(E, 0, 3))};
    EXERCISE(exercise, cantus, slots, CANTUS_LOWER);
    unsigned hard = 0, soft = 0;
    assert(standard_counterpoint_third_species_strong_check(&exercise, &hard, &soft));
    assert(hard & (1u << STRONG(CONSONANT)));
    assert(!(soft & (1u << STRONG(CONSONANT))));
}

static void test_invalid_input(void) {
    const PitchSpelling *cantus[] = {P(C, 0, 3), P(D, 0, 3)};
    const StandardCounterpointSlot too_few[] = {WHOLE(P(E, 0, 3))};
    EXERCISE(invalid, cantus, too_few, CANTUS_LOWER);
    unsigned hard = 5, soft = 5;

    assert(!standard_counterpoint_third_species_strong_check(NULL, &hard, &soft));
    assert(!standard_counterpoint_third_species_strong_check(&invalid, &hard, &soft));
    assert(hard == 5 && soft == 5);
    assert(!holds(STRONG(CONSONANT), &invalid));
    assert(!holds(STRONG(COUNT), &invalid));
}

int main(void) {
    test_rules_reference_framework();
    test_names();
    test_consonant();
    test_imperfect_preferred_and_unison();
    test_no_three_perfect();
    test_max_same_imperfect_run();
    test_no_dissonant_outline();
    test_previous_bar_perfects();
    test_bar_line_repeated_perfect();
    test_bar_line_direct_perfect();
    test_check_masks();
    test_invalid_input();
    printf("test_third_species_strong_rules (standard-counterpoint): ok\n");
    return 0;
}
