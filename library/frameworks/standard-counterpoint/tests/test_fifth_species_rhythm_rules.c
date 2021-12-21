#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "fifth_species_rhythm_rules.h"
#include "fifth_species_test_util.h"
#include "rule_check.h"

#define RHYTHM(name) STANDARD_COUNTERPOINT_FIFTH_SPECIES_RHYTHM_##name

static bool holds(StandardCounterpointFifthSpeciesRhythmRule rule,
                  const StandardCounterpointFifthSpeciesExercise *exercise) {
    return standard_counterpoint_fifth_species_rhythm_rule_holds(rule, exercise);
}

static void test_rules_reference_framework(void) {
    CHECK_RULES_REFERENCE_FRAMEWORK(standard_counterpoint_fifth_species_rhythm_rule,
                                    standard_counterpoint_fifth_species_rhythm_constraint,
                                    STANDARD_COUNTERPOINT_FIFTH_SPECIES_RHYTHM_RULE_COUNT);
    for (int i = 0; i < STANDARD_COUNTERPOINT_FIFTH_SPECIES_RHYTHM_RULE_COUNT; i++) {
        ConstraintType type = constraint_type(
            standard_counterpoint_fifth_species_rhythm_constraint((StandardCounterpointFifthSpeciesRhythmRule)i));
        assert(type == (i == RHYTHM(EIGHTHS_STEPWISE) ? CONSTRAINT_TYPE_HARD : CONSTRAINT_TYPE_SOFT));
    }
}

static void test_names(void) {
    assert(strcmp(standard_counterpoint_fifth_species_rhythm_rule_name(RHYTHM(EIGHTHS_STEPWISE)), "eighths-stepwise") ==
           0);
    assert(strcmp(standard_counterpoint_fifth_species_rhythm_rule_name(RHYTHM(MIXTURE)), "mixture") == 0);
    assert(strcmp(standard_counterpoint_fifth_species_rhythm_rule_name(RHYTHM(NO_LONG_RUN)), "no-long-run") == 0);
    assert(strcmp(standard_counterpoint_fifth_species_rhythm_rule_name(RHYTHM(EIGHTHS_SPARING)), "eighths-sparing") ==
           0);
    assert(standard_counterpoint_fifth_species_rhythm_rule_name(RHYTHM(RULE_COUNT)) == NULL);
}

static void test_good_exercise(void) {
    GOOD_FIFTH_SPECIES_CANTUS(cantus);
    GOOD_FIFTH_SPECIES_SLOTS(slots);
    EXERCISE(exercise, cantus, slots, CANTUS_LOWER);
    for (int i = 0; i < STANDARD_COUNTERPOINT_FIFTH_SPECIES_RHYTHM_RULE_COUNT; i++) {
        assert(holds((StandardCounterpointFifthSpeciesRhythmRule)i, &exercise));
    }
}

static void test_eighths_stepwise(void) {
    const PitchSpelling *cantus[] = {P(C, 0, 3), P(C, 0, 3), P(C, 0, 3)};

    /* An eighth reached by a leap of a third. */
    const StandardCounterpointSlot leap[] = {REST4(), HALF(P(G, 0, 3)), QUARTER(P(E, 0, 4)), EIGHTH(P(C, 0, 4)),
                                             EIGHTH(P(D, 0, 4)), HALF(P(E, 0, 4)), WHOLE(P(C, 0, 4))};
    EXERCISE(leap_exercise, cantus, leap, CANTUS_LOWER);
    assert(!holds(RHYTHM(EIGHTHS_STEPWISE), &leap_exercise));

    /* A repeated pitch and steps. */
    const StandardCounterpointSlot steps[] = {REST4(), HALF(P(G, 0, 3)), QUARTER(P(E, 0, 4)), EIGHTH(P(E, 0, 4)),
                                              EIGHTH(P(D, 0, 4)), HALF(P(C, 0, 4)), WHOLE(P(C, 0, 4))};
    EXERCISE(steps_exercise, cantus, steps, CANTUS_LOWER);
    assert(holds(RHYTHM(EIGHTHS_STEPWISE), &steps_exercise));
}

