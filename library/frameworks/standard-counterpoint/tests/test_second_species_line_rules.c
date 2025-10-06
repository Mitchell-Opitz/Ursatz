#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "rule_check.h"
#include "second_species_line_rules.h"
#include "second_species_test_util.h"
#include "eval_check.h"

/* Every call to the group's rule_holds below also runs the rule through constraint_evaluate. */
STANDARD_COUNTERPOINT_EVAL_TABLE(g_eval, "second-species-line", STANDARD_COUNTERPOINT_SECOND_SPECIES_LINE_COUNT);
#define standard_counterpoint_second_species_line_rule_holds(which, a, b) \
    STANDARD_COUNTERPOINT_EVAL_HOLDS2(g_eval, standard_counterpoint_second_species_line_constraint, standard_counterpoint_second_species_line_rule_holds, StandardCounterpointSecondSpeciesTonic, which, a, b)

#define WHOLE(pitch) ATK(pitch), TIE(pitch)

#define LINE_RULE(name) STANDARD_COUNTERPOINT_SECOND_SPECIES_LINE_##name

static bool is_hard(int rule) {
    return rule == LINE_RULE(LEAPS) || rule == LINE_RULE(FINAL_STEP) || rule == LINE_RULE(RANGE) ||
           rule == LINE_RULE(LEADING_TONE);
}

static bool holds(StandardCounterpointSecondSpeciesLineRule rule,
                  const StandardCounterpointSecondSpeciesExercise *exercise) {
    return standard_counterpoint_second_species_line_rule_holds(rule, exercise, P(D, 0, 4));
}

static void test_rules_reference_framework(void) {
    CHECK_RULES_REFERENCE_FRAMEWORK(standard_counterpoint_second_species_line_rule,
                                    standard_counterpoint_second_species_line_constraint,
                                    STANDARD_COUNTERPOINT_SECOND_SPECIES_LINE_COUNT);
    for (int i = 0; i < STANDARD_COUNTERPOINT_SECOND_SPECIES_LINE_COUNT; i++) {
        ConstraintType type = constraint_type(
            standard_counterpoint_second_species_line_constraint((StandardCounterpointSecondSpeciesLineRule)i));
        assert(type == (is_hard(i) ? CONSTRAINT_TYPE_HARD : CONSTRAINT_TYPE_SOFT));
    }
}

static void test_names(void) {
    assert(strcmp(standard_counterpoint_second_species_line_rule_name(LINE_RULE(LEAPS)), "leaps") == 0);
    assert(strcmp(standard_counterpoint_second_species_line_rule_name(LINE_RULE(SECONDARY_CLIMAXES)),
                  "secondary-climaxes") == 0);
    assert(standard_counterpoint_second_species_line_rule_name(LINE_RULE(COUNT)) == NULL);
}

static void test_good_line(void) {
    GOOD_SECOND_SPECIES_CANTUS(cantus);
    GOOD_SECOND_SPECIES_SLOTS(slots);
    EXERCISE(exercise, cantus, slots, CANTUS_LOWER);
    unsigned hard = 99, soft = 99;
    assert(standard_counterpoint_second_species_line_check(&exercise, P(D, 0, 4), &hard, &soft));
    assert(hard == 0 && soft == 0);
}

static void test_leaps(void) {
    const PitchSpelling *cantus[] = {P(C, 0, 3), P(D, 0, 3), P(E, 0, 3)};

    /* A leap of a seventh. */
    const StandardCounterpointSlot seventh[] = {ATK(P(A, 0, 3)), ATK(P(G, 0, 4)), ATK(P(F, 0, 4)), ATK(P(G, 0, 4)),
                                                WHOLE(P(A, 0, 4))};
    EXERCISE(seventh_exercise, cantus, seventh, CANTUS_LOWER);
    assert(!holds(LINE_RULE(LEAPS), &seventh_exercise));

    /* The ascending minor sixth is allowed; the descending one is not. */
    const StandardCounterpointSlot ascending[] = {ATK(P(A, 0, 3)), ATK(P(F, 0, 4)), ATK(P(E, 0, 4)), ATK(P(F, 0, 4)),
                                                  WHOLE(P(G, 0, 4))};
    const StandardCounterpointSlot descending[] = {ATK(P(F, 0, 4)), ATK(P(A, 0, 3)), ATK(P(B, 0, 3)), ATK(P(C, 0, 4)),
                                                   WHOLE(P(D, 0, 4))};
    EXERCISE(ascending_exercise, cantus, ascending, CANTUS_LOWER);
    EXERCISE(descending_exercise, cantus, descending, CANTUS_LOWER);
    assert(holds(LINE_RULE(LEAPS), &ascending_exercise));
    assert(!holds(LINE_RULE(LEAPS), &descending_exercise));

    /* A descending sixth is allowed as the leap of a change of register: C4,
       E3, F3 over C3 and D3. Without the step back into the next downbeat it
       is no change of register. */
    const StandardCounterpointSlot register_change[] = {ATK(P(C, 0, 4)), ATK(P(E, 0, 3)), ATK(P(F, 0, 3)),
                                                        ATK(P(G, 0, 3)), WHOLE(P(A, 0, 3))};
    const StandardCounterpointSlot no_step[] = {ATK(P(C, 0, 4)), ATK(P(E, 0, 3)), ATK(P(G, 0, 3)), ATK(P(A, 0, 3)),
                                                WHOLE(P(B, 0, 3))};
    EXERCISE(register_exercise, cantus, register_change, CANTUS_LOWER);
    EXERCISE(no_step_exercise, cantus, no_step, CANTUS_LOWER);
    assert(holds(LINE_RULE(LEAPS), &register_exercise));
    assert(!holds(LINE_RULE(LEAPS), &no_step_exercise));

    /* A leap larger than an octave and an augmented fourth. */
    const StandardCounterpointSlot big[] = {ATK(P(A, 0, 3)), ATK(P(E, 0, 5)), ATK(P(D, 0, 5)), ATK(P(E, 0, 5)),
                                            WHOLE(P(F, 0, 5))};
    const StandardCounterpointSlot tritone[] = {ATK(P(F, 0, 3)), ATK(P(B, 0, 3)), ATK(P(A, 0, 3)), ATK(P(B, 0, 3)),
                                                WHOLE(P(C, 0, 4))};
    EXERCISE(big_exercise, cantus, big, CANTUS_LOWER);
    EXERCISE(tritone_exercise, cantus, tritone, CANTUS_LOWER);
    assert(!holds(LINE_RULE(LEAPS), &big_exercise));
    assert(!holds(LINE_RULE(LEAPS), &tritone_exercise));
}

