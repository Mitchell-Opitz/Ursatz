#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "fifth_species_harmony_rules.h"
#include "fifth_species_test_util.h"
#include "rule_check.h"
#include "eval_check.h"

/* Every call to the group's rule_holds below also runs the rule through constraint_evaluate. */
STANDARD_COUNTERPOINT_EVAL_TABLE(g_eval, "fifth-species-harmony", STANDARD_COUNTERPOINT_FIFTH_SPECIES_HARMONY_RULE_COUNT);
#define standard_counterpoint_fifth_species_harmony_rule_holds(which, subject) \
    STANDARD_COUNTERPOINT_EVAL_HOLDS(g_eval, standard_counterpoint_fifth_species_harmony_constraint, standard_counterpoint_fifth_species_harmony_preference, standard_counterpoint_fifth_species_harmony_rule_holds, StandardCounterpointFifthSpeciesExercise, which, subject)

#define HARMONY(name) STANDARD_COUNTERPOINT_FIFTH_SPECIES_HARMONY_##name

static bool holds(StandardCounterpointFifthSpeciesHarmonyRule rule,
                  const StandardCounterpointFifthSpeciesExercise *exercise) {
    return standard_counterpoint_fifth_species_harmony_rule_holds(rule, exercise);
}

static void test_rules_reference_framework(void) {
    CHECK_RULES_REFERENCE_FRAMEWORK(standard_counterpoint_fifth_species_harmony_rule,
                                    standard_counterpoint_fifth_species_harmony_constraint,
                                    STANDARD_COUNTERPOINT_FIFTH_SPECIES_HARMONY_RULE_COUNT);
    for (int i = 0; i < STANDARD_COUNTERPOINT_FIFTH_SPECIES_HARMONY_RULE_COUNT; i++) {
        ConstraintType type = constraint_type(
            standard_counterpoint_fifth_species_harmony_constraint((StandardCounterpointFifthSpeciesHarmonyRule)i));
        assert(type == (i == HARMONY(NO_INTERIOR_UNISON) ? CONSTRAINT_TYPE_SOFT : CONSTRAINT_TYPE_HARD));
    }
}

static void test_names(void) {
    assert(strcmp(standard_counterpoint_fifth_species_harmony_rule_name(HARMONY(DOWNBEAT_CONSONANT)),
                  "downbeat-consonant") == 0);
    assert(strcmp(standard_counterpoint_fifth_species_harmony_rule_name(HARMONY(DISSONANCE_FIGURE)),
                  "dissonance-figure") == 0);
    assert(strcmp(standard_counterpoint_fifth_species_harmony_rule_name(HARMONY(NO_REPEATED_PERFECT)),
                  "no-repeated-perfect") == 0);
    assert(strcmp(standard_counterpoint_fifth_species_harmony_rule_name(HARMONY(NO_DIRECT_PERFECT)),
                  "no-direct-perfect") == 0);
    assert(strcmp(standard_counterpoint_fifth_species_harmony_rule_name(HARMONY(MAX_SAME_IMPERFECT_RUN)),
                  "max-same-imperfect-run") == 0);
    assert(strcmp(standard_counterpoint_fifth_species_harmony_rule_name(HARMONY(NO_INTERIOR_UNISON)),
                  "no-interior-unison") == 0);
    assert(standard_counterpoint_fifth_species_harmony_rule_name(HARMONY(RULE_COUNT)) == NULL);
}

static void test_good_exercise(void) {
    GOOD_FIFTH_SPECIES_CANTUS(cantus);
    GOOD_FIFTH_SPECIES_SLOTS(slots);
    EXERCISE(exercise, cantus, slots, CANTUS_LOWER);
    for (int i = 0; i < STANDARD_COUNTERPOINT_FIFTH_SPECIES_HARMONY_RULE_COUNT; i++) {
        assert(holds((StandardCounterpointFifthSpeciesHarmonyRule)i, &exercise));
    }
}

static void test_downbeat_consonant(void) {
    const PitchSpelling *cantus[] = {P(C, 0, 3), P(C, 0, 3), P(C, 0, 3)};

    /* A new note, D4, a ninth over C3, on the downbeat. */
    const StandardCounterpointSlot dissonant[] = {REST4(), HALF(P(E, 0, 4)), HALF(P(D, 0, 4)), HALF(P(C, 0, 4)),
                                                  WHOLE(P(C, 0, 4))};
    EXERCISE(dissonant_exercise, cantus, dissonant, CANTUS_LOWER);
    assert(!holds(HARMONY(DOWNBEAT_CONSONANT), &dissonant_exercise));

    const StandardCounterpointSlot consonant[] = {REST4(), HALF(P(E, 0, 4)), HALF(P(E, 0, 4)), HALF(P(C, 0, 4)),
                                                  WHOLE(P(C, 0, 4))};
    EXERCISE(consonant_exercise, cantus, consonant, CANTUS_LOWER);
    assert(holds(HARMONY(DOWNBEAT_CONSONANT), &consonant_exercise));
}

