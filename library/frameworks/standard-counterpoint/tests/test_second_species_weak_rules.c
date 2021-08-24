#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "rule_check.h"
#include "second_species_test_util.h"
#include "second_species_weak_rules.h"

/* A bar that is one whole note: a note on the strong beat tied through the
   weak beat. */
#define WHOLE(pitch) ATK(pitch), TIE(pitch)

#define WEAK(name) STANDARD_COUNTERPOINT_SECOND_SPECIES_WEAK_##name

static void test_rules_reference_framework(void) {
    CHECK_RULES_REFERENCE_FRAMEWORK(standard_counterpoint_second_species_weak_rule,
                                    standard_counterpoint_second_species_weak_constraint,
                                    STANDARD_COUNTERPOINT_SECOND_SPECIES_WEAK_COUNT);
    for (int i = 0; i < STANDARD_COUNTERPOINT_SECOND_SPECIES_WEAK_COUNT; i++) {
        ConstraintType type = constraint_type(
            standard_counterpoint_second_species_weak_constraint((StandardCounterpointSecondSpeciesWeakRule)i));
        assert(type == (i == WEAK(DISSONANT_PASSING) ? CONSTRAINT_TYPE_HARD : CONSTRAINT_TYPE_SOFT));
    }
}

static void test_names(void) {
    assert(strcmp(standard_counterpoint_second_species_weak_rule_name(WEAK(DISSONANT_PASSING)), "dissonant-passing") ==
           0);
    assert(strcmp(standard_counterpoint_second_species_weak_rule_name(WEAK(REGISTER_CHANGE_RARE)),
                  "register-change-rare") == 0);
    assert(standard_counterpoint_second_species_weak_rule_name(WEAK(COUNT)) == NULL);
}

static void test_dissonant_passing(void) {
    const PitchSpelling *cantus[] = {P(C, 0, 3), P(E, 0, 3)};

    /* E3, F3, G3: a dissonant passing tone. */
    const StandardCounterpointSlot passing[] = {ATK(P(E, 0, 3)), ATK(P(F, 0, 3)), WHOLE(P(G, 0, 3))};
    EXERCISE(passing_exercise, cantus, passing, CANTUS_LOWER);
    assert(standard_counterpoint_second_species_weak_rule_holds(WEAK(DISSONANT_PASSING), &passing_exercise));

    /* A dissonance reached by leap. */
    const PitchSpelling *leap_cantus[] = {P(C, 0, 3), P(F, 0, 3)};
    const StandardCounterpointSlot leap[] = {ATK(P(E, 0, 3)), ATK(P(B, 0, 3)), WHOLE(P(A, 0, 3))};
    EXERCISE(leap_exercise, leap_cantus, leap, CANTUS_LOWER);
    assert(!standard_counterpoint_second_species_weak_rule_holds(WEAK(DISSONANT_PASSING), &leap_exercise));

    /* A dissonance that changes direction (a neighbor). */
    const StandardCounterpointSlot neighbor[] = {ATK(P(E, 0, 3)), ATK(P(F, 0, 3)), WHOLE(P(E, 0, 3))};
    EXERCISE(neighbor_exercise, cantus, neighbor, CANTUS_LOWER);
    assert(!standard_counterpoint_second_species_weak_rule_holds(WEAK(DISSONANT_PASSING), &neighbor_exercise));

    /* A dissonant weak beat in the last bar has no downbeat to pass to. */
    const StandardCounterpointSlot last_bar[] = {WHOLE(P(G, 0, 3)), ATK(P(E, 0, 3)), ATK(P(F, 0, 3))};
    EXERCISE(last_exercise, cantus, last_bar, CANTUS_LOWER);
    assert(!standard_counterpoint_second_species_weak_rule_holds(WEAK(DISSONANT_PASSING), &last_exercise));

    /* A dissonant weak beat after a half rest has no downbeat before it. */
    const StandardCounterpointSlot after_rest[] = {REST(), ATK(P(F, 0, 3)), WHOLE(P(G, 0, 3))};
    EXERCISE(rest_exercise, cantus, after_rest, CANTUS_LOWER);
    assert(!standard_counterpoint_second_species_weak_rule_holds(WEAK(DISSONANT_PASSING), &rest_exercise));

    /* A tie is not a new note, so nothing is judged. */
    const StandardCounterpointSlot tied[] = {WHOLE(P(G, 0, 3)), WHOLE(P(B, 0, 3))};
    EXERCISE(tied_exercise, cantus, tied, CANTUS_LOWER);
    assert(standard_counterpoint_second_species_weak_rule_holds(WEAK(DISSONANT_PASSING), &tied_exercise));
}

