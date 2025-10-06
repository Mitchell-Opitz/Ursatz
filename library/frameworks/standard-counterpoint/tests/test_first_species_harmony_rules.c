#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "counterpoint_test_util.h"
#include "first_species_harmony_rules.h"
#include "first_species_test_util.h"
#include "rule_check.h"
#include "eval_check.h"

/* Every call to the group's rule_holds below also runs the rule through constraint_evaluate. */
STANDARD_COUNTERPOINT_EVAL_TABLE(g_eval, "first-species-harmony", STANDARD_COUNTERPOINT_FIRST_SPECIES_HARMONY_COUNT);
#define standard_counterpoint_first_species_harmony_rule_holds(which, subject) \
    STANDARD_COUNTERPOINT_EVAL_HOLDS(g_eval, standard_counterpoint_first_species_harmony_constraint, standard_counterpoint_first_species_harmony_rule_holds, StandardCounterpointPair, which, subject)

static bool holds(StandardCounterpointFirstSpeciesHarmonyRule rule, const PitchSpelling *const *upper_notes,
                  const PitchSpelling *const *lower_notes, size_t count) {
    StandardCounterpointLine upper = {upper_notes, count};
    StandardCounterpointLine lower = {lower_notes, count};
    StandardCounterpointPair pair = {&upper, &lower};
    return standard_counterpoint_first_species_harmony_rule_holds(rule, &pair);
}

#define HOLDS(rule, upper, lower) holds(rule, upper, lower, sizeof(upper) / sizeof((upper)[0]))

static bool is_soft(int rule) {
    return rule == STANDARD_COUNTERPOINT_FIRST_SPECIES_HARMONY_IMPERFECT_PREFERRED ||
           rule == STANDARD_COUNTERPOINT_FIRST_SPECIES_HARMONY_PERFECT_THEN_IMPERFECT ||
           rule == STANDARD_COUNTERPOINT_FIRST_SPECIES_HARMONY_INTERVAL_VARIETY;
}

static void test_rules_reference_framework(void) {
    CHECK_RULES_REFERENCE_FRAMEWORK(standard_counterpoint_first_species_harmony_rule,
                                    standard_counterpoint_first_species_harmony_constraint,
                                    STANDARD_COUNTERPOINT_FIRST_SPECIES_HARMONY_COUNT);
    for (int i = 0; i < STANDARD_COUNTERPOINT_FIRST_SPECIES_HARMONY_COUNT; i++) {
        ConstraintType type = constraint_type(
            standard_counterpoint_first_species_harmony_constraint((StandardCounterpointFirstSpeciesHarmonyRule)i));
        assert(type == (is_soft(i) ? CONSTRAINT_TYPE_SOFT : CONSTRAINT_TYPE_HARD));
    }
}

static void test_names(void) {
    assert(strcmp(standard_counterpoint_first_species_harmony_rule_name(
                      STANDARD_COUNTERPOINT_FIRST_SPECIES_HARMONY_ALL_CONSONANT),
                  "all-consonant") == 0);
    assert(strcmp(standard_counterpoint_first_species_harmony_rule_name(
                      STANDARD_COUNTERPOINT_FIRST_SPECIES_HARMONY_INTERVAL_VARIETY),
                  "interval-variety") == 0);
    assert(standard_counterpoint_first_species_harmony_rule_name(STANDARD_COUNTERPOINT_FIRST_SPECIES_HARMONY_COUNT) ==
           NULL);
}

static void test_good_exercise(void) {
    FIRST_SPECIES_LOWER(lower_notes);
    FIRST_SPECIES_UPPER(upper_notes);
    StandardCounterpointLine lower = LINE_OF(lower_notes);
    StandardCounterpointLine upper = LINE_OF(upper_notes);
    StandardCounterpointPair pair = {&upper, &lower};
    unsigned hard = 99, soft = 99;
    assert(standard_counterpoint_first_species_harmony_check(&pair, &hard, &soft));
    assert(hard == 0 && soft == 0);
}

