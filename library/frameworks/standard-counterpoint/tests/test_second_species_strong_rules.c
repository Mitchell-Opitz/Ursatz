#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "rule_check.h"
#include "second_species_strong_rules.h"
#include "second_species_test_util.h"
#include "eval_check.h"

/* Every call to the group's rule_holds below also runs the rule through constraint_evaluate. */
STANDARD_COUNTERPOINT_EVAL_TABLE(g_eval, "second-species-strong", STANDARD_COUNTERPOINT_SECOND_SPECIES_STRONG_COUNT);
#define standard_counterpoint_second_species_strong_rule_holds(which, subject) \
    STANDARD_COUNTERPOINT_EVAL_HOLDS(g_eval, standard_counterpoint_second_species_strong_constraint, standard_counterpoint_second_species_strong_rule_holds, StandardCounterpointSecondSpeciesExercise, which, subject)

/* A bar that is one whole note: a note on the strong beat tied through the
   weak beat. */
#define WHOLE(pitch) ATK(pitch), TIE(pitch)

#define STRONG(name) STANDARD_COUNTERPOINT_SECOND_SPECIES_STRONG_##name

static bool is_soft(int rule) {
    return rule == STRONG(IMPERFECT_PREFERRED) || rule == STRONG(NO_STRONG_UNISON);
}

static void test_rules_reference_framework(void) {
    CHECK_RULES_REFERENCE_FRAMEWORK(standard_counterpoint_second_species_strong_rule,
                                    standard_counterpoint_second_species_strong_constraint,
                                    STANDARD_COUNTERPOINT_SECOND_SPECIES_STRONG_COUNT);
    for (int i = 0; i < STANDARD_COUNTERPOINT_SECOND_SPECIES_STRONG_COUNT; i++) {
        ConstraintType type = constraint_type(
            standard_counterpoint_second_species_strong_constraint((StandardCounterpointSecondSpeciesStrongRule)i));
        assert(type == (is_soft(i) ? CONSTRAINT_TYPE_SOFT : CONSTRAINT_TYPE_HARD));
    }
}

static void test_names(void) {
    assert(strcmp(standard_counterpoint_second_species_strong_rule_name(STRONG(CONSONANT)), "consonant") == 0);
    assert(strcmp(standard_counterpoint_second_species_strong_rule_name(STRONG(NO_DISSONANT_OUTLINE)),
                  "no-dissonant-outline") == 0);
    assert(standard_counterpoint_second_species_strong_rule_name(STRONG(COUNT)) == NULL);
}

static void test_consonant(void) {
    const PitchSpelling *cantus[] = {P(C, 0, 3), P(D, 0, 3)};
    const StandardCounterpointSlot good[] = {WHOLE(P(E, 0, 3)), WHOLE(P(F, 0, 3))};
    const StandardCounterpointSlot second[] = {WHOLE(P(E, 0, 3)), WHOLE(P(E, 0, 3))};
    EXERCISE(good_exercise, cantus, good, CANTUS_LOWER);
    EXERCISE(second_exercise, cantus, second, CANTUS_LOWER);
    assert(standard_counterpoint_second_species_strong_rule_holds(STRONG(CONSONANT), &good_exercise));
    /* The second bar's downbeat, E3 against D3, is a second. */
    assert(!standard_counterpoint_second_species_strong_rule_holds(STRONG(CONSONANT), &second_exercise));

    /* A half rest on the first downbeat has no interval to judge. */
    const StandardCounterpointSlot rest_slots[] = {REST(), ATK(P(E, 0, 3)), WHOLE(P(F, 0, 3))};
    EXERCISE(rest_exercise, cantus, rest_slots, CANTUS_LOWER);
    assert(standard_counterpoint_second_species_strong_rule_holds(STRONG(CONSONANT), &rest_exercise));
}

static void test_imperfect_preferred(void) {
    const PitchSpelling *cantus[] = {P(C, 0, 3), P(C, 0, 3), P(C, 0, 3)};
    const StandardCounterpointSlot perfect_inside[] = {WHOLE(P(E, 0, 3)), WHOLE(P(G, 0, 3)), WHOLE(P(E, 0, 3))};
    const StandardCounterpointSlot imperfect_inside[] = {WHOLE(P(G, 0, 3)), WHOLE(P(E, 0, 3)), WHOLE(P(C, 0, 4))};
    EXERCISE(perfect_exercise, cantus, perfect_inside, CANTUS_LOWER);
    EXERCISE(imperfect_exercise, cantus, imperfect_inside, CANTUS_LOWER);
    assert(!standard_counterpoint_second_species_strong_rule_holds(STRONG(IMPERFECT_PREFERRED), &perfect_exercise));
    /* The first and last downbeats are free to be perfect. */
    assert(standard_counterpoint_second_species_strong_rule_holds(STRONG(IMPERFECT_PREFERRED), &imperfect_exercise));
}

