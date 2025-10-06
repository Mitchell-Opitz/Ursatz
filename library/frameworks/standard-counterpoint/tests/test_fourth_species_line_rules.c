#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "fourth_species_line_rules.h"
#include "fourth_species_test_util.h"
#include "rule_check.h"
#include "eval_check.h"

/* Every call to the group's rule_holds below also runs the rule through constraint_evaluate. */
STANDARD_COUNTERPOINT_EVAL_TABLE(g_eval, "fourth-species-line", STANDARD_COUNTERPOINT_FOURTH_SPECIES_LINE_COUNT);
#define standard_counterpoint_fourth_species_line_rule_holds(which, a, b) \
    STANDARD_COUNTERPOINT_EVAL_HOLDS2(g_eval, standard_counterpoint_fourth_species_line_constraint, standard_counterpoint_fourth_species_line_rule_holds, StandardCounterpointFourthSpeciesTonic, which, a, b)

#define LINE(name) STANDARD_COUNTERPOINT_FOURTH_SPECIES_LINE_##name

static bool holds(StandardCounterpointFourthSpeciesLineRule rule,
                  const StandardCounterpointFourthSpeciesExercise *exercise) {
    return standard_counterpoint_fourth_species_line_rule_holds(rule, exercise, P(D, 0, 3));
}

static void test_rules_reference_framework(void) {
    CHECK_RULES_REFERENCE_FRAMEWORK(standard_counterpoint_fourth_species_line_rule,
                                    standard_counterpoint_fourth_species_line_constraint,
                                    STANDARD_COUNTERPOINT_FOURTH_SPECIES_LINE_COUNT);
    for (int i = 0; i < STANDARD_COUNTERPOINT_FOURTH_SPECIES_LINE_COUNT; i++) {
        ConstraintType type = constraint_type(
            standard_counterpoint_fourth_species_line_constraint((StandardCounterpointFourthSpeciesLineRule)i));
        bool soft = i == LINE(DOWNWARD_STEPS) || i == LINE(UPWARD_LEAP);
        assert(type == (soft ? CONSTRAINT_TYPE_SOFT : CONSTRAINT_TYPE_HARD));
    }
}

static void test_names(void) {
    assert(strcmp(standard_counterpoint_fourth_species_line_rule_name(LINE(LEAPS)), "leaps") == 0);
    assert(strcmp(standard_counterpoint_fourth_species_line_rule_name(LINE(FINAL_STEP)), "final-step") == 0);
    assert(strcmp(standard_counterpoint_fourth_species_line_rule_name(LINE(RANGE)), "range") == 0);
    assert(strcmp(standard_counterpoint_fourth_species_line_rule_name(LINE(LEADING_TONE)), "leading-tone") == 0);
    assert(strcmp(standard_counterpoint_fourth_species_line_rule_name(LINE(DOWNWARD_STEPS)), "downward-steps") == 0);
    assert(strcmp(standard_counterpoint_fourth_species_line_rule_name(LINE(UPWARD_LEAP)), "upward-leap") == 0);
    assert(standard_counterpoint_fourth_species_line_rule_name(LINE(COUNT)) == NULL);
}

static void test_good_exercise(void) {
    GOOD_FOURTH_SPECIES_CANTUS(cantus);
    GOOD_FOURTH_SPECIES_SLOTS(slots);
    EXERCISE(exercise, cantus, slots, CANTUS_LOWER);
    for (int i = 0; i < STANDARD_COUNTERPOINT_FOURTH_SPECIES_LINE_COUNT; i++) {
        assert(holds((StandardCounterpointFourthSpeciesLineRule)i, &exercise));
    }
}

static void test_leaps(void) {
    const PitchSpelling *cantus[] = {P(C, 0, 3), P(C, 0, 3), P(C, 0, 3)};

    /* A seventh down. */
    const StandardCounterpointSlot seventh[] = {REST(), ATK(P(E, 0, 4)), TIE(P(E, 0, 4)), ATK(P(F, 0, 3)),
                                                WHOLE(P(C, 0, 4))};
    EXERCISE(seventh_exercise, cantus, seventh, CANTUS_LOWER);
    assert(!holds(LINE(LEAPS), &seventh_exercise));

    /* An ascending minor sixth is allowed; a descending one is not. */
    const StandardCounterpointSlot up[] = {REST(), ATK(P(E, 0, 3)), TIE(P(E, 0, 3)), ATK(P(C, 0, 4)),
                                           WHOLE(P(C, 0, 4))};
    EXERCISE(up_exercise, cantus, up, CANTUS_LOWER);
    assert(holds(LINE(LEAPS), &up_exercise));

    const StandardCounterpointSlot down[] = {REST(), ATK(P(C, 0, 4)), TIE(P(C, 0, 4)), ATK(P(E, 0, 3)),
                                             WHOLE(P(C, 0, 4))};
    EXERCISE(down_exercise, cantus, down, CANTUS_LOWER);
    assert(!holds(LINE(LEAPS), &down_exercise));
}