static void test_mixture(void) {
    const PitchSpelling *cantus[] = {P(C, 0, 3), P(C, 0, 3), P(C, 0, 3), P(C, 0, 3), P(C, 0, 3), P(C, 0, 3)};

    /* Half and whole notes only. */
    const StandardCounterpointSlot two_values[] = {REST4(), HALF(P(G, 0, 3)), WHOLE(P(E, 0, 3)), HALF(P(G, 0, 3)),
                                                   HALF(P(E, 0, 3)), WHOLE(P(G, 0, 3)), HALF(P(E, 0, 3)),
                                                   HALF(P(G, 0, 3)), WHOLE(P(C, 0, 4))};
    EXERCISE(two_exercise, cantus, two_values, CANTUS_LOWER);
    assert(!holds(RHYTHM(MIXTURE), &two_exercise));

    /* Quarters too. */
    const StandardCounterpointSlot three_values[] = {REST4(), HALF(P(G, 0, 3)), WHOLE(P(E, 0, 3)), HALF(P(G, 0, 3)),
                                                     HALF(P(E, 0, 3)), QUARTER(P(G, 0, 3)), QUARTER(P(E, 0, 3)),
                                                     QUARTER(P(G, 0, 3)), QUARTER(P(E, 0, 3)), HALF(P(E, 0, 3)),
                                                     HALF(P(G, 0, 3)), WHOLE(P(C, 0, 4))};
    EXERCISE(three_exercise, cantus, three_values, CANTUS_LOWER);
    assert(holds(RHYTHM(MIXTURE), &three_exercise));

    /* A short exercise is not asked for a mixture. */
    const PitchSpelling *short_cantus[] = {P(C, 0, 3), P(C, 0, 3), P(C, 0, 3)};
    const StandardCounterpointSlot short_slots[] = {REST4(), HALF(P(G, 0, 3)), WHOLE(P(E, 0, 3)), WHOLE(P(C, 0, 4))};
    EXERCISE(short_exercise, short_cantus, short_slots, CANTUS_LOWER);
    assert(holds(RHYTHM(MIXTURE), &short_exercise));
}

static void test_no_long_run(void) {
    const PitchSpelling *cantus[] = {P(C, 0, 3), P(C, 0, 3), P(C, 0, 3), P(C, 0, 3), P(C, 0, 3), P(C, 0, 3)};

    /* Four whole-note bars in a row. */
    const StandardCounterpointSlot four[] = {REST4(), HALF(P(G, 0, 3)), WHOLE(P(E, 0, 3)), WHOLE(P(E, 0, 3)),
                                             WHOLE(P(E, 0, 3)), WHOLE(P(E, 0, 3)), WHOLE(P(C, 0, 4))};
    EXERCISE(four_exercise, cantus, four, CANTUS_LOWER);
    assert(!holds(RHYTHM(NO_LONG_RUN), &four_exercise));

    const StandardCounterpointSlot three[] = {REST4(), HALF(P(G, 0, 3)), WHOLE(P(E, 0, 3)), WHOLE(P(E, 0, 3)),
                                              QUARTER(P(G, 0, 3)), QUARTER(P(E, 0, 3)), QUARTER(P(G, 0, 3)),
                                              QUARTER(P(E, 0, 3)), WHOLE(P(E, 0, 3)), WHOLE(P(C, 0, 4))};
    EXERCISE(three_exercise, cantus, three, CANTUS_LOWER);
    assert(holds(RHYTHM(NO_LONG_RUN), &three_exercise));
}