static void test_all_consonant(void) {
    const PitchSpelling *lower[] = {P(C, 0, 3), P(D, 0, 3)};
    const PitchSpelling *consonant[] = {P(E, 0, 3), P(F, 0, 3)};
    const PitchSpelling *second[] = {P(E, 0, 3), P(E, 0, 3)};
    const PitchSpelling *fourth_lower[] = {P(C, 0, 3), P(C, 0, 3)};
    const PitchSpelling *fourth[] = {P(E, 0, 3), P(F, 0, 3)};
    assert(HOLDS(STANDARD_COUNTERPOINT_FIRST_SPECIES_HARMONY_ALL_CONSONANT, consonant, lower));
    assert(!HOLDS(STANDARD_COUNTERPOINT_FIRST_SPECIES_HARMONY_ALL_CONSONANT, second, lower));
    /* A perfect fourth is a dissonance. */
    assert(!HOLDS(STANDARD_COUNTERPOINT_FIRST_SPECIES_HARMONY_ALL_CONSONANT, fourth, fourth_lower));
}

static void test_no_repeated_perfect(void) {
    const PitchSpelling *lower[] = {P(C, 0, 3), P(D, 0, 3)};
    const PitchSpelling *fifths[] = {P(G, 0, 3), P(A, 0, 3)};
    const PitchSpelling *fifth_then_twelfth[] = {P(G, 0, 3), P(A, 0, 4)};
    const PitchSpelling *octave_then_fifth[] = {P(C, 0, 4), P(A, 0, 3)};
    const PitchSpelling *static_lower[] = {P(C, 0, 3), P(C, 0, 3)};
    const PitchSpelling *static_fifths[] = {P(G, 0, 3), P(G, 0, 3)};
    const PitchSpelling *unison_lower[] = {P(C, 0, 4), P(D, 0, 4)};
    const PitchSpelling *unisons[] = {P(C, 0, 4), P(D, 0, 4)};
    assert(!HOLDS(STANDARD_COUNTERPOINT_FIRST_SPECIES_HARMONY_NO_REPEATED_PERFECT, fifths, lower));
    /* Compound intervals count as simple: P5 then P12 is P5 then P5. */
    assert(!HOLDS(STANDARD_COUNTERPOINT_FIRST_SPECIES_HARMONY_NO_REPEATED_PERFECT, fifth_then_twelfth, lower));
    /* Two different perfect consonances in a row are allowed. */
    assert(HOLDS(STANDARD_COUNTERPOINT_FIRST_SPECIES_HARMONY_NO_REPEATED_PERFECT, octave_then_fifth, lower));
    /* Whatever the motion, even none at all. */
    assert(!HOLDS(STANDARD_COUNTERPOINT_FIRST_SPECIES_HARMONY_NO_REPEATED_PERFECT, static_fifths, static_lower));
    assert(!HOLDS(STANDARD_COUNTERPOINT_FIRST_SPECIES_HARMONY_NO_REPEATED_PERFECT, unisons, unison_lower));
}

static void test_no_direct_perfects(void) {
    const PitchSpelling *lower[] = {P(C, 0, 3), P(E, 0, 3)};
    const PitchSpelling *direct[] = {P(E, 0, 3), P(B, 0, 3)};
    const PitchSpelling *contrary_lower[] = {P(E, 0, 3), P(D, 0, 3)};
    const PitchSpelling *contrary[] = {P(B, 0, 3), P(D, 0, 4)};
    assert(!HOLDS(STANDARD_COUNTERPOINT_FIRST_SPECIES_HARMONY_NO_DIRECT_PERFECTS, direct, lower));
    assert(HOLDS(STANDARD_COUNTERPOINT_FIRST_SPECIES_HARMONY_NO_DIRECT_PERFECTS, contrary, contrary_lower));
}