static void test_cantus_firmus_rules_on_the_line(void) {
    const PitchSpelling *cantus[] = {P(C, 0, 3), P(D, 0, 3), P(E, 0, 3)};

    /* The last note is reached by leap. */
    const StandardCounterpointSlot leap_end[] = {ATK(P(A, 0, 3)), ATK(P(B, 0, 3)), ATK(P(A, 0, 3)), ATK(P(G, 0, 3)),
                                                 WHOLE(P(D, 0, 4))};
    EXERCISE(final_exercise, cantus, leap_end, CANTUS_LOWER);
    assert(!holds(LINE_RULE(FINAL_STEP), &final_exercise));

    /* The line spans an eleventh. */
    const StandardCounterpointSlot wide[] = {ATK(P(A, 0, 3)), ATK(P(B, 0, 3)), ATK(P(C, 0, 4)), ATK(P(D, 0, 4)),
                                             ATK(P(E, 0, 4)), ATK(P(F, 0, 4)), ATK(P(G, 0, 4)), ATK(P(D, 0, 5))};
    const PitchSpelling *long_cantus[] = {P(C, 0, 3), P(D, 0, 3), P(E, 0, 3), P(F, 0, 3)};
    EXERCISE(range_exercise, long_cantus, wide, CANTUS_LOWER);
    assert(!holds(LINE_RULE(RANGE), &range_exercise));

    /* The leading tone does not resolve. */
    const StandardCounterpointSlot unresolved[] = {ATK(P(A, 0, 3)), ATK(P(C, 1, 4)), ATK(P(A, 0, 3)), ATK(P(B, 0, 3)),
                                                   WHOLE(P(D, 0, 4))};
    EXERCISE(leading_exercise, cantus, unresolved, CANTUS_LOWER);
    assert(!holds(LINE_RULE(LEADING_TONE), &leading_exercise));

    /* The climax sounds twice. */
    const StandardCounterpointSlot twice[] = {ATK(P(A, 0, 3)), ATK(P(B, 0, 3)), ATK(P(A, 0, 3)), ATK(P(B, 0, 3)),
                                              WHOLE(P(A, 0, 3))};
    EXERCISE(climax_exercise, cantus, twice, CANTUS_LOWER);
    assert(!holds(LINE_RULE(SINGLE_CLIMAX), &climax_exercise));

    /* A run of four notes sounds twice. */
    const StandardCounterpointSlot lick[] = {ATK(P(A, 0, 3)), ATK(P(B, 0, 3)), ATK(P(C, 0, 4)), ATK(P(B, 0, 3)),
                                             ATK(P(A, 0, 3)), ATK(P(B, 0, 3)), ATK(P(C, 0, 4)), ATK(P(B, 0, 3))};
    const PitchSpelling *lick_cantus[] = {P(C, 0, 3), P(D, 0, 3), P(E, 0, 3), P(F, 0, 3)};
    EXERCISE(lick_exercise, lick_cantus, lick, CANTUS_LOWER);
    assert(!holds(LINE_RULE(NO_REPEATED_MOTIVE), &lick_exercise));
}

static void test_stepwise(void) {
    const PitchSpelling *cantus[] = {P(C, 0, 3), P(D, 0, 3), P(E, 0, 3)};
    const StandardCounterpointSlot leaping[] = {ATK(P(A, 0, 3)), ATK(P(C, 0, 4)), ATK(P(E, 0, 4)), ATK(P(C, 0, 4)),
                                                WHOLE(P(A, 0, 3))};
    EXERCISE(exercise, cantus, leaping, CANTUS_LOWER);
    assert(!holds(LINE_RULE(STEPWISE), &exercise));
}

