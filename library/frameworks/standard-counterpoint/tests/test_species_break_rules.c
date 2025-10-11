#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "fourth_species_test_util.h"
#include "rule_check.h"
#include "species_break_rules.h"
#include "eval_check.h"

/* Every call to the group's rule_holds below also runs the rule through constraint_evaluate. */
STANDARD_COUNTERPOINT_EVAL_TABLE(g_eval, "species-break", STANDARD_COUNTERPOINT_SPECIES_BREAK_COUNT);
#define standard_counterpoint_species_break_rule_holds(which, subject) \
    STANDARD_COUNTERPOINT_EVAL_HOLDS(g_eval, standard_counterpoint_species_break_constraint, standard_counterpoint_species_break_rule_holds, StandardCounterpointFourthSpeciesExercise, which, subject)

#define BREAK(name) STANDARD_COUNTERPOINT_SPECIES_BREAK_##name

static bool holds(StandardCounterpointSpeciesBreakRule rule,
                  const StandardCounterpointFourthSpeciesExercise *exercise) {
    return standard_counterpoint_species_break_rule_holds(rule, exercise);
}

static void test_rules_reference_framework(void) {
    CHECK_RULES_REFERENCE_FRAMEWORK(standard_counterpoint_species_break_rule,
                                    standard_counterpoint_species_break_constraint,
                                    STANDARD_COUNTERPOINT_SPECIES_BREAK_COUNT);
    for (int i = 0; i < STANDARD_COUNTERPOINT_SPECIES_BREAK_COUNT; i++) {
        ConstraintType type =
            constraint_type(standard_counterpoint_species_break_constraint((StandardCounterpointSpeciesBreakRule)i));
        assert(type == (i == BREAK(LIMITS) ? CONSTRAINT_TYPE_SOFT : CONSTRAINT_TYPE_HARD));
    }
}

static void test_names(void) {
    assert(strcmp(standard_counterpoint_species_break_rule_name(BREAK(DOWNBEAT_CONSONANT)), "downbeat-consonant") ==
           0);
    assert(strcmp(standard_counterpoint_species_break_rule_name(BREAK(WEAK_PASSING)), "weak-passing") == 0);
    assert(strcmp(standard_counterpoint_species_break_rule_name(BREAK(LIMITS)), "limits") == 0);
    assert(standard_counterpoint_species_break_rule_name(BREAK(COUNT)) == NULL);
}

static void test_good_exercise_and_no_break(void) {
    GOOD_FOURTH_SPECIES_CANTUS(cantus);
    GOOD_FOURTH_SPECIES_SLOTS(slots);
    EXERCISE(exercise, cantus, slots, CANTUS_LOWER);
    for (int i = 0; i < STANDARD_COUNTERPOINT_SPECIES_BREAK_COUNT; i++) {
        assert(holds((StandardCounterpointSpeciesBreakRule)i, &exercise));
    }

    /* An exercise with no broken bar satisfies all three. */
    const PitchSpelling *three[] = {P(D, 0, 3), P(F, 0, 3), P(E, 0, 3)};
    const StandardCounterpointSlot tied[] = {REST(), ATK(P(A, 0, 3)), TIE(P(A, 0, 3)), ATK(P(D, 0, 4)),
                                             WHOLE(P(D, 0, 4))};
    EXERCISE(tied_exercise, three, tied, CANTUS_LOWER);
    for (int i = 0; i < STANDARD_COUNTERPOINT_SPECIES_BREAK_COUNT; i++) {
        assert(holds((StandardCounterpointSpeciesBreakRule)i, &tied_exercise));
    }
}

static void test_downbeat_consonant(void) {
    const PitchSpelling *cantus[] = {P(C, 0, 3), P(C, 0, 3), P(C, 0, 3), P(C, 0, 3)};

    const StandardCounterpointSlot consonant[] = {REST(), ATK(P(G, 0, 3)), ATK(P(E, 0, 3)), ATK(P(G, 0, 3)),
                                                  TIE(P(G, 0, 3)), ATK(P(E, 0, 3)), WHOLE(P(C, 0, 4))};
    EXERCISE(consonant_exercise, cantus, consonant, CANTUS_LOWER);
    assert(holds(BREAK(DOWNBEAT_CONSONANT), &consonant_exercise));

    /* A fourth above the cantus firmus is a dissonance. */
    const StandardCounterpointSlot dissonant[] = {REST(), ATK(P(G, 0, 3)), ATK(P(F, 0, 3)), ATK(P(G, 0, 3)),
                                                  TIE(P(G, 0, 3)), ATK(P(E, 0, 3)), WHOLE(P(C, 0, 4))};
    EXERCISE(dissonant_exercise, cantus, dissonant, CANTUS_LOWER);
    assert(!holds(BREAK(DOWNBEAT_CONSONANT), &dissonant_exercise));
}