static void test_max_same_imperfect_run(void) {
    const PitchSpelling *lower[] = {P(C, 0, 3), P(D, 0, 3), P(E, 0, 3), P(F, 0, 3)};
    const PitchSpelling *four_thirds[] = {P(E, 0, 3), P(F, 0, 3), P(G, 0, 3), P(A, 0, 3)};
    const PitchSpelling *three_thirds[] = {P(E, 0, 3), P(F, 0, 3), P(G, 0, 3), P(D, 0, 4)};
    /* A tenth is a third: compound intervals count as simple, so these four
       still make a run of thirds. */
    const PitchSpelling *tenths[] = {P(E, 0, 4), P(F, 0, 4), P(G, 0, 4), P(A, 0, 3)};
    assert(!HOLDS(STANDARD_COUNTERPOINT_FIRST_SPECIES_HARMONY_MAX_SAME_IMPERFECT_RUN, four_thirds, lower));
    assert(HOLDS(STANDARD_COUNTERPOINT_FIRST_SPECIES_HARMONY_MAX_SAME_IMPERFECT_RUN, three_thirds, lower));
    assert(!HOLDS(STANDARD_COUNTERPOINT_FIRST_SPECIES_HARMONY_MAX_SAME_IMPERFECT_RUN, tenths, lower));
}

static void test_imperfect_preferred(void) {
    const PitchSpelling *lower[] = {P(C, 0, 3), P(D, 0, 3), P(E, 0, 3)};
    const PitchSpelling *interior_perfect[] = {P(E, 0, 3), P(A, 0, 3), P(G, 0, 3)};
    const PitchSpelling *ends_perfect[] = {P(G, 0, 3), P(F, 0, 3), P(G, 0, 3)};
    assert(!HOLDS(STANDARD_COUNTERPOINT_FIRST_SPECIES_HARMONY_IMPERFECT_PREFERRED, interior_perfect, lower));
    /* The first and last intervals are free to be perfect. */
    assert(HOLDS(STANDARD_COUNTERPOINT_FIRST_SPECIES_HARMONY_IMPERFECT_PREFERRED, ends_perfect, lower));
}

static void test_perfect_then_imperfect(void) {
    const PitchSpelling *lower[] = {P(C, 0, 3), P(D, 0, 3)};
    const PitchSpelling *then_perfect[] = {P(G, 0, 3), P(D, 0, 4)};
    const PitchSpelling *then_dissonance[] = {P(G, 0, 3), P(E, 0, 3)};
    const PitchSpelling *then_imperfect[] = {P(G, 0, 3), P(F, 0, 3)};
    const PitchSpelling *perfect_last[] = {P(E, 0, 3), P(D, 0, 4)};
    assert(!HOLDS(STANDARD_COUNTERPOINT_FIRST_SPECIES_HARMONY_PERFECT_THEN_IMPERFECT, then_perfect, lower));
    assert(!HOLDS(STANDARD_COUNTERPOINT_FIRST_SPECIES_HARMONY_PERFECT_THEN_IMPERFECT, then_dissonance, lower));
    assert(HOLDS(STANDARD_COUNTERPOINT_FIRST_SPECIES_HARMONY_PERFECT_THEN_IMPERFECT, then_imperfect, lower));
    /* A perfect consonance at the end has nothing to follow it. */
    assert(HOLDS(STANDARD_COUNTERPOINT_FIRST_SPECIES_HARMONY_PERFECT_THEN_IMPERFECT, perfect_last, lower));
}

