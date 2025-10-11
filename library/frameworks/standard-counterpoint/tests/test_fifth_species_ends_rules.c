#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "fifth_species_ends_rules.h"
#include "fifth_species_test_util.h"
#include "rule_check.h"
#include "eval_check.h"

/* Every call to the group's rule_holds below also runs the rule through constraint_evaluate. */
STANDARD_COUNTERPOINT_EVAL_TABLE(g_eval, "fifth-species-ends", STANDARD_COUNTERPOINT_FIFTH_SPECIES_ENDS_COUNT);
#define standard_counterpoint_fifth_species_ends_rule_holds(which, a, b) \
    STANDARD_COUNTERPOINT_EVAL_HOLDS2(g_eval, standard_counterpoint_fifth_species_ends_constraint, standard_counterpoint_fifth_species_ends_rule_holds, StandardCounterpointFifthSpeciesTonic, which, a, b)

#define ENDS(name) STANDARD_COUNTERPOINT_FIFTH_SPECIES_ENDS_##name

static bool holds(StandardCounterpointFifthSpeciesEndsRule rule,
                  const StandardCounterpointFifthSpeciesExercise *exercise) {
    return standard_counterpoint_fifth_species_ends_rule_holds(rule, exercise, P(D, 0, 3));
}

static void test_rules_reference_framework(void) {
    CHECK_RULES_REFERENCE_FRAMEWORK(standard_counterpoint_fifth_species_ends_rule,
                                    standard_counterpoint_fifth_species_ends_constraint,
                                    STANDARD_COUNTERPOINT_FIFTH_SPECIES_ENDS_COUNT);
    for (int i = 0; i < STANDARD_COUNTERPOINT_FIFTH_SPECIES_ENDS_COUNT; i++) {
        assert(constraint_type(standard_counterpoint_fifth_species_ends_constraint(
                   (StandardCounterpointFifthSpeciesEndsRule)i)) == CONSTRAINT_TYPE_HARD);
    }
}

static void test_names(void) {
    assert(strcmp(standard_counterpoint_fifth_species_ends_rule_name(ENDS(BEGIN)), "begin") == 0);
    assert(strcmp(standard_counterpoint_fifth_species_ends_rule_name(ENDS(CLAUSULA_VERA)), "clausula-vera") == 0);
    assert(strcmp(standard_counterpoint_fifth_species_ends_rule_name(ENDS(FINAL_BAR)), "final-bar") == 0);
    assert(standard_counterpoint_fifth_species_ends_rule_name(ENDS(COUNT)) == NULL);
}

static void test_good_endings(void) {
    GOOD_FIFTH_SPECIES_CANTUS(good_notes);
    GOOD_FIFTH_SPECIES_SLOTS(good_pitches);
    EXERCISE(good, good_notes, good_pitches, CANTUS_LOWER);
    unsigned violations = 99;
    assert(standard_counterpoint_fifth_species_ends_check(&good, P(D, 0, 3), &violations));
    assert(violations == 0);

    /* The counterpoint below the cantus firmus. */
    const PitchSpelling *cantus[] = {P(D, 0, 4), P(E, 0, 4), P(D, 0, 4)};
    const StandardCounterpointSlot below[] = {REST4(), HALF(P(D, 0, 3)), ATK(P(D, 0, 3)), T5(P(D, 0, 3)),
                                              QUARTER(P(C, 1, 3)), WHOLE(P(D, 0, 3))};
    EXERCISE(below_exercise, cantus, below, CANTUS_UPPER);
    violations = 99;
    assert(standard_counterpoint_fifth_species_ends_check(&below_exercise, P(D, 0, 3), &violations));
    assert(violations == 0);
}

