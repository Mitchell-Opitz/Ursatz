#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "fifth_species_line_rules.h"
#include "fifth_species_test_util.h"
#include "rule_check.h"
#include "eval_check.h"

/* Every call to the group's rule_holds below also runs the rule through constraint_evaluate. */
STANDARD_COUNTERPOINT_EVAL_TABLE(g_eval, "fifth-species-line", STANDARD_COUNTERPOINT_FIFTH_SPECIES_LINE_COUNT);
#define standard_counterpoint_fifth_species_line_rule_holds(which, a, b) \
    STANDARD_COUNTERPOINT_EVAL_HOLDS2(g_eval, standard_counterpoint_fifth_species_line_constraint, standard_counterpoint_fifth_species_line_rule_holds, StandardCounterpointFifthSpeciesTonic, which, a, b)

#define LINE(name) STANDARD_COUNTERPOINT_FIFTH_SPECIES_LINE_##name

static bool holds(StandardCounterpointFifthSpeciesLineRule rule,
                  const StandardCounterpointFifthSpeciesExercise *exercise) {
    return standard_counterpoint_fifth_species_line_rule_holds(rule, exercise, P(D, 0, 3));
}

static void test_rules_reference_framework(void) {
    CHECK_RULES_REFERENCE_FRAMEWORK(standard_counterpoint_fifth_species_line_rule,
                                    standard_counterpoint_fifth_species_line_constraint,
                                    STANDARD_COUNTERPOINT_FIFTH_SPECIES_LINE_COUNT);
    for (int i = 0; i < STANDARD_COUNTERPOINT_FIFTH_SPECIES_LINE_COUNT; i++) {
        ConstraintType type = constraint_type(
            standard_counterpoint_fifth_species_line_constraint((StandardCounterpointFifthSpeciesLineRule)i));
        assert(type == (i <= LINE(LEADING_TONE) ? CONSTRAINT_TYPE_HARD : CONSTRAINT_TYPE_SOFT));
    }
}

static void test_names(void) {
    assert(strcmp(standard_counterpoint_fifth_species_line_rule_name(LINE(LEAPS)), "leaps") == 0);
    assert(strcmp(standard_counterpoint_fifth_species_line_rule_name(LINE(FINAL_STEP)), "final-step") == 0);
    assert(strcmp(standard_counterpoint_fifth_species_line_rule_name(LINE(RANGE)), "range") == 0);
    assert(strcmp(standard_counterpoint_fifth_species_line_rule_name(LINE(LEADING_TONE)), "leading-tone") == 0);
    assert(strcmp(standard_counterpoint_fifth_species_line_rule_name(LINE(SINGLE_CLIMAX)), "single-climax") == 0);
    assert(strcmp(standard_counterpoint_fifth_species_line_rule_name(LINE(LEAP_RECOVERY)), "leap-recovery") == 0);
    assert(strcmp(standard_counterpoint_fifth_species_line_rule_name(LINE(MOSTLY_STEPWISE)), "mostly-stepwise") == 0);
    assert(strcmp(standard_counterpoint_fifth_species_line_rule_name(LINE(NO_REPEATED_MOTIVE)),
                  "no-repeated-motive") == 0);
    assert(standard_counterpoint_fifth_species_line_rule_name(LINE(COUNT)) == NULL);
}

static void test_good_exercise(void) {
    GOOD_FIFTH_SPECIES_CANTUS(cantus);
    GOOD_FIFTH_SPECIES_SLOTS(slots);
    EXERCISE(exercise, cantus, slots, CANTUS_LOWER);
    for (int i = 0; i < STANDARD_COUNTERPOINT_FIFTH_SPECIES_LINE_COUNT; i++) {
        assert(holds((StandardCounterpointFifthSpeciesLineRule)i, &exercise));
    }
}