static void test_interval_variety(void) {
    const PitchSpelling *lower[] = {P(C, 0, 3), P(D, 0, 3), P(E, 0, 3)};
    const PitchSpelling *two_sizes[] = {P(G, 0, 3), P(F, 0, 3), P(G, 0, 3)};
    const PitchSpelling *three_sizes[] = {P(G, 0, 3), P(F, 0, 3), P(C, 0, 4)};
    const PitchSpelling *short_lower[] = {P(C, 0, 3), P(D, 0, 3)};
    const PitchSpelling *short_varied[] = {P(E, 0, 3), P(G, 0, 3)};
    const PitchSpelling *short_same[] = {P(E, 0, 3), P(F, 0, 3)};
    assert(!HOLDS(STANDARD_COUNTERPOINT_FIRST_SPECIES_HARMONY_INTERVAL_VARIETY, two_sizes, lower));
    assert(HOLDS(STANDARD_COUNTERPOINT_FIRST_SPECIES_HARMONY_INTERVAL_VARIETY, three_sizes, lower));
    /* With fewer than three notes, every interval must differ. */
    assert(HOLDS(STANDARD_COUNTERPOINT_FIRST_SPECIES_HARMONY_INTERVAL_VARIETY, short_varied, short_lower));
    assert(!HOLDS(STANDARD_COUNTERPOINT_FIRST_SPECIES_HARMONY_INTERVAL_VARIETY, short_same, short_lower));
}

static void test_check_masks(void) {
    /* Octave then octave in parallel. */
    const PitchSpelling *lower_notes[] = {P(C, 0, 3), P(D, 0, 3)};
    const PitchSpelling *upper_notes[] = {P(C, 0, 4), P(D, 0, 4)};
    StandardCounterpointLine lower = LINE_OF(lower_notes);
    StandardCounterpointLine upper = LINE_OF(upper_notes);
    StandardCounterpointPair pair = {&upper, &lower};
    unsigned hard = 0, soft = 0;
    assert(standard_counterpoint_first_species_harmony_check(&pair, &hard, &soft));
    assert(hard & (1u << STANDARD_COUNTERPOINT_FIRST_SPECIES_HARMONY_NO_REPEATED_PERFECT));
    assert(hard & (1u << STANDARD_COUNTERPOINT_FIRST_SPECIES_HARMONY_NO_DIRECT_PERFECTS));
    assert(!(hard & (1u << STANDARD_COUNTERPOINT_FIRST_SPECIES_HARMONY_ALL_CONSONANT)));
    assert(soft & (1u << STANDARD_COUNTERPOINT_FIRST_SPECIES_HARMONY_PERFECT_THEN_IMPERFECT));
    assert(soft & (1u << STANDARD_COUNTERPOINT_FIRST_SPECIES_HARMONY_INTERVAL_VARIETY));
    assert(!(soft & (1u << STANDARD_COUNTERPOINT_FIRST_SPECIES_HARMONY_NO_REPEATED_PERFECT)));
}

static void test_invalid_input(void) {
    const PitchSpelling *three[] = {P(C, 0, 3), P(D, 0, 3), P(E, 0, 3)};
    const PitchSpelling *two[] = {P(C, 0, 4), P(D, 0, 4)};
    StandardCounterpointLine lower = LINE_OF(three);
    StandardCounterpointLine short_upper = LINE_OF(two);
    StandardCounterpointPair mismatched = {&short_upper, &lower};
    unsigned hard = 5, soft = 5;

    assert(!standard_counterpoint_first_species_harmony_check(NULL, &hard, &soft));
    assert(!standard_counterpoint_first_species_harmony_check(&mismatched, &hard, &soft));
    assert(hard == 5 && soft == 5);
    assert(!standard_counterpoint_first_species_harmony_rule_holds(
        STANDARD_COUNTERPOINT_FIRST_SPECIES_HARMONY_ALL_CONSONANT, NULL));
    assert(!standard_counterpoint_first_species_harmony_rule_holds(STANDARD_COUNTERPOINT_FIRST_SPECIES_HARMONY_COUNT,
                                                                   &mismatched));
}

int main(void) {
    test_rules_reference_framework();
    test_names();
    test_good_exercise();
    test_all_consonant();
    test_no_repeated_perfect();
    test_no_direct_perfects();
    test_max_same_imperfect_run();
    test_imperfect_preferred();
    test_perfect_then_imperfect();
    test_interval_variety();
    test_check_masks();
    test_invalid_input();
    printf("test_first_species_harmony_rules (standard-counterpoint): ok\n");
    return 0;
}
