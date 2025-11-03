#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "rule_check.h"
#include "third_species_beat_rules.h"
#include "third_species_test_util.h"
#include "eval_check.h"

/* Every call to the group's rule_holds below also runs the rule through constraint_evaluate. */
STANDARD_COUNTERPOINT_EVAL_TABLE(g_eval, "third-species-beat", STANDARD_COUNTERPOINT_THIRD_SPECIES_BEAT_COUNT);
#define standard_counterpoint_third_species_beat_rule_holds(which, subject) \
    STANDARD_COUNTERPOINT_EVAL_HOLDS(g_eval, standard_counterpoint_third_species_beat_constraint, standard_counterpoint_third_species_beat_preference, standard_counterpoint_third_species_beat_rule_holds, StandardCounterpointThirdSpeciesExercise, which, subject)

#define BEAT(name) STANDARD_COUNTERPOINT_THIRD_SPECIES_BEAT_##name

/* Four quarter notes. */
#define BAR(a, b, c, d) ATK(a), ATK(b), ATK(c), ATK(d)

static bool holds(StandardCounterpointThirdSpeciesBeatRule rule,
                  const StandardCounterpointThirdSpeciesExercise *exercise) {
    return standard_counterpoint_third_species_beat_rule_holds(rule, exercise);
}

static void test_rules_reference_framework(void) {
    CHECK_RULES_REFERENCE_FRAMEWORK(standard_counterpoint_third_species_beat_rule,
                                    standard_counterpoint_third_species_beat_constraint,
                                    STANDARD_COUNTERPOINT_THIRD_SPECIES_BEAT_COUNT);
    for (int i = 0; i < STANDARD_COUNTERPOINT_THIRD_SPECIES_BEAT_COUNT; i++) {
        ConstraintType type = constraint_type(
            standard_counterpoint_third_species_beat_constraint((StandardCounterpointThirdSpeciesBeatRule)i));
        assert(type == (i == BEAT(DISSONANCE_FIGURE) ? CONSTRAINT_TYPE_HARD : CONSTRAINT_TYPE_SOFT));
    }
}

static void test_names(void) {
    assert(strcmp(standard_counterpoint_third_species_beat_rule_name(BEAT(DISSONANCE_FIGURE)), "dissonance-figure") ==
           0);
    assert(strcmp(standard_counterpoint_third_species_beat_rule_name(BEAT(LARGE_LEAP_RECOVERY)),
                  "large-leap-recovery") == 0);
    assert(standard_counterpoint_third_species_beat_rule_name(BEAT(COUNT)) == NULL);
}

static void test_dissonance_figure(void) {
    const PitchSpelling *cantus[] = {P(C, 0, 3), P(C, 0, 3)};

    const StandardCounterpointSlot passing[] = {BAR(P(E, 0, 3), P(F, 0, 3), P(G, 0, 3), P(A, 0, 3)), WHOLE(P(G, 0, 3))};
    EXERCISE(passing_exercise, cantus, passing, CANTUS_LOWER);
    assert(holds(BEAT(DISSONANCE_FIGURE), &passing_exercise));

    /* A dissonance reached by leap. */
    const StandardCounterpointSlot leap[] = {BAR(P(G, 0, 3), P(B, 0, 3), P(A, 0, 3), P(G, 0, 3)), WHOLE(P(G, 0, 3))};
    EXERCISE(leap_exercise, cantus, leap, CANTUS_LOWER);
    assert(!holds(BEAT(DISSONANCE_FIGURE), &leap_exercise));

    /* Three dissonances in a row, over E3. */
    const PitchSpelling *e_cantus[] = {P(E, 0, 3), P(E, 0, 3)};
    const StandardCounterpointSlot three[] = {BAR(P(G, 0, 3), P(A, 0, 3), P(B, -1, 3), P(A, 0, 3)), WHOLE(P(G, 0, 3))};
    EXERCISE(three_exercise, e_cantus, three, CANTUS_LOWER);
    assert(!holds(BEAT(DISSONANCE_FIGURE), &three_exercise));

    /* A nota cambiata is a figure. */
    const PitchSpelling *cambiata_cantus[] = {P(C, 0, 3), P(E, 0, 3)};
    const StandardCounterpointSlot cambiata[] = {BAR(P(C, 0, 4), P(B, 0, 3), P(G, 0, 3), P(A, 0, 3)), WHOLE(P(B, 0, 3))};
    EXERCISE(cambiata_exercise, cambiata_cantus, cambiata, CANTUS_LOWER);
    assert(holds(BEAT(DISSONANCE_FIGURE), &cambiata_exercise));

    /* A whole note's ties are not new notes. */
    const StandardCounterpointSlot whole[] = {WHOLE(P(G, 0, 3)), WHOLE(P(E, 0, 3))};
    EXERCISE(whole_exercise, cantus, whole, CANTUS_LOWER);
    assert(holds(BEAT(DISSONANCE_FIGURE), &whole_exercise));
}