static void test_leaps(void) {
    const PitchSpelling *cantus[] = {P(C, 0, 3), P(C, 0, 3), P(C, 0, 3)};

    /* A seventh down. */
    const StandardCounterpointSlot seventh[] = {REST4(), HALF(P(E, 0, 4)), HALF(P(F, 0, 3)), HALF(P(C, 0, 4)),
                                                WHOLE(P(C, 0, 4))};
    EXERCISE(seventh_exercise, cantus, seventh, CANTUS_LOWER);
    assert(!holds(LINE(LEAPS), &seventh_exercise));

    /* An ascending minor sixth is allowed; a descending one is not. */
    const StandardCounterpointSlot up[] = {REST4(), HALF(P(E, 0, 3)), HALF(P(C, 0, 4)), HALF(P(C, 0, 4)),
                                           WHOLE(P(C, 0, 4))};
    EXERCISE(up_exercise, cantus, up, CANTUS_LOWER);
    assert(holds(LINE(LEAPS), &up_exercise));

    const StandardCounterpointSlot down[] = {REST4(), HALF(P(C, 0, 4)), HALF(P(E, 0, 3)), HALF(P(C, 0, 4)),
                                             WHOLE(P(C, 0, 4))};
    EXERCISE(down_exercise, cantus, down, CANTUS_LOWER);
    assert(!holds(LINE(LEAPS), &down_exercise));
}

static void test_cantus_firmus_shaped_rules(void) {
    const PitchSpelling *cantus[] = {P(D, 0, 3), P(E, 0, 3), P(D, 0, 3)};

    /* The last note is approached by a leap. */
    const StandardCounterpointSlot leap_in[] = {REST4(), HALF(P(A, 0, 3)), HALF(P(A, 0, 3)), HALF(P(A, 0, 3)),
                                                WHOLE(P(D, 0, 4))};
    EXERCISE(leap_exercise, cantus, leap_in, CANTUS_LOWER);
    assert(!holds(LINE(FINAL_STEP), &leap_exercise));

    /* Eleven steps from lowest to highest. */
    const StandardCounterpointSlot wide[] = {REST4(), HALF(P(A, 0, 2)), HALF(P(D, 0, 4)), HALF(P(D, 0, 4)),
                                             WHOLE(P(D, 0, 4))};
    EXERCISE(wide_exercise, cantus, wide, CANTUS_LOWER);
    assert(!holds(LINE(RANGE), &wide_exercise));

    /* A leading tone that does not rise to do. */
    const StandardCounterpointSlot falls[] = {REST4(), HALF(P(D, 0, 4)), HALF(P(C, 1, 4)), HALF(P(A, 0, 3)),
                                              WHOLE(P(D, 0, 4))};
    EXERCISE(falls_exercise, cantus, falls, CANTUS_LOWER);
    assert(!holds(LINE(LEADING_TONE), &falls_exercise));

    /* The highest note comes three times. */
    const StandardCounterpointSlot top[] = {REST4(), HALF(P(D, 0, 4)), HALF(P(C, 1, 4)), HALF(P(D, 0, 4)),
                                            WHOLE(P(D, 0, 4))};
    EXERCISE(top_exercise, cantus, top, CANTUS_LOWER);
    assert(!holds(LINE(SINGLE_CLIMAX), &top_exercise));

    /* A leap up of a fourth followed by a step up. */
    const StandardCounterpointSlot unrecovered[] = {REST4(), HALF(P(A, 0, 3)), HALF(P(D, 0, 4)), HALF(P(E, 0, 4)),
                                                    WHOLE(P(D, 0, 4))};
    EXERCISE(unrecovered_exercise, cantus, unrecovered, CANTUS_LOWER);
    assert(!holds(LINE(LEAP_RECOVERY), &unrecovered_exercise));

    /* Nothing but leaps. */
    const StandardCounterpointSlot leaps[] = {REST4(), HALF(P(A, 0, 3)), HALF(P(D, 0, 4)), HALF(P(A, 0, 3)),
                                              WHOLE(P(D, 0, 4))};
    EXERCISE(leaps_exercise, cantus, leaps, CANTUS_LOWER);
    assert(!holds(LINE(MOSTLY_STEPWISE), &leaps_exercise));
}