static void test_dissonance_figure(void) {
    const PitchSpelling *cantus[] = {P(C, 0, 3), P(C, 0, 3), P(C, 0, 3)};

    /* A passing tone. */
    const StandardCounterpointSlot passing[] = {REST4(), HALF(P(E, 0, 4)), HALF(P(E, 0, 4)), QUARTER(P(D, 0, 4)),
                                                QUARTER(P(C, 0, 4)), WHOLE(P(C, 0, 4))};
    EXERCISE(passing_exercise, cantus, passing, CANTUS_LOWER);
    assert(holds(HARMONY(DISSONANCE_FIGURE), &passing_exercise));

    /* A dissonance reached by leap. */
    const StandardCounterpointSlot leap[] = {REST4(), HALF(P(E, 0, 4)), HALF(P(C, 0, 4)), QUARTER(P(F, 0, 4)),
                                             QUARTER(P(E, 0, 4)), WHOLE(P(C, 0, 4))};
    EXERCISE(leap_exercise, cantus, leap, CANTUS_LOWER);
    assert(!holds(HARMONY(DISSONANCE_FIGURE), &leap_exercise));

    /* A tied dissonance on a downbeat that is not a suspension. */
    const PitchSpelling *tied_cantus[] = {P(D, 0, 3), P(C, 0, 3), P(C, 0, 3)};
    const StandardCounterpointSlot tied[] = {REST4(), HALF(P(F, 0, 4)), T2(P(F, 0, 4)), ATK(P(C, 0, 4)),
                                             T5(P(C, 0, 4)), WHOLE(P(C, 0, 4))};
    EXERCISE(tied_exercise, tied_cantus, tied, CANTUS_LOWER);
    assert(!holds(HARMONY(DISSONANCE_FIGURE), &tied_exercise));
}

static void test_no_repeated_perfect(void) {
    const PitchSpelling *cantus[] = {P(C, 0, 3), P(C, 0, 3), P(C, 0, 3), P(C, 0, 3)};

    /* Two octaves on downbeats in a row. */
    const StandardCounterpointSlot octaves[] = {REST4(), HALF(P(E, 0, 4)), WHOLE(P(C, 0, 4)), WHOLE(P(C, 0, 4)),
                                                WHOLE(P(C, 0, 4))};
    EXERCISE(octaves_exercise, cantus, octaves, CANTUS_LOWER);
    assert(!holds(HARMONY(NO_REPEATED_PERFECT), &octaves_exercise));

    /* An octave, then a fifth. */
    const StandardCounterpointSlot mixed[] = {REST4(), HALF(P(E, 0, 4)), WHOLE(P(C, 0, 4)), WHOLE(P(G, 0, 3)),
                                              WHOLE(P(C, 0, 4))};
    EXERCISE(mixed_exercise, cantus, mixed, CANTUS_LOWER);
    assert(holds(HARMONY(NO_REPEATED_PERFECT), &mixed_exercise));
}

static void test_no_direct_perfect(void) {
    const PitchSpelling *cantus[] = {P(C, 0, 3), P(D, 0, 3), P(C, 0, 3)};

    /* The cantus firmus and the counterpoint both rise into a fifth. */
    const StandardCounterpointSlot similar[] = {REST4(), HALF(P(E, 0, 3)), WHOLE(P(A, 0, 3)), WHOLE(P(C, 0, 4))};
    EXERCISE(similar_exercise, cantus, similar, CANTUS_LOWER);
    assert(!holds(HARMONY(NO_DIRECT_PERFECT), &similar_exercise));

    /* Contrary motion into a fifth. */
    const StandardCounterpointSlot contrary[] = {REST4(), HALF(P(C, 0, 4)), WHOLE(P(A, 0, 3)), WHOLE(P(C, 0, 4))};
    EXERCISE(contrary_exercise, cantus, contrary, CANTUS_LOWER);
    assert(holds(HARMONY(NO_DIRECT_PERFECT), &contrary_exercise));

    /* The same, with the cantus firmus in the upper part. */
    const PitchSpelling *upper_cantus[] = {P(C, 0, 5), P(B, 0, 4), P(C, 0, 5)};
    const StandardCounterpointSlot below[] = {REST4(), HALF(P(A, 0, 4)), WHOLE(P(E, 0, 4)), WHOLE(P(C, 0, 4))};
    EXERCISE(below_exercise, upper_cantus, below, CANTUS_UPPER);
    assert(!holds(HARMONY(NO_DIRECT_PERFECT), &below_exercise));
}