static void test_no_strong_unison(void) {
    const PitchSpelling *cantus[] = {P(C, 0, 3), P(C, 0, 3), P(C, 0, 3)};
    const StandardCounterpointSlot unison_inside[] = {WHOLE(P(E, 0, 3)), WHOLE(P(C, 0, 3)), WHOLE(P(E, 0, 3))};
    const StandardCounterpointSlot unison_ends[] = {WHOLE(P(C, 0, 3)), WHOLE(P(E, 0, 3)), WHOLE(P(C, 0, 3))};
    EXERCISE(inside_exercise, cantus, unison_inside, CANTUS_LOWER);
    EXERCISE(ends_exercise, cantus, unison_ends, CANTUS_LOWER);
    assert(!standard_counterpoint_second_species_strong_rule_holds(STRONG(NO_STRONG_UNISON), &inside_exercise));
    assert(standard_counterpoint_second_species_strong_rule_holds(STRONG(NO_STRONG_UNISON), &ends_exercise));
}

static void test_no_repeated_perfect(void) {
    const PitchSpelling *cantus[] = {P(C, 0, 3), P(D, 0, 3)};

    const StandardCounterpointSlot fifths[] = {WHOLE(P(G, 0, 3)), WHOLE(P(A, 0, 3))};
    EXERCISE(fifth_exercise, cantus, fifths, CANTUS_LOWER);
    assert(!standard_counterpoint_second_species_strong_rule_holds(STRONG(NO_REPEATED_PERFECT), &fifth_exercise));

    /* Compound intervals count as simple: P5 then P12. */
    const StandardCounterpointSlot twelfth[] = {WHOLE(P(G, 0, 3)), WHOLE(P(A, 0, 4))};
    EXERCISE(twelfth_exercise, cantus, twelfth, CANTUS_LOWER);
    assert(!standard_counterpoint_second_species_strong_rule_holds(STRONG(NO_REPEATED_PERFECT), &twelfth_exercise));

    /* Two different perfect intervals are fine. */
    const StandardCounterpointSlot octave_then_fifth[] = {WHOLE(P(C, 0, 4)), WHOLE(P(A, 0, 3))};
    EXERCISE(different_exercise, cantus, octave_then_fifth, CANTUS_LOWER);
    assert(standard_counterpoint_second_species_strong_rule_holds(STRONG(NO_REPEATED_PERFECT), &different_exercise));

    /* A perfect weak beat, then the same perfect interval on the downbeat. */
    const StandardCounterpointSlot weak_fifth[] = {ATK(P(E, 0, 3)), ATK(P(G, 0, 3)), WHOLE(P(A, 0, 3))};
    EXERCISE(weak_exercise, cantus, weak_fifth, CANTUS_LOWER);
    assert(!standard_counterpoint_second_species_strong_rule_holds(STRONG(NO_REPEATED_PERFECT), &weak_exercise));

    /* Hidden fifths between downbeats are fine: here a downbeat third, then
       a downbeat fifth by similar motion. */
    const StandardCounterpointSlot hidden[] = {WHOLE(P(E, 0, 3)), WHOLE(P(A, 0, 3))};
    EXERCISE(hidden_exercise, cantus, hidden, CANTUS_LOWER);
    assert(standard_counterpoint_second_species_strong_rule_holds(STRONG(NO_REPEATED_PERFECT), &hidden_exercise));
}

