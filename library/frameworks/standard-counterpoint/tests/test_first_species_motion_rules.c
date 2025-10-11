#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "counterpoint_test_util.h"
#include "first_species_motion_rules.h"
#include "rule_check.h"
#include "eval_check.h"

/* Every call to the group's rule_holds below also runs the rule through constraint_evaluate. */
STANDARD_COUNTERPOINT_EVAL_TABLE(g_eval, "first-species-motion", STANDARD_COUNTERPOINT_FIRST_SPECIES_MOTION_COUNT);
#define standard_counterpoint_first_species_motion_rule_holds(which, subject) \
    STANDARD_COUNTERPOINT_EVAL_HOLDS(g_eval, standard_counterpoint_first_species_motion_constraint, standard_counterpoint_first_species_motion_rule_holds, StandardCounterpointPair, which, subject)

static bool holds(StandardCounterpointFirstSpeciesMotionRule rule, const PitchSpelling *const *upper_notes,
                  const PitchSpelling *const *lower_notes, size_t count) {
    StandardCounterpointLine upper = {upper_notes, count};
    StandardCounterpointLine lower = {lower_notes, count};
    StandardCounterpointPair pair = {&upper, &lower};
    return standard_counterpoint_first_species_motion_rule_holds(rule, &pair);
}

#define HOLDS(rule, upper, lower) holds(rule, upper, lower, sizeof(upper) / sizeof((upper)[0]))

static void test_rules_reference_framework(void) {
    CHECK_RULES_REFERENCE_FRAMEWORK(standard_counterpoint_first_species_motion_rule,
                                    standard_counterpoint_first_species_motion_constraint,
                                    STANDARD_COUNTERPOINT_FIRST_SPECIES_MOTION_COUNT);
    for (int i = 0; i < STANDARD_COUNTERPOINT_FIRST_SPECIES_MOTION_COUNT; i++) {
        assert(constraint_type(standard_counterpoint_first_species_motion_constraint(
                   (StandardCounterpointFirstSpeciesMotionRule)i)) == CONSTRAINT_TYPE_SOFT);
    }
}

static void test_names(void) {
    assert(strcmp(standard_counterpoint_first_species_motion_rule_name(
                      STANDARD_COUNTERPOINT_FIRST_SPECIES_MOTION_VARIETY),
                  "motion-variety") == 0);
    assert(strcmp(standard_counterpoint_first_species_motion_rule_name(
                      STANDARD_COUNTERPOINT_FIRST_SPECIES_MOTION_SIMILAR_WITH_LARGE_LEAP),
                  "similar-with-large-leap") == 0);
    assert(standard_counterpoint_first_species_motion_rule_name(STANDARD_COUNTERPOINT_FIRST_SPECIES_MOTION_COUNT) ==
           NULL);
}

/* Parallel, similar, contrary, contrary: all three types in four motions. */
#define VARIED_LOWER(name) \
    const PitchSpelling *name[] = {P(C, 0, 3), P(D, 0, 3), P(E, 0, 3), P(D, 0, 3), P(C, 0, 3)}
#define VARIED_UPPER(name) \
    const PitchSpelling *name[] = {P(E, 0, 3), P(F, 0, 3), P(A, 0, 3), P(B, 0, 3), P(C, 0, 4)}

static void test_motion_variety(void) {
    VARIED_LOWER(lower);
    VARIED_UPPER(upper);
    const PitchSpelling *parallel_lower[] = {P(C, 0, 3), P(D, 0, 3), P(E, 0, 3), P(F, 0, 3), P(G, 0, 3)};
    const PitchSpelling *parallel_upper[] = {P(E, 0, 3), P(F, 0, 3), P(G, 0, 3), P(A, 0, 3), P(B, 0, 3)};
    const PitchSpelling *short_lower[] = {P(C, 0, 3), P(D, 0, 3), P(E, 0, 3), P(F, 0, 3)};
    const PitchSpelling *short_upper[] = {P(E, 0, 3), P(F, 0, 3), P(G, 0, 3), P(A, 0, 3)};
    assert(HOLDS(STANDARD_COUNTERPOINT_FIRST_SPECIES_MOTION_VARIETY, upper, lower));
    assert(!HOLDS(STANDARD_COUNTERPOINT_FIRST_SPECIES_MOTION_VARIETY, parallel_upper, parallel_lower));
    /* Three motions are too few to ask for every type. */
    assert(HOLDS(STANDARD_COUNTERPOINT_FIRST_SPECIES_MOTION_VARIETY, short_upper, short_lower));
}

static void test_contrary_preferred(void) {
    VARIED_LOWER(lower);
    VARIED_UPPER(upper);
    const PitchSpelling *parallel_lower[] = {P(C, 0, 3), P(D, 0, 3), P(E, 0, 3), P(F, 0, 3), P(G, 0, 3)};
    const PitchSpelling *parallel_upper[] = {P(E, 0, 3), P(F, 0, 3), P(G, 0, 3), P(A, 0, 3), P(B, 0, 3)};
    assert(HOLDS(STANDARD_COUNTERPOINT_FIRST_SPECIES_MOTION_CONTRARY_PREFERRED, upper, lower));
    assert(!HOLDS(STANDARD_COUNTERPOINT_FIRST_SPECIES_MOTION_CONTRARY_PREFERRED, parallel_upper, parallel_lower));
}