static void test_pattern_fit(void) {
    const PitchSpelling *cantus[] = {P(C, 0, 3), P(E, 0, 3)};

    /* A consonant passing tone. */
    const StandardCounterpointSlot passing[] = {ATK(P(G, 0, 3)), ATK(P(A, 0, 3)), WHOLE(P(B, 0, 3))};
    EXERCISE(passing_exercise, cantus, passing, CANTUS_LOWER);
    assert(standard_counterpoint_second_species_weak_rule_holds(WEAK(PATTERN_FIT), &passing_exercise));

    /* A repeated note is a consonance that fits no pattern. */
    const StandardCounterpointSlot repeated[] = {ATK(P(E, 0, 3)), ATK(P(E, 0, 3)), WHOLE(P(E, 0, 3))};
    EXERCISE(repeated_exercise, cantus, repeated, CANTUS_LOWER);
    assert(!standard_counterpoint_second_species_weak_rule_holds(WEAK(PATTERN_FIT), &repeated_exercise));

    /* A half rest first bar is passed over; so is a whole note. */
    const StandardCounterpointSlot rest[] = {REST(), ATK(P(A, 0, 3)), WHOLE(P(B, 0, 3))};
    EXERCISE(rest_exercise, cantus, rest, CANTUS_LOWER);
    assert(standard_counterpoint_second_species_weak_rule_holds(WEAK(PATTERN_FIT), &rest_exercise));
    const StandardCounterpointSlot whole[] = {WHOLE(P(G, 0, 3)), WHOLE(P(B, 0, 3))};
    EXERCISE(whole_exercise, cantus, whole, CANTUS_LOWER);
    assert(standard_counterpoint_second_species_weak_rule_holds(WEAK(PATTERN_FIT), &whole_exercise));
}

static void test_mixture(void) {
    const PitchSpelling *cantus[] = {P(C, 0, 3), P(C, 0, 3), P(C, 0, 3), P(C, 0, 3), P(C, 0, 3)};

    /* Four weak beats, all consonant. */
    const StandardCounterpointSlot all_consonant[] = {ATK(P(E, 0, 3)), ATK(P(G, 0, 3)), ATK(P(E, 0, 3)), ATK(P(G, 0, 3)),
                                                      ATK(P(E, 0, 3)), ATK(P(G, 0, 3)), ATK(P(E, 0, 3)), ATK(P(G, 0, 3)),
                                                      WHOLE(P(E, 0, 3))};
    EXERCISE(consonant_exercise, cantus, all_consonant, CANTUS_LOWER);
    assert(!standard_counterpoint_second_species_weak_rule_holds(WEAK(MIXTURE), &consonant_exercise));

    /* One of them a dissonance. */
    const StandardCounterpointSlot mixed[] = {ATK(P(E, 0, 3)), ATK(P(G, 0, 3)), ATK(P(E, 0, 3)), ATK(P(F, 0, 3)),
                                              ATK(P(E, 0, 3)), ATK(P(G, 0, 3)), ATK(P(E, 0, 3)), ATK(P(G, 0, 3)),
                                              WHOLE(P(E, 0, 3))};
    EXERCISE(mixed_exercise, cantus, mixed, CANTUS_LOWER);
    assert(standard_counterpoint_second_species_weak_rule_holds(WEAK(MIXTURE), &mixed_exercise));

    /* Fewer than four weak beats: nothing to mix. */
    const StandardCounterpointSlot few[] = {ATK(P(E, 0, 3)), ATK(P(G, 0, 3)), ATK(P(E, 0, 3)), ATK(P(G, 0, 3)),
                                            ATK(P(E, 0, 3)), ATK(P(G, 0, 3)), WHOLE(P(E, 0, 3)), WHOLE(P(E, 0, 3))};
    EXERCISE(few_exercise, cantus, few, CANTUS_LOWER);
    assert(standard_counterpoint_second_species_weak_rule_holds(WEAK(MIXTURE), &few_exercise));
}