static void test_eighths_sparing(void) {
    const PitchSpelling *cantus[] = {P(C, 0, 3), P(C, 0, 3), P(C, 0, 3), P(C, 0, 3), P(C, 0, 3)};

    /* Pairs of eighths in two bars in a row. */
    const StandardCounterpointSlot two[] = {REST4(), HALF(P(G, 0, 3)), QUARTER(P(E, 0, 3)), EIGHTH(P(E, 0, 3)),
                                            EIGHTH(P(E, 0, 3)), HALF(P(E, 0, 3)), QUARTER(P(E, 0, 3)),
                                            EIGHTH(P(E, 0, 3)), EIGHTH(P(E, 0, 3)), HALF(P(E, 0, 3)),
                                            WHOLE(P(E, 0, 3)), WHOLE(P(C, 0, 4))};
    EXERCISE(two_exercise, cantus, two, CANTUS_LOWER);
    assert(!holds(RHYTHM(EIGHTHS_SPARING), &two_exercise));

    const StandardCounterpointSlot one[] = {REST4(), HALF(P(G, 0, 3)), QUARTER(P(E, 0, 3)), EIGHTH(P(E, 0, 3)),
                                            EIGHTH(P(E, 0, 3)), HALF(P(E, 0, 3)), WHOLE(P(E, 0, 3)),
                                            WHOLE(P(E, 0, 3)), WHOLE(P(C, 0, 4))};
    EXERCISE(one_exercise, cantus, one, CANTUS_LOWER);
    assert(holds(RHYTHM(EIGHTHS_SPARING), &one_exercise));
}

static void test_check_masks(void) {
    unsigned hard = 5, soft = 5;

    const PitchSpelling *cantus[] = {P(C, 0, 3), P(C, 0, 3), P(C, 0, 3)};
    const StandardCounterpointSlot leap[] = {REST4(), HALF(P(G, 0, 3)), QUARTER(P(E, 0, 4)), EIGHTH(P(C, 0, 4)),
                                             EIGHTH(P(D, 0, 4)), HALF(P(E, 0, 4)), WHOLE(P(C, 0, 4))};
    EXERCISE(leap_exercise, cantus, leap, CANTUS_LOWER);
    assert(standard_counterpoint_fifth_species_rhythm_check(&leap_exercise, &hard, &soft));
    assert(hard == (1u << RHYTHM(EIGHTHS_STEPWISE)));
    assert(soft == 0);

    /* Half and whole notes only, alternating. */
    const PitchSpelling *six[] = {P(C, 0, 3), P(C, 0, 3), P(C, 0, 3), P(C, 0, 3), P(C, 0, 3), P(C, 0, 3)};
    const StandardCounterpointSlot two_values[] = {REST4(), HALF(P(G, 0, 3)), WHOLE(P(E, 0, 3)), HALF(P(G, 0, 3)),
                                                   HALF(P(E, 0, 3)), WHOLE(P(G, 0, 3)), HALF(P(E, 0, 3)),
                                                   HALF(P(G, 0, 3)), WHOLE(P(C, 0, 4))};
    EXERCISE(two_exercise, six, two_values, CANTUS_LOWER);
    assert(standard_counterpoint_fifth_species_rhythm_check(&two_exercise, &hard, &soft));
    assert(hard == 0);
    assert(soft == (1u << RHYTHM(MIXTURE)));
}

static void test_invalid_input(void) {
    const PitchSpelling *cantus[] = {P(C, 0, 3), P(D, 0, 3)};
    const StandardCounterpointSlot too_few[] = {WHOLE(P(E, 0, 3)), WHOLE(P(E, 0, 3))};
    EXERCISE(invalid, cantus, too_few, CANTUS_LOWER);
    unsigned hard = 5, soft = 5;

    assert(!standard_counterpoint_fifth_species_rhythm_check(NULL, &hard, &soft));
    assert(!standard_counterpoint_fifth_species_rhythm_check(&invalid, &hard, &soft));
    assert(!standard_counterpoint_fifth_species_rhythm_check(&invalid, &hard, NULL));
    assert(hard == 5 && soft == 5);
    assert(!holds(RHYTHM(MIXTURE), &invalid));
    assert(!holds(RHYTHM(RULE_COUNT), &invalid));
}

int main(void) {
    test_rules_reference_framework();
    test_names();
    test_good_exercise();
    test_eighths_stepwise();
    test_mixture();
    test_no_long_run();
    test_eighths_sparing();
    test_check_masks();
    test_invalid_input();
    printf("test_fifth_species_rhythm_rules (standard-counterpoint): ok\n");
    return 0;
}