static void test_begin(void) {
    const PitchSpelling *cantus[] = {P(D, 0, 3), P(E, 0, 3), P(D, 0, 3)};
    const StandardCounterpointSlot octave[] = {REST4(), HALF(P(D, 0, 4)), ATK(P(D, 0, 4)), T5(P(D, 0, 4)),
                                               QUARTER(P(C, 1, 4)), WHOLE(P(D, 0, 4))};
    const StandardCounterpointSlot fifth[] = {REST4(), HALF(P(A, 0, 3)), ATK(P(D, 0, 4)), T5(P(D, 0, 4)),
                                              QUARTER(P(C, 1, 4)), WHOLE(P(D, 0, 4))};
    const StandardCounterpointSlot third[] = {REST4(), HALF(P(F, 0, 4)), ATK(P(D, 0, 4)), T5(P(D, 0, 4)),
                                              QUARTER(P(C, 1, 4)), WHOLE(P(D, 0, 4))};
    /* After a rest of a quarter, and without a rest. */
    const StandardCounterpointSlot after_quarter[] = {REST(), REST(), QUARTER(P(A, 0, 3)), HALF(P(A, 0, 3)),
                                                      ATK(P(D, 0, 4)), T5(P(D, 0, 4)), QUARTER(P(C, 1, 4)),
                                                      WHOLE(P(D, 0, 4))};
    const StandardCounterpointSlot no_rest[] = {WHOLE(P(F, 0, 4)), ATK(P(D, 0, 4)), T5(P(D, 0, 4)),
                                                QUARTER(P(C, 1, 4)), WHOLE(P(D, 0, 4))};
    EXERCISE(octave_exercise, cantus, octave, CANTUS_LOWER);
    EXERCISE(fifth_exercise, cantus, fifth, CANTUS_LOWER);
    EXERCISE(third_exercise, cantus, third, CANTUS_LOWER);
    EXERCISE(quarter_exercise, cantus, after_quarter, CANTUS_LOWER);
    EXERCISE(no_rest_exercise, cantus, no_rest, CANTUS_LOWER);
    assert(holds(ENDS(BEGIN), &octave_exercise));
    assert(holds(ENDS(BEGIN), &fifth_exercise));
    assert(!holds(ENDS(BEGIN), &third_exercise));
    assert(holds(ENDS(BEGIN), &quarter_exercise));
    assert(!holds(ENDS(BEGIN), &no_rest_exercise));

    /* A unison is a perfect consonance too. */
    const PitchSpelling *e_cantus[] = {P(D, 0, 4), P(E, 0, 4), P(D, 0, 4)};
    const StandardCounterpointSlot unison[] = {REST4(), HALF(P(D, 0, 4)), ATK(P(D, 0, 4)), T5(P(D, 0, 4)),
                                               QUARTER(P(C, 1, 4)), WHOLE(P(D, 0, 4))};
    EXERCISE(unison_exercise, e_cantus, unison, CANTUS_LOWER);
    assert(holds(ENDS(BEGIN), &unison_exercise));
}

static void test_clausula_vera(void) {
    const PitchSpelling *cantus[] = {P(D, 0, 3), P(E, 0, 3), P(D, 0, 3)};

    const StandardCounterpointSlot good[] = {REST4(), HALF(P(D, 0, 4)), ATK(P(D, 0, 4)), T5(P(D, 0, 4)),
                                             QUARTER(P(C, 1, 4)), WHOLE(P(D, 0, 4))};
    EXERCISE(good_exercise, cantus, good, CANTUS_LOWER);
    assert(holds(ENDS(CLAUSULA_VERA), &good_exercise));

    /* The last note is approached by a leap. */
    const StandardCounterpointSlot leap[] = {REST4(), HALF(P(D, 0, 4)), ATK(P(D, 0, 4)), T5(P(D, 0, 4)),
                                             QUARTER(P(A, 0, 3)), WHOLE(P(D, 0, 4))};
    EXERCISE(leap_exercise, cantus, leap, CANTUS_LOWER);
    assert(!holds(ENDS(CLAUSULA_VERA), &leap_exercise));

    /* The counterpoint steps the same way as the cantus firmus. */
    const StandardCounterpointSlot same_way[] = {REST4(), HALF(P(D, 0, 4)), ATK(P(D, 0, 4)), T5(P(D, 0, 4)),
                                                 QUARTER(P(E, 0, 4)), WHOLE(P(D, 0, 4))};
    EXERCISE(same_way_exercise, cantus, same_way, CANTUS_LOWER);
    assert(!holds(ENDS(CLAUSULA_VERA), &same_way_exercise));
}