static void test_leap_direction(void) {
    const PitchSpelling *cantus[] = {P(C, 0, 3), P(D, 0, 3), P(E, 0, 3)};
    /* Steps inside the bars, leaps across the bar lines. */
    const StandardCounterpointSlot across[] = {ATK(P(E, 0, 3)), ATK(P(F, 0, 3)), ATK(P(A, 0, 3)), ATK(P(B, 0, 3)),
                                               WHOLE(P(D, 0, 4))};
    /* A leap inside a bar. */
    const StandardCounterpointSlot inside[] = {ATK(P(E, 0, 3)), ATK(P(G, 0, 3)), ATK(P(A, 0, 3)), ATK(P(B, 0, 3)),
                                               WHOLE(P(C, 0, 4))};
    EXERCISE(across_exercise, cantus, across, CANTUS_LOWER);
    EXERCISE(inside_exercise, cantus, inside, CANTUS_LOWER);
    assert(!holds(LINE_RULE(LEAP_DIRECTION), &across_exercise));
    assert(holds(LINE_RULE(LEAP_DIRECTION), &inside_exercise));
}

static void test_secondary_climaxes(void) {
    const PitchSpelling *cantus[] = {P(C, 0, 3), P(D, 0, 3), P(E, 0, 3), P(F, 0, 3), P(G, 0, 3)};

    /* A line that only rises has no secondary climax. */
    const StandardCounterpointSlot rising[] = {ATK(P(C, 0, 4)), ATK(P(D, 0, 4)), ATK(P(E, 0, 4)), ATK(P(F, 0, 4)),
                                               ATK(P(G, 0, 4)), ATK(P(A, 0, 4)), ATK(P(B, 0, 4)), ATK(P(C, 0, 5)),
                                               WHOLE(P(D, 0, 5))};
    EXERCISE(rising_exercise, cantus, rising, CANTUS_LOWER);
    assert(!holds(LINE_RULE(SECONDARY_CLIMAXES), &rising_exercise));

    /* One secondary climax (E4), below the climax (G4). */
    const StandardCounterpointSlot one_peak[] = {ATK(P(C, 0, 4)), ATK(P(D, 0, 4)), ATK(P(E, 0, 4)), ATK(P(D, 0, 4)),
                                                 ATK(P(E, 0, 4)), ATK(P(F, 0, 4)), ATK(P(G, 0, 4)), ATK(P(F, 0, 4)),
                                                 WHOLE(P(E, 0, 4))};
    EXERCISE(one_peak_exercise, cantus, one_peak, CANTUS_LOWER);
    assert(holds(LINE_RULE(SECONDARY_CLIMAXES), &one_peak_exercise));

    /* Short lines are not asked to have one. */
    const StandardCounterpointSlot short_line[] = {ATK(P(C, 0, 4)), ATK(P(D, 0, 4)), WHOLE(P(E, 0, 4))};
    const PitchSpelling *short_cantus[] = {P(C, 0, 3), P(D, 0, 3)};
    EXERCISE(short_exercise, short_cantus, short_line, CANTUS_LOWER);
    assert(holds(LINE_RULE(SECONDARY_CLIMAXES), &short_exercise));
}

static void test_invalid_input(void) {
    const PitchSpelling *cantus[] = {P(C, 0, 3), P(D, 0, 3)};
    const StandardCounterpointSlot too_few[] = {WHOLE(P(E, 0, 3))};
    const StandardCounterpointSlot slots[] = {ATK(P(E, 0, 3)), ATK(P(F, 0, 3)), WHOLE(P(G, 0, 3))};
    EXERCISE(invalid, cantus, too_few, CANTUS_LOWER);
    EXERCISE(valid, cantus, slots, CANTUS_LOWER);
    unsigned hard = 5, soft = 5;

    assert(!standard_counterpoint_second_species_line_check(NULL, P(D, 0, 4), &hard, &soft));
    assert(!standard_counterpoint_second_species_line_check(&invalid, P(D, 0, 4), &hard, &soft));
    assert(!standard_counterpoint_second_species_line_check(&valid, NULL, &hard, &soft));
    assert(!standard_counterpoint_second_species_line_check(&valid, P(D, 0, 4), NULL, &soft));
    assert(!standard_counterpoint_second_species_line_check(&valid, P(D, 0, 4), &hard, NULL));
    assert(hard == 5 && soft == 5);
    assert(!standard_counterpoint_second_species_line_rule_holds(LINE_RULE(COUNT), &valid, P(D, 0, 4)));
    assert(!standard_counterpoint_second_species_line_rule_holds(LINE_RULE(LEAPS), &valid, NULL));
}

int main(void) {
    test_rules_reference_framework();
    test_names();
    test_good_line();
    test_leaps();
    test_cantus_firmus_rules_on_the_line();
    test_stepwise();
    test_leap_direction();
    test_secondary_climaxes();
    test_invalid_input();
    printf("test_second_species_line_rules (standard-counterpoint): ok\n");
    return 0;
}