static void test_mixture(void) {
    const PitchSpelling *cantus[] = {P(C, 0, 3), P(C, 0, 3), P(C, 0, 3), P(C, 0, 3)};

    /* Four bars, nothing dissonant on beats 2 to 4. */
    const StandardCounterpointSlot consonant[] = {BAR(P(E, 0, 3), P(G, 0, 3), P(E, 0, 3), P(G, 0, 3)),
                                                  BAR(P(E, 0, 3), P(G, 0, 3), P(E, 0, 3), P(G, 0, 3)),
                                                  BAR(P(E, 0, 3), P(G, 0, 3), P(E, 0, 3), P(G, 0, 3)),
                                                  WHOLE(P(E, 0, 3))};
    EXERCISE(consonant_exercise, cantus, consonant, CANTUS_LOWER);
    assert(!holds(BEAT(MIXTURE), &consonant_exercise));

    /* One passing dissonance. */
    const StandardCounterpointSlot mixed[] = {BAR(P(E, 0, 3), P(G, 0, 3), P(E, 0, 3), P(G, 0, 3)),
                                              BAR(P(E, 0, 3), P(F, 0, 3), P(G, 0, 3), P(A, 0, 3)),
                                              BAR(P(E, 0, 3), P(G, 0, 3), P(E, 0, 3), P(G, 0, 3)),
                                              WHOLE(P(E, 0, 3))};
    EXERCISE(mixed_exercise, cantus, mixed, CANTUS_LOWER);
    assert(holds(BEAT(MIXTURE), &mixed_exercise));

    /* Fewer than four bars: nothing to mix. */
    const PitchSpelling *short_cantus[] = {P(C, 0, 3), P(C, 0, 3), P(C, 0, 3)};
    const StandardCounterpointSlot few[] = {BAR(P(E, 0, 3), P(G, 0, 3), P(E, 0, 3), P(G, 0, 3)),
                                            BAR(P(E, 0, 3), P(G, 0, 3), P(E, 0, 3), P(G, 0, 3)), WHOLE(P(E, 0, 3))};
    EXERCISE(few_exercise, short_cantus, few, CANTUS_LOWER);
    assert(holds(BEAT(MIXTURE), &few_exercise));
}

static void test_large_leap_recovery(void) {
    const PitchSpelling *cantus[] = {P(C, 0, 3), P(C, 0, 3)};

    /* A leap of a fourth up, then a step back down. */
    const StandardCounterpointSlot recovered[] = {BAR(P(E, 0, 3), P(A, 0, 3), P(G, 0, 3), P(A, 0, 3)), WHOLE(P(A, 0, 3))};
    EXERCISE(recovered_exercise, cantus, recovered, CANTUS_LOWER);
    assert(holds(BEAT(LARGE_LEAP_RECOVERY), &recovered_exercise));

    /* The line goes on up after the leap. */
    const StandardCounterpointSlot continued[] = {BAR(P(E, 0, 3), P(A, 0, 3), P(B, 0, 3), P(A, 0, 3)), WHOLE(P(A, 0, 3))};
    EXERCISE(continued_exercise, cantus, continued, CANTUS_LOWER);
    assert(!holds(BEAT(LARGE_LEAP_RECOVERY), &continued_exercise));

    /* A leap of a third needs no recovery. */
    const StandardCounterpointSlot third[] = {BAR(P(E, 0, 3), P(G, 0, 3), P(A, 0, 3), P(B, 0, 3)), WHOLE(P(C, 0, 4))};
    EXERCISE(third_exercise, cantus, third, CANTUS_LOWER);
    assert(holds(BEAT(LARGE_LEAP_RECOVERY), &third_exercise));

    /* A leap into the last note has nothing after it. */
    const StandardCounterpointSlot last[] = {BAR(P(E, 0, 3), P(F, 0, 3), P(G, 0, 3), P(A, 0, 3)), WHOLE(P(E, 0, 3))};
    EXERCISE(last_exercise, cantus, last, CANTUS_LOWER);
    assert(holds(BEAT(LARGE_LEAP_RECOVERY), &last_exercise));
}

static void test_check_masks(void) {
    /* A dissonance reached by leap in an exercise too short for a mixture. */
    const PitchSpelling *cantus[] = {P(C, 0, 3), P(C, 0, 3)};
    const StandardCounterpointSlot slots[] = {BAR(P(G, 0, 3), P(B, 0, 3), P(A, 0, 3), P(G, 0, 3)), WHOLE(P(G, 0, 3))};
    EXERCISE(exercise, cantus, slots, CANTUS_LOWER);
    unsigned hard = 0, soft = 0;
    assert(standard_counterpoint_third_species_beat_check(&exercise, &hard, &soft));
    assert(hard == (1u << BEAT(DISSONANCE_FIGURE)));
    assert(!(soft & (1u << BEAT(DISSONANCE_FIGURE))));
}

static void test_invalid_input(void) {
    const PitchSpelling *cantus[] = {P(C, 0, 3), P(D, 0, 3)};
    const StandardCounterpointSlot too_few[] = {WHOLE(P(E, 0, 3))};
    EXERCISE(invalid, cantus, too_few, CANTUS_LOWER);
    unsigned hard = 5, soft = 5;

    assert(!standard_counterpoint_third_species_beat_check(NULL, &hard, &soft));
    assert(!standard_counterpoint_third_species_beat_check(&invalid, &hard, &soft));
    assert(hard == 5 && soft == 5);
    assert(!holds(BEAT(MIXTURE), &invalid));
    assert(!holds(BEAT(COUNT), &invalid));
}

int main(void) {
    test_rules_reference_framework();
    test_names();
    test_dissonance_figure();
    test_mixture();
    test_large_leap_recovery();
    test_check_masks();
    test_invalid_input();
    printf("test_third_species_beat_rules (standard-counterpoint): ok\n");
    return 0;
}