static void test_weak_passing(void) {
    const PitchSpelling *cantus[] = {P(C, 0, 3), P(C, 0, 3), P(C, 0, 3), P(C, 0, 3)};

    /* E3 F3 G3: a passing tone between two consonant downbeats. */
    const StandardCounterpointSlot passing[] = {REST(), ATK(P(G, 0, 3)), ATK(P(E, 0, 3)), ATK(P(F, 0, 3)),
                                                ATK(P(G, 0, 3)), ATK(P(E, 0, 3)), WHOLE(P(C, 0, 4))};
    EXERCISE(passing_exercise, cantus, passing, CANTUS_LOWER);
    assert(holds(BREAK(WEAK_PASSING), &passing_exercise));

    /* E3 F3 E3: a neighbor, not a passing tone. */
    const StandardCounterpointSlot neighbor[] = {REST(), ATK(P(G, 0, 3)), ATK(P(E, 0, 3)), ATK(P(F, 0, 3)),
                                                 ATK(P(E, 0, 3)), ATK(P(G, 0, 3)), WHOLE(P(C, 0, 4))};
    EXERCISE(neighbor_exercise, cantus, neighbor, CANTUS_LOWER);
    assert(!holds(BREAK(WEAK_PASSING), &neighbor_exercise));

    /* A tied next downbeat is not a consonant arrival. */
    const StandardCounterpointSlot tied_next[] = {REST(), ATK(P(G, 0, 3)), ATK(P(E, 0, 3)), ATK(P(F, 0, 3)),
                                                  TIE(P(F, 0, 3)), ATK(P(E, 0, 3)), WHOLE(P(C, 0, 4))};
    EXERCISE(tied_next_exercise, cantus, tied_next, CANTUS_LOWER);
    assert(!holds(BREAK(WEAK_PASSING), &tied_next_exercise));
}

static void test_limits(void) {
    const PitchSpelling *four[] = {P(C, 0, 3), P(C, 0, 3), P(C, 0, 3), P(C, 0, 3)};
    const PitchSpelling *five[] = {P(C, 0, 3), P(C, 0, 3), P(C, 0, 3), P(C, 0, 3), P(C, 0, 3)};

    /* One stretch of two bars. */
    const StandardCounterpointSlot two[] = {REST(), ATK(P(G, 0, 3)), ATK(P(E, 0, 3)), ATK(P(G, 0, 3)),
                                            ATK(P(E, 0, 3)), ATK(P(G, 0, 3)), WHOLE(P(C, 0, 4))};
    EXERCISE(two_bars, four, two, CANTUS_LOWER);
    assert(holds(BREAK(LIMITS), &two_bars));

    /* One stretch of three bars. */
    const StandardCounterpointSlot three[] = {REST(), ATK(P(G, 0, 3)), ATK(P(E, 0, 3)), ATK(P(G, 0, 3)),
                                              ATK(P(E, 0, 3)), ATK(P(G, 0, 3)), ATK(P(E, 0, 3)), ATK(P(G, 0, 3)),
                                              WHOLE(P(C, 0, 4))};
    EXERCISE(three_bars, five, three, CANTUS_LOWER);
    assert(!holds(BREAK(LIMITS), &three_bars));

    /* Two stretches of one bar. */
    const StandardCounterpointSlot stretches[] = {REST(), ATK(P(G, 0, 3)), ATK(P(E, 0, 3)), ATK(P(G, 0, 3)),
                                                  TIE(P(G, 0, 3)), ATK(P(E, 0, 3)), ATK(P(G, 0, 3)), ATK(P(E, 0, 3)),
                                                  WHOLE(P(C, 0, 4))};
    EXERCISE(two_stretches, five, stretches, CANTUS_LOWER);
    assert(!holds(BREAK(LIMITS), &two_stretches));
}

static void test_check_masks(void) {
    const PitchSpelling *four[] = {P(C, 0, 3), P(C, 0, 3), P(C, 0, 3), P(C, 0, 3)};
    const PitchSpelling *five[] = {P(C, 0, 3), P(C, 0, 3), P(C, 0, 3), P(C, 0, 3), P(C, 0, 3)};
    unsigned hard = 0, soft = 0;

    const StandardCounterpointSlot dissonant[] = {REST(), ATK(P(G, 0, 3)), ATK(P(F, 0, 3)), ATK(P(G, 0, 3)),
                                                  TIE(P(G, 0, 3)), ATK(P(E, 0, 3)), WHOLE(P(C, 0, 4))};
    EXERCISE(dissonant_exercise, four, dissonant, CANTUS_LOWER);
    assert(standard_counterpoint_species_break_check(&dissonant_exercise, &hard, &soft));
    assert(hard == (1u << BREAK(DOWNBEAT_CONSONANT)));
    assert(soft == 0);

    const StandardCounterpointSlot three[] = {REST(), ATK(P(G, 0, 3)), ATK(P(E, 0, 3)), ATK(P(G, 0, 3)),
                                              ATK(P(E, 0, 3)), ATK(P(G, 0, 3)), ATK(P(E, 0, 3)), ATK(P(G, 0, 3)),
                                              WHOLE(P(C, 0, 4))};
    EXERCISE(three_bars, five, three, CANTUS_LOWER);
    assert(standard_counterpoint_species_break_check(&three_bars, &hard, &soft));
    assert(hard == 0);
    assert(soft == (1u << BREAK(LIMITS)));
}

static void test_invalid_input(void) {
    const PitchSpelling *cantus[] = {P(C, 0, 3), P(D, 0, 3)};
    const StandardCounterpointSlot too_few[] = {WHOLE(P(E, 0, 3))};
    EXERCISE(invalid, cantus, too_few, CANTUS_LOWER);
    unsigned hard = 5, soft = 5;

    assert(!standard_counterpoint_species_break_check(NULL, &hard, &soft));
    assert(!standard_counterpoint_species_break_check(&invalid, &hard, &soft));
    assert(!standard_counterpoint_species_break_check(&invalid, NULL, &soft));
    assert(hard == 5 && soft == 5);
    assert(!holds(BREAK(LIMITS), &invalid));
    assert(!holds(BREAK(COUNT), &invalid));
}

int main(void) {
    test_rules_reference_framework();
    test_names();
    test_good_exercise_and_no_break();
    test_downbeat_consonant();
    test_weak_passing();
    test_limits();
    test_check_masks();
    test_invalid_input();
    printf("test_species_break_rules (standard-counterpoint): ok\n");
    return 0;
}
