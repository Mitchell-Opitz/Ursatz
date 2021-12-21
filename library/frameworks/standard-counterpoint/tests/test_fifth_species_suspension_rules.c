#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "fifth_species_suspension_rules.h"
#include "fifth_species_test_util.h"
#include "rule_check.h"

#define SUSPENSION(name) STANDARD_COUNTERPOINT_FIFTH_SPECIES_SUSPENSION_##name

static bool holds(StandardCounterpointFifthSpeciesSuspensionRule rule,
                  const StandardCounterpointFifthSpeciesExercise *exercise) {
    return standard_counterpoint_fifth_species_suspension_rule_holds(rule, exercise);
}

static void test_rules_reference_framework(void) {
    CHECK_RULES_REFERENCE_FRAMEWORK(standard_counterpoint_fifth_species_suspension_rule,
                                    standard_counterpoint_fifth_species_suspension_constraint,
                                    STANDARD_COUNTERPOINT_FIFTH_SPECIES_SUSPENSION_RULE_COUNT);
    for (int i = 0; i < STANDARD_COUNTERPOINT_FIFTH_SPECIES_SUSPENSION_RULE_COUNT; i++) {
        ConstraintType type = constraint_type(standard_counterpoint_fifth_species_suspension_constraint(
            (StandardCounterpointFifthSpeciesSuspensionRule)i));
        assert(type == (i == SUSPENSION(PROPER) ? CONSTRAINT_TYPE_HARD : CONSTRAINT_TYPE_SOFT));
    }
}

static void test_names(void) {
    assert(strcmp(standard_counterpoint_fifth_species_suspension_rule_name(SUSPENSION(PROPER)), "proper") == 0);
    assert(strcmp(standard_counterpoint_fifth_species_suspension_rule_name(SUSPENSION(USED)), "used") == 0);
    assert(strcmp(standard_counterpoint_fifth_species_suspension_rule_name(SUSPENSION(FEW_NINE_EIGHT)),
                  "few-nine-eight") == 0);
    assert(standard_counterpoint_fifth_species_suspension_rule_name(SUSPENSION(RULE_COUNT)) == NULL);
}

static void test_good_exercise(void) {
    GOOD_FIFTH_SPECIES_CANTUS(cantus);
    GOOD_FIFTH_SPECIES_SLOTS(slots);
    EXERCISE(exercise, cantus, slots, CANTUS_LOWER);
    for (int i = 0; i < STANDARD_COUNTERPOINT_FIFTH_SPECIES_SUSPENSION_RULE_COUNT; i++) {
        assert(holds((StandardCounterpointFifthSpeciesSuspensionRule)i, &exercise));
    }
}

static void test_proper(void) {
    const PitchSpelling *cantus[] = {P(D, 0, 3), P(C, 0, 3), P(C, 0, 3)};

    /* F4 tied over C3 and resolved by a leap. */
    const StandardCounterpointSlot leap[] = {REST4(), HALF(P(F, 0, 4)), T2(P(F, 0, 4)), ATK(P(C, 0, 4)),
                                             T5(P(C, 0, 4)), WHOLE(P(C, 0, 4))};
    EXERCISE(leap_exercise, cantus, leap, CANTUS_LOWER);
    assert(!holds(SUSPENSION(PROPER), &leap_exercise));

    /* The same, resolved by a step down: a 4-3. */
    const StandardCounterpointSlot step[] = {REST4(), HALF(P(F, 0, 4)), T2(P(F, 0, 4)), ATK(P(E, 0, 4)),
                                             T5(P(E, 0, 4)), WHOLE(P(C, 0, 4))};
    EXERCISE(step_exercise, cantus, step, CANTUS_LOWER);
    assert(holds(SUSPENSION(PROPER), &step_exercise));

    /* A tied consonance needs no resolution. */
    const PitchSpelling *c_cantus[] = {P(C, 0, 3), P(C, 0, 3), P(C, 0, 3)};
    const StandardCounterpointSlot consonant[] = {REST4(), HALF(P(E, 0, 4)), T2(P(E, 0, 4)), ATK(P(G, 0, 3)),
                                                  T5(P(G, 0, 3)), WHOLE(P(C, 0, 4))};
    EXERCISE(consonant_exercise, c_cantus, consonant, CANTUS_LOWER);
    assert(holds(SUSPENSION(PROPER), &consonant_exercise));
}