static void test_cantus_firmus_shaped_rules(void) {
    const PitchSpelling *cantus[] = {P(D, 0, 3), P(E, 0, 3), P(D, 0, 3)};

    /* The last note is approached by a leap. */
    const StandardCounterpointSlot leap_in[] = {REST(), ATK(P(A, 0, 3)), TIE(P(A, 0, 3)), ATK(P(A, 0, 3)),
                                                WHOLE(P(D, 0, 4))};
    EXERCISE(leap_exercise, cantus, leap_in, CANTUS_LOWER);
    assert(!holds(LINE(FINAL_STEP), &leap_exercise));

    /* Eleven steps from lowest to highest. */
    const StandardCounterpointSlot wide[] = {REST(), ATK(P(A, 0, 2)), TIE(P(A, 0, 2)), ATK(P(D, 0, 4)),
                                             WHOLE(P(D, 0, 4))};
    EXERCISE(wide_exercise, cantus, wide, CANTUS_LOWER);
    assert(!holds(LINE(RANGE), &wide_exercise));

    /* A leading tone that does not rise to do. */
    const StandardCounterpointSlot falls[] = {REST(), ATK(P(D, 0, 4)), TIE(P(D, 0, 4)), ATK(P(C, 1, 4)),
                                              ATK(P(A, 0, 3)), TIE(P(A, 0, 3))};
    EXERCISE(falls_exercise, cantus, falls, CANTUS_LOWER);
    assert(!holds(LINE(LEADING_TONE), &falls_exercise));
}

static void test_downward_steps(void) {
    const PitchSpelling *cantus[] = {P(D, 0, 3), P(E, 0, 3), P(D, 0, 3)};
    const StandardCounterpointSlot rising[] = {REST(), ATK(P(A, 0, 3)), TIE(P(A, 0, 3)), ATK(P(B, 0, 3)),
                                               WHOLE(P(C, 0, 4))};
    EXERCISE(rising_exercise, cantus, rising, CANTUS_LOWER);
    assert(!holds(LINE(DOWNWARD_STEPS), &rising_exercise));
}

static void test_upward_leap(void) {
    const PitchSpelling *cantus[] = {P(D, 0, 3), P(E, 0, 3), P(D, 0, 3)};
    const StandardCounterpointSlot stepwise[] = {REST(), ATK(P(D, 0, 4)), TIE(P(D, 0, 4)), ATK(P(C, 1, 4)),
                                                 WHOLE(P(D, 0, 4))};
    EXERCISE(stepwise_exercise, cantus, stepwise, CANTUS_LOWER);
    assert(!holds(LINE(UPWARD_LEAP), &stepwise_exercise));
}

static void test_check_masks(void) {
    const PitchSpelling *cantus[] = {P(C, 0, 3), P(C, 0, 3), P(C, 0, 3)};
    const StandardCounterpointSlot seventh[] = {REST(), ATK(P(E, 0, 4)), TIE(P(E, 0, 4)), ATK(P(F, 0, 3)),
                                                WHOLE(P(C, 0, 4))};
    EXERCISE(seventh_exercise, cantus, seventh, CANTUS_LOWER);
    unsigned hard = 0, soft = 0;
    assert(standard_counterpoint_fourth_species_line_check(&seventh_exercise, P(D, 0, 3), &hard, &soft));
    assert(hard & (1u << LINE(LEAPS)));
    assert(!(soft & (1u << LINE(LEAPS))));

    const PitchSpelling *d_cantus[] = {P(D, 0, 3), P(E, 0, 3), P(D, 0, 3)};
    const StandardCounterpointSlot stepwise[] = {REST(), ATK(P(D, 0, 4)), TIE(P(D, 0, 4)), ATK(P(C, 1, 4)),
                                                 WHOLE(P(D, 0, 4))};
    EXERCISE(stepwise_exercise, d_cantus, stepwise, CANTUS_LOWER);
    assert(standard_counterpoint_fourth_species_line_check(&stepwise_exercise, P(D, 0, 3), &hard, &soft));
    assert(hard == 0);
    assert(soft == (1u << LINE(UPWARD_LEAP)));
}

static void test_invalid_input(void) {
    const PitchSpelling *cantus[] = {P(C, 0, 3), P(D, 0, 3)};
    const StandardCounterpointSlot too_few[] = {WHOLE(P(E, 0, 3))};
    EXERCISE(invalid, cantus, too_few, CANTUS_LOWER);
    GOOD_FOURTH_SPECIES_CANTUS(good_notes);
    GOOD_FOURTH_SPECIES_SLOTS(good_pitches);
    EXERCISE(good, good_notes, good_pitches, CANTUS_LOWER);
    unsigned hard = 5, soft = 5;

    assert(!standard_counterpoint_fourth_species_line_check(NULL, P(D, 0, 3), &hard, &soft));
    assert(!standard_counterpoint_fourth_species_line_check(&invalid, P(D, 0, 3), &hard, &soft));
    assert(!standard_counterpoint_fourth_species_line_check(&good, NULL, &hard, &soft));
    assert(!standard_counterpoint_fourth_species_line_check(&good, P(D, 0, 3), NULL, &soft));
    assert(hard == 5 && soft == 5);
    assert(!standard_counterpoint_fourth_species_line_rule_holds(LINE(LEAPS), &good, NULL));
    assert(!standard_counterpoint_fourth_species_line_rule_holds(LINE(COUNT), &good, P(D, 0, 3)));
}

int main(void) {
    test_rules_reference_framework();
    test_names();
    test_good_exercise();
    test_leaps();
    test_cantus_firmus_shaped_rules();
    test_downward_steps();
    test_upward_leap();
    test_check_masks();
    test_invalid_input();
    printf("test_fourth_species_line_rules (standard-counterpoint): ok\n");
    return 0;
}