static void test_similar_with_leap(void) {
    const PitchSpelling *lower[] = {P(C, 0, 3), P(D, 0, 3)};
    const PitchSpelling *similar_fourth[] = {P(E, 0, 3), P(A, 0, 3)};
    const PitchSpelling *similar_fifth[] = {P(E, 0, 3), P(B, 0, 3)};
    const PitchSpelling *similar_steps[] = {P(E, 0, 3), P(F, 0, 3)};
    const PitchSpelling *contrary_lower[] = {P(D, 0, 3), P(C, 0, 3)};
    const PitchSpelling *oblique_lower[] = {P(C, 0, 3), P(C, 0, 3)};

    /* A leap of a fourth in similar motion is a leap, but not a large one. */
    assert(!HOLDS(STANDARD_COUNTERPOINT_FIRST_SPECIES_MOTION_SIMILAR_WITH_LEAP, similar_fourth, lower));
    assert(HOLDS(STANDARD_COUNTERPOINT_FIRST_SPECIES_MOTION_SIMILAR_WITH_LARGE_LEAP, similar_fourth, lower));
    assert(!HOLDS(STANDARD_COUNTERPOINT_FIRST_SPECIES_MOTION_SIMILAR_WITH_LEAP, similar_fifth, lower));
    assert(!HOLDS(STANDARD_COUNTERPOINT_FIRST_SPECIES_MOTION_SIMILAR_WITH_LARGE_LEAP, similar_fifth, lower));
    assert(HOLDS(STANDARD_COUNTERPOINT_FIRST_SPECIES_MOTION_SIMILAR_WITH_LEAP, similar_steps, lower));
    /* Contrary and oblique motion may leap. */
    assert(HOLDS(STANDARD_COUNTERPOINT_FIRST_SPECIES_MOTION_SIMILAR_WITH_LEAP, similar_fifth, contrary_lower));
    assert(HOLDS(STANDARD_COUNTERPOINT_FIRST_SPECIES_MOTION_SIMILAR_WITH_LARGE_LEAP, similar_fifth, oblique_lower));
}

static void test_check_masks(void) {
    const PitchSpelling *lower_notes[] = {P(C, 0, 3), P(D, 0, 3), P(E, 0, 3), P(F, 0, 3), P(G, 0, 3)};
    const PitchSpelling *upper_notes[] = {P(E, 0, 3), P(F, 0, 3), P(G, 0, 3), P(A, 0, 3), P(B, 0, 3)};
    StandardCounterpointLine lower = LINE_OF(lower_notes);
    StandardCounterpointLine upper = LINE_OF(upper_notes);
    StandardCounterpointPair pair = {&upper, &lower};
    unsigned violations = 0;
    assert(standard_counterpoint_first_species_motion_check(&pair, &violations));
    assert(violations == ((1u << STANDARD_COUNTERPOINT_FIRST_SPECIES_MOTION_VARIETY) |
                          (1u << STANDARD_COUNTERPOINT_FIRST_SPECIES_MOTION_CONTRARY_PREFERRED)));

    VARIED_LOWER(varied_lower);
    VARIED_UPPER(varied_upper);
    StandardCounterpointLine good_lower = LINE_OF(varied_lower);
    StandardCounterpointLine good_upper = LINE_OF(varied_upper);
    StandardCounterpointPair good = {&good_upper, &good_lower};
    /* The similar motion pairs a step with a leap of a third: a leap, not a
       large one. */
    assert(standard_counterpoint_first_species_motion_check(&good, &violations));
    assert(violations == (1u << STANDARD_COUNTERPOINT_FIRST_SPECIES_MOTION_SIMILAR_WITH_LEAP));
}

static void test_invalid_input(void) {
    const PitchSpelling *three[] = {P(C, 0, 3), P(D, 0, 3), P(E, 0, 3)};
    const PitchSpelling *two[] = {P(C, 0, 4), P(D, 0, 4)};
    StandardCounterpointLine lower = LINE_OF(three);
    StandardCounterpointLine short_upper = LINE_OF(two);
    StandardCounterpointPair mismatched = {&short_upper, &lower};
    unsigned violations = 5;

    assert(!standard_counterpoint_first_species_motion_check(NULL, &violations));
    assert(!standard_counterpoint_first_species_motion_check(&mismatched, &violations));
    assert(!standard_counterpoint_first_species_motion_check(&mismatched, NULL));
    assert(violations == 5);
    assert(!standard_counterpoint_first_species_motion_rule_holds(STANDARD_COUNTERPOINT_FIRST_SPECIES_MOTION_COUNT,
                                                                  &mismatched));
}

int main(void) {
    test_rules_reference_framework();
    test_names();
    test_motion_variety();
    test_contrary_preferred();
    test_similar_with_leap();
    test_check_masks();
    test_invalid_input();
    printf("test_first_species_motion_rules (standard-counterpoint): ok\n");
    return 0;
}