static void test_used(void) {
    /* Five bars of consonances and no suspension. */
    const PitchSpelling *cantus[] = {P(C, 0, 3), P(C, 0, 3), P(C, 0, 3), P(C, 0, 3), P(C, 0, 3)};
    const StandardCounterpointSlot none[] = {REST4(), HALF(P(G, 0, 3)), WHOLE(P(E, 0, 3)), WHOLE(P(G, 0, 3)),
                                             WHOLE(P(E, 0, 3)), WHOLE(P(C, 0, 4))};
    EXERCISE(none_exercise, cantus, none, CANTUS_LOWER);
    assert(!holds(SUSPENSION(USED), &none_exercise));

    /* A short exercise does not need one. */
    const PitchSpelling *short_cantus[] = {P(C, 0, 3), P(C, 0, 3), P(C, 0, 3)};
    const StandardCounterpointSlot short_none[] = {REST4(), HALF(P(G, 0, 3)), WHOLE(P(E, 0, 3)), WHOLE(P(C, 0, 4))};
    EXERCISE(short_exercise, short_cantus, short_none, CANTUS_LOWER);
    assert(holds(SUSPENSION(USED), &short_exercise));
}

static void test_few_nine_eight(void) {
    /* The one suspension is a 9-8. */
    const PitchSpelling *cantus[] = {P(G, 0, 3), P(C, 0, 3), P(C, 0, 3)};
    const StandardCounterpointSlot nine_eight[] = {REST4(), HALF(P(D, 0, 4)), T2(P(D, 0, 4)), ATK(P(C, 0, 4)),
                                                   T5(P(C, 0, 4)), WHOLE(P(C, 0, 4))};
    EXERCISE(nine_eight_exercise, cantus, nine_eight, CANTUS_LOWER);
    assert(holds(SUSPENSION(PROPER), &nine_eight_exercise));
    assert(!holds(SUSPENSION(FEW_NINE_EIGHT), &nine_eight_exercise));
}

static void test_check_masks(void) {
    unsigned hard = 5, soft = 5;

    const PitchSpelling *cantus[] = {P(G, 0, 3), P(C, 0, 3), P(C, 0, 3)};
    const StandardCounterpointSlot nine_eight[] = {REST4(), HALF(P(D, 0, 4)), T2(P(D, 0, 4)), ATK(P(C, 0, 4)),
                                                   T5(P(C, 0, 4)), WHOLE(P(C, 0, 4))};
    EXERCISE(exercise, cantus, nine_eight, CANTUS_LOWER);
    assert(standard_counterpoint_fifth_species_suspension_check(&exercise, &hard, &soft));
    assert(hard == 0);
    assert(soft == (1u << SUSPENSION(FEW_NINE_EIGHT)));

    const PitchSpelling *leap_cantus[] = {P(D, 0, 3), P(C, 0, 3), P(C, 0, 3)};
    const StandardCounterpointSlot leap[] = {REST4(), HALF(P(F, 0, 4)), T2(P(F, 0, 4)), ATK(P(C, 0, 4)),
                                             T5(P(C, 0, 4)), WHOLE(P(C, 0, 4))};
    EXERCISE(improper, leap_cantus, leap, CANTUS_LOWER);
    assert(standard_counterpoint_fifth_species_suspension_check(&improper, &hard, &soft));
    assert(hard == (1u << SUSPENSION(PROPER)));
}

static void test_invalid_input(void) {
    const PitchSpelling *cantus[] = {P(C, 0, 3), P(D, 0, 3)};
    const StandardCounterpointSlot too_few[] = {WHOLE(P(E, 0, 3)), WHOLE(P(E, 0, 3))};
    EXERCISE(invalid, cantus, too_few, CANTUS_LOWER);
    unsigned hard = 5, soft = 5;

    assert(!standard_counterpoint_fifth_species_suspension_check(NULL, &hard, &soft));
    assert(!standard_counterpoint_fifth_species_suspension_check(&invalid, &hard, &soft));
    assert(!standard_counterpoint_fifth_species_suspension_check(&invalid, NULL, &soft));
    assert(hard == 5 && soft == 5);
    assert(!holds(SUSPENSION(USED), &invalid));
    assert(!holds(SUSPENSION(RULE_COUNT), &invalid));
}

int main(void) {
    test_rules_reference_framework();
    test_names();
    test_good_exercise();
    test_proper();
    test_used();
    test_few_nine_eight();
    test_check_masks();
    test_invalid_input();
    printf("test_fifth_species_suspension_rules (standard-counterpoint): ok\n");
    return 0;
}