static void test_max_same_imperfect_run(void) {
    const PitchSpelling *cantus[] = {P(C, 0, 3), P(C, 0, 3), P(C, 0, 3), P(C, 0, 3), P(C, 0, 3), P(C, 0, 3)};

    /* Four thirds in a row on the downbeats. */
    const StandardCounterpointSlot four[] = {REST4(), HALF(P(G, 0, 3)), WHOLE(P(E, 0, 3)), WHOLE(P(E, 0, 3)),
                                             WHOLE(P(E, 0, 3)), WHOLE(P(E, 0, 3)), WHOLE(P(C, 0, 4))};
    EXERCISE(four_exercise, cantus, four, CANTUS_LOWER);
    assert(!holds(HARMONY(MAX_SAME_IMPERFECT_RUN), &four_exercise));

    const StandardCounterpointSlot three[] = {REST4(), HALF(P(G, 0, 3)), WHOLE(P(E, 0, 3)), WHOLE(P(E, 0, 3)),
                                              WHOLE(P(E, 0, 3)), WHOLE(P(A, 0, 3)), WHOLE(P(C, 0, 4))};
    EXERCISE(three_exercise, cantus, three, CANTUS_LOWER);
    assert(holds(HARMONY(MAX_SAME_IMPERFECT_RUN), &three_exercise));
}

static void test_no_interior_unison(void) {
    const PitchSpelling *cantus[] = {P(C, 0, 3), P(C, 0, 3), P(C, 0, 3), P(C, 0, 3)};

    const StandardCounterpointSlot unison[] = {REST4(), HALF(P(E, 0, 3)), WHOLE(P(C, 0, 3)), WHOLE(P(E, 0, 3)),
                                               WHOLE(P(C, 0, 4))};
    EXERCISE(unison_exercise, cantus, unison, CANTUS_LOWER);
    assert(!holds(HARMONY(NO_INTERIOR_UNISON), &unison_exercise));

    const StandardCounterpointSlot none[] = {REST4(), HALF(P(E, 0, 3)), WHOLE(P(E, 0, 3)), WHOLE(P(G, 0, 3)),
                                             WHOLE(P(C, 0, 4))};
    EXERCISE(none_exercise, cantus, none, CANTUS_LOWER);
    assert(holds(HARMONY(NO_INTERIOR_UNISON), &none_exercise));
}

static void test_check_masks(void) {
    const PitchSpelling *cantus[] = {P(C, 0, 3), P(C, 0, 3), P(C, 0, 3), P(C, 0, 3)};
    unsigned hard = 5, soft = 5;

    const StandardCounterpointSlot unison[] = {REST4(), HALF(P(E, 0, 3)), WHOLE(P(C, 0, 3)), WHOLE(P(E, 0, 3)),
                                               WHOLE(P(C, 0, 4))};
    EXERCISE(unison_exercise, cantus, unison, CANTUS_LOWER);
    assert(standard_counterpoint_fifth_species_harmony_check(&unison_exercise, &hard, &soft));
    assert(hard == 0);
    assert(soft == (1u << HARMONY(NO_INTERIOR_UNISON)));

    const PitchSpelling *three[] = {P(C, 0, 3), P(C, 0, 3), P(C, 0, 3)};
    const StandardCounterpointSlot dissonant[] = {REST4(), HALF(P(E, 0, 4)), HALF(P(D, 0, 4)), HALF(P(C, 0, 4)),
                                                  WHOLE(P(C, 0, 4))};
    EXERCISE(dissonant_exercise, three, dissonant, CANTUS_LOWER);
    assert(standard_counterpoint_fifth_species_harmony_check(&dissonant_exercise, &hard, &soft));
    assert(hard == ((1u << HARMONY(DOWNBEAT_CONSONANT)) | (1u << HARMONY(DISSONANCE_FIGURE))));
}

static void test_invalid_input(void) {
    const PitchSpelling *cantus[] = {P(C, 0, 3), P(D, 0, 3)};
    const StandardCounterpointSlot too_few[] = {WHOLE(P(E, 0, 3)), WHOLE(P(E, 0, 3))};
    EXERCISE(invalid, cantus, too_few, CANTUS_LOWER);
    unsigned hard = 5, soft = 5;

    assert(!standard_counterpoint_fifth_species_harmony_check(NULL, &hard, &soft));
    assert(!standard_counterpoint_fifth_species_harmony_check(&invalid, &hard, &soft));
    assert(!standard_counterpoint_fifth_species_harmony_check(&invalid, NULL, &soft));
    assert(hard == 5 && soft == 5);
    assert(!holds(HARMONY(NO_INTERIOR_UNISON), &invalid));
    assert(!holds(HARMONY(RULE_COUNT), &invalid));
}

int main(void) {
    test_rules_reference_framework();
    test_names();
    test_good_exercise();
    test_downbeat_consonant();
    test_dissonance_figure();
    test_no_repeated_perfect();
    test_no_direct_perfect();
    test_max_same_imperfect_run();
    test_no_interior_unison();
    test_check_masks();
    test_invalid_input();
    printf("test_fifth_species_harmony_rules (standard-counterpoint): ok\n");
    return 0;
}