static void test_final_bar(void) {
    const PitchSpelling *cantus[] = {P(D, 0, 3), P(E, 0, 3), P(D, 0, 3)};
    const StandardCounterpointSlot on_do[] = {REST4(), HALF(P(D, 0, 4)), ATK(P(D, 0, 4)), T5(P(D, 0, 4)),
                                              QUARTER(P(C, 1, 4)), WHOLE(P(D, 0, 4))};
    const StandardCounterpointSlot on_mi[] = {REST4(), HALF(P(D, 0, 4)), ATK(P(D, 0, 4)), T5(P(D, 0, 4)),
                                              QUARTER(P(C, 1, 4)), WHOLE(P(E, 0, 4))};
    EXERCISE(do_exercise, cantus, on_do, CANTUS_LOWER);
    EXERCISE(mi_exercise, cantus, on_mi, CANTUS_LOWER);
    assert(holds(ENDS(FINAL_BAR), &do_exercise));
    assert(!holds(ENDS(FINAL_BAR), &mi_exercise));
}

static void test_check_mask(void) {
    const PitchSpelling *cantus[] = {P(D, 0, 3), P(E, 0, 3), P(D, 0, 3)};
    const StandardCounterpointSlot slots[] = {REST4(), HALF(P(F, 0, 4)), ATK(P(D, 0, 4)), T5(P(D, 0, 4)),
                                              QUARTER(P(C, 1, 4)), WHOLE(P(D, 0, 4))};
    EXERCISE(exercise, cantus, slots, CANTUS_LOWER);
    unsigned violations = 0;
    assert(standard_counterpoint_fifth_species_ends_check(&exercise, P(D, 0, 3), &violations));
    assert(violations == (1u << ENDS(BEGIN)));
}

static void test_invalid_input(void) {
    const PitchSpelling *cantus[] = {P(D, 0, 3), P(E, 0, 3)};
    const StandardCounterpointSlot too_few[] = {WHOLE(P(E, 0, 3)), WHOLE(P(E, 0, 3))};
    EXERCISE(invalid, cantus, too_few, CANTUS_LOWER);
    const PitchSpelling *good_notes[] = {P(D, 0, 3), P(E, 0, 3), P(D, 0, 3)};
    const StandardCounterpointSlot good_pitches[] = {REST4(), HALF(P(D, 0, 4)), ATK(P(D, 0, 4)), T5(P(D, 0, 4)),
                                                   QUARTER(P(C, 1, 4)), WHOLE(P(D, 0, 4))};
    EXERCISE(good, good_notes, good_pitches, CANTUS_LOWER);
    unsigned violations = 5;

    assert(!standard_counterpoint_fifth_species_ends_check(NULL, P(D, 0, 3), &violations));
    assert(!standard_counterpoint_fifth_species_ends_check(&invalid, P(D, 0, 3), &violations));
    assert(!standard_counterpoint_fifth_species_ends_check(&good, NULL, &violations));
    assert(!standard_counterpoint_fifth_species_ends_check(&good, P(D, 0, 3), NULL));
    assert(violations == 5);
    assert(!standard_counterpoint_fifth_species_ends_rule_holds(ENDS(BEGIN), &good, NULL));
    assert(!standard_counterpoint_fifth_species_ends_rule_holds(ENDS(COUNT), &good, P(D, 0, 3)));
}

int main(void) {
    test_rules_reference_framework();
    test_names();
    test_good_endings();
    test_begin();
    test_clausula_vera();
    test_final_bar();
    test_check_mask();
    test_invalid_input();
    printf("test_fifth_species_ends_rules (standard-counterpoint): ok\n");
    return 0;
}
