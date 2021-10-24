#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "fourth_species_suspension_rules.h"
#include "fourth_species_test_util.h"
#include "rule_check.h"

#define SUSPENSION(name) STANDARD_COUNTERPOINT_FOURTH_SPECIES_SUSPENSION_##name

static bool holds(StandardCounterpointFourthSpeciesSuspensionRule rule,
                  const StandardCounterpointFourthSpeciesExercise *exercise) {
    return standard_counterpoint_fourth_species_suspension_rule_holds(rule, exercise);
}

static void test_rules_reference_framework(void) {
    CHECK_RULES_REFERENCE_FRAMEWORK(standard_counterpoint_fourth_species_suspension_rule,
                                    standard_counterpoint_fourth_species_suspension_constraint,
                                    STANDARD_COUNTERPOINT_FOURTH_SPECIES_SUSPENSION_RULE_COUNT);
    for (int i = 0; i < STANDARD_COUNTERPOINT_FOURTH_SPECIES_SUSPENSION_RULE_COUNT; i++) {
        ConstraintType type = constraint_type(standard_counterpoint_fourth_species_suspension_constraint(
            (StandardCounterpointFourthSpeciesSuspensionRule)i));
        assert(type == (i == SUSPENSION(PROPER) ? CONSTRAINT_TYPE_HARD : CONSTRAINT_TYPE_SOFT));
    }
}

static void test_names(void) {
    assert(strcmp(standard_counterpoint_fourth_species_suspension_rule_name(SUSPENSION(PROPER)), "proper") == 0);
    assert(strcmp(standard_counterpoint_fourth_species_suspension_rule_name(SUSPENSION(MANY)), "many") == 0);
    assert(strcmp(standard_counterpoint_fourth_species_suspension_rule_name(SUSPENSION(FEW_NINE_EIGHT)),
                  "few-nine-eight") == 0);
    assert(standard_counterpoint_fourth_species_suspension_rule_name(SUSPENSION(RULE_COUNT)) == NULL);
}

static void test_good_exercise(void) {
    GOOD_FOURTH_SPECIES_CANTUS(cantus);
    GOOD_FOURTH_SPECIES_SLOTS(slots);
    EXERCISE(exercise, cantus, slots, CANTUS_LOWER);
    for (int i = 0; i < STANDARD_COUNTERPOINT_FOURTH_SPECIES_SUSPENSION_RULE_COUNT; i++) {
        assert(holds((StandardCounterpointFourthSpeciesSuspensionRule)i, &exercise));
    }
}

static void test_proper(void) {
    const PitchSpelling *cantus[] = {P(D, 0, 3), P(F, 0, 3), P(E, 0, 3)};

    /* A tied tritone is a dissonance that is not a suspension. */
    const StandardCounterpointSlot tritone[] = {REST(), ATK(P(B, 0, 3)), TIE(P(B, 0, 3)), ATK(P(A, 0, 3)),
                                                WHOLE(P(D, 0, 4))};
    EXERCISE(tritone_exercise, cantus, tritone, CANTUS_LOWER);
    assert(!holds(SUSPENSION(PROPER), &tritone_exercise));

    /* A new note on the downbeat is a broken bar, judged elsewhere. */
    const StandardCounterpointSlot broken[] = {REST(), ATK(P(A, 0, 3)), ATK(P(G, 0, 3)), ATK(P(A, 0, 3)),
                                               WHOLE(P(B, 0, 3))};
    EXERCISE(broken_exercise, cantus, broken, CANTUS_LOWER);
    assert(holds(SUSPENSION(PROPER), &broken_exercise));

    /* A 7-6 over E3. */
    const StandardCounterpointSlot seven_six[] = {REST(), ATK(P(A, 0, 3)), TIE(P(A, 0, 3)), ATK(P(D, 0, 4)),
                                                  WHOLE(P(D, 0, 4))};
    EXERCISE(good_exercise, cantus, seven_six, CANTUS_LOWER);
    assert(holds(SUSPENSION(PROPER), &good_exercise));
}