static void test_no_repeated_motive(void) {
    const PitchSpelling *cantus[] = {P(C, 0, 3), P(C, 0, 3), P(C, 0, 3), P(C, 0, 3)};

    /* A3 B3 C4 B3 comes twice. */
    const StandardCounterpointSlot repeated[] = {REST4(), HALF(P(A, 0, 3)), QUARTER(P(B, 0, 3)), QUARTER(P(C, 0, 4)),
                                                 QUARTER(P(B, 0, 3)), QUARTER(P(A, 0, 3)), QUARTER(P(B, 0, 3)),
                                                 QUARTER(P(C, 0, 4)), QUARTER(P(B, 0, 3)), QUARTER(P(A, 0, 3)),
                                                 WHOLE(P(C, 0, 4))};
    EXERCISE(repeated_exercise, cantus, repeated, CANTUS_LOWER);
    assert(!holds(LINE(NO_REPEATED_MOTIVE), &repeated_exercise));
}

static void test_check_masks(void) {
    const PitchSpelling *cantus[] = {P(C, 0, 3), P(C, 0, 3), P(C, 0, 3)};
    const StandardCounterpointSlot seventh[] = {REST4(), HALF(P(E, 0, 4)), HALF(P(F, 0, 3)), HALF(P(C, 0, 4)),
                                                WHOLE(P(C, 0, 4))};
    EXERCISE(seventh_exercise, cantus, seventh, CANTUS_LOWER);
    unsigned hard = 0, soft = 0;
    assert(standard_counterpoint_fifth_species_line_check(&seventh_exercise, P(D, 0, 3), &hard, &soft));
    assert(hard & (1u << LINE(LEAPS)));
    assert(!(soft & (1u << LINE(LEAPS))));

    const PitchSpelling *d_cantus[] = {P(D, 0, 3), P(E, 0, 3), P(D, 0, 3)};
    const StandardCounterpointSlot top[] = {REST4(), HALF(P(D, 0, 4)), HALF(P(C, 1, 4)), HALF(P(D, 0, 4)),
                                            WHOLE(P(D, 0, 4))};
    EXERCISE(top_exercise, d_cantus, top, CANTUS_LOWER);
    assert(standard_counterpoint_fifth_species_line_check(&top_exercise, P(D, 0, 3), &hard, &soft));
    assert(soft & (1u << LINE(SINGLE_CLIMAX)));
    assert(!(hard & (1u << LINE(SINGLE_CLIMAX))));
}

static void test_invalid_input(void) {
    const PitchSpelling *cantus[] = {P(C, 0, 3), P(D, 0, 3)};
    const StandardCounterpointSlot too_few[] = {WHOLE(P(E, 0, 3)), WHOLE(P(E, 0, 3))};
    EXERCISE(invalid, cantus, too_few, CANTUS_LOWER);
    GOOD_FIFTH_SPECIES_CANTUS(good_notes);
    GOOD_FIFTH_SPECIES_SLOTS(good_pitches);
    EXERCISE(good, good_notes, good_pitches, CANTUS_LOWER);
    unsigned hard = 5, soft = 5;

    assert(!standard_counterpoint_fifth_species_line_check(NULL, P(D, 0, 3), &hard, &soft));
    assert(!standard_counterpoint_fifth_species_line_check(&invalid, P(D, 0, 3), &hard, &soft));
    assert(!standard_counterpoint_fifth_species_line_check(&good, NULL, &hard, &soft));
    assert(!standard_counterpoint_fifth_species_line_check(&good, P(D, 0, 3), NULL, &soft));
    assert(hard == 5 && soft == 5);
    assert(!standard_counterpoint_fifth_species_line_rule_holds(LINE(LEAPS), &good, NULL));
    assert(!standard_counterpoint_fifth_species_line_rule_holds(LINE(COUNT), &good, P(D, 0, 3)));
}

int main(void) {
    test_rules_reference_framework();
    test_names();
    test_good_exercise();
    test_leaps();
    test_cantus_firmus_shaped_rules();
    test_no_repeated_motive();
    test_check_masks();
    test_invalid_input();
    printf("test_fifth_species_line_rules (standard-counterpoint): ok\n");
    return 0;
}