static void test_no_dissonant_outline(void) {
    const PitchSpelling *cantus[] = {P(C, 0, 3), P(D, 0, 3)};

    /* G3 to F4 is a seventh between consonant downbeats. */
    const StandardCounterpointSlot seventh[] = {ATK(P(G, 0, 3)), ATK(P(A, 0, 3)), WHOLE(P(F, 0, 4))};
    EXERCISE(seventh_exercise, cantus, seventh, CANTUS_LOWER);
    assert(!standard_counterpoint_second_species_strong_rule_holds(STRONG(NO_DISSONANT_OUTLINE), &seventh_exercise));

    /* The exception: an octave leap to the weak beat, then a step back. */
    const StandardCounterpointSlot octave_leap[] = {ATK(P(G, 0, 3)), ATK(P(G, 0, 4)), WHOLE(P(F, 0, 4))};
    EXERCISE(octave_exercise, cantus, octave_leap, CANTUS_LOWER);
    assert(standard_counterpoint_second_species_strong_rule_holds(STRONG(NO_DISSONANT_OUTLINE), &octave_exercise));

    const StandardCounterpointSlot third[] = {WHOLE(P(E, 0, 3)), WHOLE(P(G, 0, 3))};
    EXERCISE(third_exercise, cantus, third, CANTUS_LOWER);
    assert(standard_counterpoint_second_species_strong_rule_holds(STRONG(NO_DISSONANT_OUTLINE), &third_exercise));
}

static void test_max_same_imperfect_run(void) {
    const PitchSpelling *cantus[] = {P(C, 0, 3), P(D, 0, 3), P(E, 0, 3), P(F, 0, 3)};
    const StandardCounterpointSlot four_thirds[] = {WHOLE(P(E, 0, 3)), WHOLE(P(F, 0, 3)), WHOLE(P(G, 0, 3)),
                                                    WHOLE(P(A, 0, 3))};
    const StandardCounterpointSlot three_thirds[] = {WHOLE(P(E, 0, 3)), WHOLE(P(F, 0, 3)), WHOLE(P(G, 0, 3)),
                                                     WHOLE(P(D, 0, 4))};
    EXERCISE(four_exercise, cantus, four_thirds, CANTUS_LOWER);
    EXERCISE(three_exercise, cantus, three_thirds, CANTUS_LOWER);
    assert(!standard_counterpoint_second_species_strong_rule_holds(STRONG(MAX_SAME_IMPERFECT_RUN), &four_exercise));
    assert(standard_counterpoint_second_species_strong_rule_holds(STRONG(MAX_SAME_IMPERFECT_RUN), &three_exercise));
}

static void test_check_masks(void) {
    /* A second against the cantus firmus on the last downbeat, and the same
       perfect fifth on two downbeats in a row. */
    const PitchSpelling *cantus[] = {P(C, 0, 3), P(D, 0, 3), P(E, 0, 3)};
    const StandardCounterpointSlot slots[] = {WHOLE(P(G, 0, 3)), WHOLE(P(A, 0, 3)), WHOLE(P(F, 0, 3))};
    EXERCISE(exercise, cantus, slots, CANTUS_LOWER);
    unsigned hard = 0, soft = 0;
    assert(standard_counterpoint_second_species_strong_check(&exercise, &hard, &soft));
    assert(hard & (1u << STRONG(CONSONANT)));
    assert(hard & (1u << STRONG(NO_REPEATED_PERFECT)));
    assert(soft & (1u << STRONG(IMPERFECT_PREFERRED)));
    assert(!(soft & (1u << STRONG(CONSONANT))));
}

static void test_invalid_input(void) {
    const PitchSpelling *cantus[] = {P(C, 0, 3), P(D, 0, 3)};
    const StandardCounterpointSlot too_few[] = {WHOLE(P(E, 0, 3))};
    EXERCISE(invalid, cantus, too_few, CANTUS_LOWER);
    unsigned hard = 5, soft = 5;

    assert(!standard_counterpoint_second_species_strong_check(NULL, &hard, &soft));
    assert(!standard_counterpoint_second_species_strong_check(&invalid, &hard, &soft));
    assert(hard == 5 && soft == 5);
    assert(!standard_counterpoint_second_species_strong_rule_holds(STRONG(CONSONANT), &invalid));
    assert(!standard_counterpoint_second_species_strong_rule_holds(STRONG(COUNT), &invalid));
}

int main(void) {
    test_rules_reference_framework();
    test_names();
    test_consonant();
    test_imperfect_preferred();
    test_no_strong_unison();
    test_no_repeated_perfect();
    test_no_dissonant_outline();
    test_max_same_imperfect_run();
    test_check_masks();
    test_invalid_input();
    printf("test_second_species_strong_rules (standard-counterpoint): ok\n");
    return 0;
}