static void test_register_change_rare(void) {
    const PitchSpelling *cantus[] = {P(C, 0, 3), P(D, 0, 3), P(E, 0, 3)};

    /* Two changes of register in a row. */
    const StandardCounterpointSlot twice[] = {ATK(P(G, 0, 3)), ATK(P(G, 0, 4)), ATK(P(F, 0, 4)), ATK(P(F, 0, 3)),
                                              WHOLE(P(G, 0, 3))};
    EXERCISE(twice_exercise, cantus, twice, CANTUS_LOWER);
    assert(!standard_counterpoint_second_species_weak_rule_holds(WEAK(REGISTER_CHANGE_RARE), &twice_exercise));

    /* One. */
    const StandardCounterpointSlot once[] = {ATK(P(G, 0, 3)), ATK(P(G, 0, 4)), ATK(P(F, 0, 4)), ATK(P(G, 0, 4)),
                                             WHOLE(P(G, 0, 4))};
    EXERCISE(once_exercise, cantus, once, CANTUS_LOWER);
    assert(standard_counterpoint_second_species_weak_rule_holds(WEAK(REGISTER_CHANGE_RARE), &once_exercise));
}

static void test_check_masks(void) {
    /* A dissonance reached by leap, in an exercise with no mixture to speak
       of. */
    const PitchSpelling *cantus[] = {P(C, 0, 3), P(F, 0, 3)};
    const StandardCounterpointSlot slots[] = {ATK(P(E, 0, 3)), ATK(P(B, 0, 3)), WHOLE(P(A, 0, 3))};
    EXERCISE(exercise, cantus, slots, CANTUS_LOWER);
    unsigned hard = 0, soft = 0;
    assert(standard_counterpoint_second_species_weak_check(&exercise, &hard, &soft));
    assert(hard == (1u << WEAK(DISSONANT_PASSING)));
    assert(!(soft & (1u << WEAK(DISSONANT_PASSING))));
}

static void test_invalid_input(void) {
    const PitchSpelling *cantus[] = {P(C, 0, 3), P(D, 0, 3)};
    const StandardCounterpointSlot too_few[] = {WHOLE(P(E, 0, 3))};
    EXERCISE(invalid, cantus, too_few, CANTUS_LOWER);
    unsigned hard = 5, soft = 5;

    assert(!standard_counterpoint_second_species_weak_check(NULL, &hard, &soft));
    assert(!standard_counterpoint_second_species_weak_check(&invalid, &hard, &soft));
    assert(hard == 5 && soft == 5);
    assert(!standard_counterpoint_second_species_weak_rule_holds(WEAK(PATTERN_FIT), &invalid));
    assert(!standard_counterpoint_second_species_weak_rule_holds(WEAK(COUNT), &invalid));
}

int main(void) {
    test_rules_reference_framework();
    test_names();
    test_dissonant_passing();
    test_pattern_fit();
    test_mixture();
    test_register_change_rare();
    test_check_masks();
    test_invalid_input();
    printf("test_second_species_weak_rules (standard-counterpoint): ok\n");
    return 0;
}