static void test_many(void) {
    const PitchSpelling *cantus[] = {P(D, 0, 3), P(F, 0, 3), P(E, 0, 3), P(D, 0, 3)};

    /* Two tied bars, both consonant. */
    const StandardCounterpointSlot consonant[] = {REST(), ATK(P(A, 0, 3)), TIE(P(A, 0, 3)), ATK(P(C, 0, 4)),
                                                  TIE(P(C, 0, 4)), ATK(P(B, 0, 3)), WHOLE(P(D, 0, 4))};
    EXERCISE(consonant_exercise, cantus, consonant, CANTUS_LOWER);
    assert(!holds(SUSPENSION(MANY), &consonant_exercise));

    /* One of the two is a 7-6: half. */
    const StandardCounterpointSlot half[] = {REST(), ATK(P(A, 0, 3)), TIE(P(A, 0, 3)), ATK(P(D, 0, 4)),
                                             TIE(P(D, 0, 4)), ATK(P(C, 0, 4)), WHOLE(P(D, 0, 4))};
    EXERCISE(half_exercise, cantus, half, CANTUS_LOWER);
    assert(holds(SUSPENSION(MANY), &half_exercise));
}

static void test_few_nine_eight(void) {
    /* The one suspension is a 9-8. */
    const PitchSpelling *cantus[] = {P(C, 0, 3), P(D, 0, 3), P(C, 0, 3)};
    const StandardCounterpointSlot nine_eight[] = {REST(), ATK(P(E, 0, 4)), TIE(P(E, 0, 4)), ATK(P(D, 0, 4)),
                                                   WHOLE(P(C, 0, 4))};
    EXERCISE(nine_eight_exercise, cantus, nine_eight, CANTUS_LOWER);
    assert(holds(SUSPENSION(MANY), &nine_eight_exercise));
    assert(!holds(SUSPENSION(FEW_NINE_EIGHT), &nine_eight_exercise));

    /* A 7-6 is preferred. */
    const PitchSpelling *cantus_s[] = {P(D, 0, 3), P(E, 0, 3), P(D, 0, 3)};
    const StandardCounterpointSlot seven_six[] = {REST(), ATK(P(D, 0, 4)), TIE(P(D, 0, 4)), ATK(P(C, 0, 4)),
                                                  WHOLE(P(D, 0, 4))};
    EXERCISE(seven_six_exercise, cantus_s, seven_six, CANTUS_LOWER);
    assert(holds(SUSPENSION(FEW_NINE_EIGHT), &seven_six_exercise));
}

static void test_check_masks(void) {
    const PitchSpelling *cantus[] = {P(C, 0, 3), P(D, 0, 3), P(C, 0, 3)};
    const StandardCounterpointSlot nine_eight[] = {REST(), ATK(P(E, 0, 4)), TIE(P(E, 0, 4)), ATK(P(D, 0, 4)),
                                                   WHOLE(P(C, 0, 4))};
    EXERCISE(exercise, cantus, nine_eight, CANTUS_LOWER);
    unsigned hard = 0, soft = 0;
    assert(standard_counterpoint_fourth_species_suspension_check(&exercise, &hard, &soft));
    assert(hard == 0);
    assert(soft == (1u << SUSPENSION(FEW_NINE_EIGHT)));

    const PitchSpelling *cantus_t[] = {P(D, 0, 3), P(F, 0, 3), P(E, 0, 3)};
    const StandardCounterpointSlot tritone[] = {REST(), ATK(P(B, 0, 3)), TIE(P(B, 0, 3)), ATK(P(A, 0, 3)),
                                                WHOLE(P(D, 0, 4))};
    EXERCISE(improper, cantus_t, tritone, CANTUS_LOWER);
    assert(standard_counterpoint_fourth_species_suspension_check(&improper, &hard, &soft));
    assert(hard == (1u << SUSPENSION(PROPER)));
}

static void test_invalid_input(void) {
    const PitchSpelling *cantus[] = {P(C, 0, 3), P(D, 0, 3)};
    const StandardCounterpointSlot too_few[] = {WHOLE(P(E, 0, 3))};
    EXERCISE(invalid, cantus, too_few, CANTUS_LOWER);
    unsigned hard = 5, soft = 5;

    assert(!standard_counterpoint_fourth_species_suspension_check(NULL, &hard, &soft));
    assert(!standard_counterpoint_fourth_species_suspension_check(&invalid, &hard, &soft));
    assert(!standard_counterpoint_fourth_species_suspension_check(&invalid, NULL, &soft));
    assert(hard == 5 && soft == 5);
    assert(!holds(SUSPENSION(MANY), &invalid));
    assert(!holds(SUSPENSION(RULE_COUNT), &invalid));
}

int main(void) {
    test_rules_reference_framework();
    test_names();
    test_good_exercise();
    test_proper();
    test_many();
    test_few_nine_eight();
    test_check_masks();
    test_invalid_input();
    printf("test_fourth_species_suspension_rules (standard-counterpoint): ok\n");
    return 0;
}
