#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "counterpoint_test_util.h"
#include "harmonic_rules.h"
#include "rule_check.h"
#include "eval_check.h"

/* Every call to the group's rule_holds below also runs the rule through constraint_evaluate. */
STANDARD_COUNTERPOINT_EVAL_TABLE(g_eval, "harmonic", STANDARD_COUNTERPOINT_HARMONIC_RULE_COUNT);
#define standard_counterpoint_harmonic_rule_holds(which, subject) \
    STANDARD_COUNTERPOINT_EVAL_HOLDS(g_eval, standard_counterpoint_harmonic_constraint, standard_counterpoint_harmonic_rule_holds, StandardCounterpointPair, which, subject)

static bool holds(StandardCounterpointHarmonicRule rule, const PitchSpelling *const *upper_notes,
                  const PitchSpelling *const *lower_notes, size_t count) {
    StandardCounterpointLine upper = {upper_notes, count};
    StandardCounterpointLine lower = {lower_notes, count};
    StandardCounterpointPair pair = {&upper, &lower};
    return standard_counterpoint_harmonic_rule_holds(rule, &pair);
}

#define HOLDS(rule, upper, lower) holds(rule, upper, lower, sizeof(upper) / sizeof((upper)[0]))

static bool is_soft(int rule) {
    return rule == STANDARD_COUNTERPOINT_HARMONIC_RULE_MOSTLY_WITHIN_OCTAVE ||
           rule == STANDARD_COUNTERPOINT_HARMONIC_RULE_DIRECT_PERFECTS ||
           rule == STANDARD_COUNTERPOINT_HARMONIC_RULE_INTERIOR_UNISONS ||
           rule == STANDARD_COUNTERPOINT_HARMONIC_RULE_CLIMAX_COORDINATION;
}

/* A first-species exercise over the Dorian cantus firmus, counterpoint
   above: it follows every harmonic rule. */
#define GOOD_LOWER(name)                                                                                  \
    const PitchSpelling *name[] = {P(D, 0, 3), P(F, 0, 3), P(E, 0, 3), P(D, 0, 3), P(G, 0, 3), P(F, 0, 3), \
                                   P(A, 0, 3), P(G, 0, 3), P(F, 0, 3), P(E, 0, 3), P(D, 0, 3)}
#define GOOD_UPPER(name)                                                                                  \
    const PitchSpelling *name[] = {P(A, 0, 3), P(D, 0, 4), P(C, 0, 4), P(F, 0, 3), P(B, 0, 3), P(A, 0, 3), \
                                   P(C, 0, 4), P(B, 0, 3), P(A, 0, 3), P(C, 1, 4), P(D, 0, 4)}

static void test_rules_reference_framework(void) {
    CHECK_RULES_REFERENCE_FRAMEWORK(standard_counterpoint_harmonic_rule, standard_counterpoint_harmonic_constraint,
                                    STANDARD_COUNTERPOINT_HARMONIC_RULE_COUNT);
    for (int i = 0; i < STANDARD_COUNTERPOINT_HARMONIC_RULE_COUNT; i++) {
        ConstraintType type = constraint_type(standard_counterpoint_harmonic_constraint((StandardCounterpointHarmonicRule)i));
        assert(type == (is_soft(i) ? CONSTRAINT_TYPE_SOFT : CONSTRAINT_TYPE_HARD));
    }
}

static void test_names(void) {
    assert(strcmp(standard_counterpoint_harmonic_rule_name(STANDARD_COUNTERPOINT_HARMONIC_RULE_FIRST_INTERVAL),
                  "first-interval") == 0);
    assert(strcmp(standard_counterpoint_harmonic_rule_name(STANDARD_COUNTERPOINT_HARMONIC_RULE_CLIMAX_COORDINATION),
                  "climax-coordination") == 0);
    assert(standard_counterpoint_harmonic_rule_name(STANDARD_COUNTERPOINT_HARMONIC_RULE_COUNT) == NULL);
}

static void test_good_exercise_follows_every_rule(void) {
    GOOD_LOWER(lower_notes);
    GOOD_UPPER(upper_notes);
    StandardCounterpointLine lower = LINE_OF(lower_notes);
    StandardCounterpointLine upper = LINE_OF(upper_notes);
    StandardCounterpointPair pair = {&upper, &lower};
    unsigned hard = 99, soft = 99;
    assert(standard_counterpoint_harmonic_check(&pair, &hard, &soft));
    assert(hard == 0 && soft == 0);
}

static void test_first_interval(void) {
    const PitchSpelling *lower[] = {P(C, 0, 4), P(D, 0, 4)};
    const PitchSpelling *unison[] = {P(C, 0, 4), P(D, 0, 4)};
    const PitchSpelling *fifth[] = {P(G, 0, 4), P(A, 0, 4)};
    const PitchSpelling *octave[] = {P(C, 0, 5), P(D, 0, 5)};
    const PitchSpelling *third[] = {P(E, 0, 4), P(F, 0, 4)};
    const PitchSpelling *fourth[] = {P(F, 0, 4), P(G, 0, 4)};
    assert(HOLDS(STANDARD_COUNTERPOINT_HARMONIC_RULE_FIRST_INTERVAL, unison, lower));
    assert(HOLDS(STANDARD_COUNTERPOINT_HARMONIC_RULE_FIRST_INTERVAL, fifth, lower));
    assert(HOLDS(STANDARD_COUNTERPOINT_HARMONIC_RULE_FIRST_INTERVAL, octave, lower));
    assert(!HOLDS(STANDARD_COUNTERPOINT_HARMONIC_RULE_FIRST_INTERVAL, third, lower));
    assert(!HOLDS(STANDARD_COUNTERPOINT_HARMONIC_RULE_FIRST_INTERVAL, fourth, lower));
}

static void test_last_interval(void) {
    const PitchSpelling *lower[] = {P(E, 0, 3), P(C, 0, 3)};
    const PitchSpelling *octave[] = {P(B, 0, 3), P(C, 0, 4)};
    const PitchSpelling *unison[] = {P(B, 0, 3), P(C, 0, 3)};
    const PitchSpelling *fifth[] = {P(B, 0, 3), P(G, 0, 3)};
    const PitchSpelling *third[] = {P(B, 0, 3), P(E, 0, 3)};
    assert(HOLDS(STANDARD_COUNTERPOINT_HARMONIC_RULE_LAST_INTERVAL, octave, lower));
    assert(HOLDS(STANDARD_COUNTERPOINT_HARMONIC_RULE_LAST_INTERVAL, unison, lower));
    assert(!HOLDS(STANDARD_COUNTERPOINT_HARMONIC_RULE_LAST_INTERVAL, fifth, lower));
    assert(!HOLDS(STANDARD_COUNTERPOINT_HARMONIC_RULE_LAST_INTERVAL, third, lower));
}

static void test_gap(void) {
    const PitchSpelling *lower[] = {P(C, 0, 3), P(C, 0, 3), P(C, 0, 3), P(C, 0, 3)};
    const PitchSpelling *twelfth[] = {P(G, 0, 4), P(E, 0, 3), P(E, 0, 3), P(E, 0, 3)};
    const PitchSpelling *thirteenth[] = {P(A, 0, 4), P(E, 0, 3), P(E, 0, 3), P(E, 0, 3)};
    const PitchSpelling *two_wide[] = {P(E, 0, 4), P(E, 0, 4), P(E, 0, 3), P(E, 0, 3)};
    assert(HOLDS(STANDARD_COUNTERPOINT_HARMONIC_RULE_MAX_GAP, twelfth, lower));
    assert(!HOLDS(STANDARD_COUNTERPOINT_HARMONIC_RULE_MAX_GAP, thirteenth, lower));

    /* One wide gap in four is within an octave "for the most part". */
    assert(HOLDS(STANDARD_COUNTERPOINT_HARMONIC_RULE_MOSTLY_WITHIN_OCTAVE, twelfth, lower));
    assert(!HOLDS(STANDARD_COUNTERPOINT_HARMONIC_RULE_MOSTLY_WITHIN_OCTAVE, two_wide, lower));
}

static void test_no_crossing(void) {
    const PitchSpelling *lower[] = {P(C, 0, 4), P(D, 0, 4)};
    const PitchSpelling *above[] = {P(E, 0, 4), P(F, 0, 4)};
    const PitchSpelling *unison[] = {P(C, 0, 4), P(F, 0, 4)};
    const PitchSpelling *below[] = {P(E, 0, 4), P(B, 0, 3)};
    assert(HOLDS(STANDARD_COUNTERPOINT_HARMONIC_RULE_NO_CROSSING, above, lower));
    assert(HOLDS(STANDARD_COUNTERPOINT_HARMONIC_RULE_NO_CROSSING, unison, lower));
    assert(!HOLDS(STANDARD_COUNTERPOINT_HARMONIC_RULE_NO_CROSSING, below, lower));
}

static void test_parallel_octaves(void) {
    const PitchSpelling *lower[] = {P(C, 0, 3), P(D, 0, 3)};
    const PitchSpelling *octaves[] = {P(C, 0, 4), P(D, 0, 4)};
    const PitchSpelling *fifteenths[] = {P(C, 0, 5), P(D, 0, 5)};
    const PitchSpelling *octave_then_sixth[] = {P(C, 0, 4), P(B, 0, 3)};
    const PitchSpelling *unison_lower[] = {P(C, 0, 4), P(D, 0, 4)};
    const PitchSpelling *unisons[] = {P(C, 0, 4), P(D, 0, 4)};
    assert(!HOLDS(STANDARD_COUNTERPOINT_HARMONIC_RULE_PARALLEL_OCTAVES, octaves, lower));
    assert(!HOLDS(STANDARD_COUNTERPOINT_HARMONIC_RULE_PARALLEL_OCTAVES, fifteenths, lower));
    assert(HOLDS(STANDARD_COUNTERPOINT_HARMONIC_RULE_PARALLEL_OCTAVES, octave_then_sixth, lower));
    /* Parallel unisons are parallel octaves for this purpose. */
    assert(!HOLDS(STANDARD_COUNTERPOINT_HARMONIC_RULE_PARALLEL_OCTAVES, unisons, unison_lower));
}

static void test_parallel_fifths(void) {
    const PitchSpelling *lower[] = {P(C, 0, 3), P(D, 0, 3)};
    const PitchSpelling *fifths[] = {P(G, 0, 3), P(A, 0, 3)};
    const PitchSpelling *twelfths[] = {P(G, 0, 4), P(A, 0, 4)};
    const PitchSpelling *fifth_then_fourth[] = {P(G, 0, 3), P(G, 0, 3)};
    const PitchSpelling *wide_lower[] = {P(C, 0, 3), P(E, 0, 3)};
    const PitchSpelling *wide_upper[] = {P(G, 0, 3), P(B, 0, 3)};
    const PitchSpelling *dim_lower[] = {P(B, 0, 3), P(C, 0, 4)};
    const PitchSpelling *dim_upper[] = {P(F, 0, 4), P(G, 0, 4)};
    assert(!HOLDS(STANDARD_COUNTERPOINT_HARMONIC_RULE_PARALLEL_FIFTHS, fifths, lower));
    assert(!HOLDS(STANDARD_COUNTERPOINT_HARMONIC_RULE_PARALLEL_FIFTHS, twelfths, lower));
    assert(HOLDS(STANDARD_COUNTERPOINT_HARMONIC_RULE_PARALLEL_FIFTHS, fifth_then_fourth, lower));
    assert(!HOLDS(STANDARD_COUNTERPOINT_HARMONIC_RULE_PARALLEL_FIFTHS, wide_upper, wide_lower));
    /* A diminished fifth followed by a perfect fifth is not parallel fifths. */
    assert(HOLDS(STANDARD_COUNTERPOINT_HARMONIC_RULE_PARALLEL_FIFTHS, dim_upper, dim_lower));
}

static void test_direct_perfects(void) {
    const PitchSpelling *lower[] = {P(C, 0, 3), P(E, 0, 3)};
    const PitchSpelling *direct_fifth[] = {P(E, 0, 3), P(B, 0, 3)};
    const PitchSpelling *oblique_lower[] = {P(C, 0, 3), P(C, 0, 3)};
    const PitchSpelling *oblique_upper[] = {P(E, 0, 3), P(G, 0, 3)};
    const PitchSpelling *contrary_lower[] = {P(E, 0, 3), P(D, 0, 3)};
    const PitchSpelling *contrary_upper[] = {P(B, 0, 3), P(D, 0, 4)};
    const PitchSpelling *similar_octave_lower[] = {P(C, 0, 3), P(E, 0, 3)};
    const PitchSpelling *similar_octave_upper[] = {P(E, 0, 3), P(E, 0, 4)};
    assert(!HOLDS(STANDARD_COUNTERPOINT_HARMONIC_RULE_DIRECT_PERFECTS, direct_fifth, lower));
    /* Arriving by oblique or contrary motion is fine. */
    assert(HOLDS(STANDARD_COUNTERPOINT_HARMONIC_RULE_DIRECT_PERFECTS, oblique_upper, oblique_lower));
    assert(HOLDS(STANDARD_COUNTERPOINT_HARMONIC_RULE_DIRECT_PERFECTS, contrary_upper, contrary_lower));
    assert(!HOLDS(STANDARD_COUNTERPOINT_HARMONIC_RULE_DIRECT_PERFECTS, similar_octave_upper, similar_octave_lower));
}

static void test_interior_unisons(void) {
    const PitchSpelling *lower[] = {P(C, 0, 3), P(D, 0, 3), P(E, 0, 3)};
    const PitchSpelling *interior[] = {P(C, 0, 4), P(D, 0, 3), P(G, 0, 3)};
    const PitchSpelling *ends[] = {P(C, 0, 3), P(F, 0, 3), P(E, 0, 3)};
    assert(!HOLDS(STANDARD_COUNTERPOINT_HARMONIC_RULE_INTERIOR_UNISONS, interior, lower));
    assert(HOLDS(STANDARD_COUNTERPOINT_HARMONIC_RULE_INTERIOR_UNISONS, ends, lower));
}

static void test_climax_coordination(void) {
    const PitchSpelling *together_lower[] = {P(C, 0, 3), P(E, 0, 3), P(D, 0, 3)};
    const PitchSpelling *together_upper[] = {P(C, 0, 4), P(E, 0, 4), P(D, 0, 4)};
    const PitchSpelling *apart_lower[] = {P(C, 0, 3), P(D, 0, 3), P(E, 0, 3)};
    assert(!HOLDS(STANDARD_COUNTERPOINT_HARMONIC_RULE_CLIMAX_COORDINATION, together_upper, together_lower));
    assert(HOLDS(STANDARD_COUNTERPOINT_HARMONIC_RULE_CLIMAX_COORDINATION, together_upper, apart_lower));
}

static void test_check_masks(void) {
    /* Parallel octaves, which are also a direct octave. */
    const PitchSpelling *lower_notes[] = {P(C, 0, 3), P(D, 0, 3)};
    const PitchSpelling *upper_notes[] = {P(C, 0, 4), P(D, 0, 4)};
    StandardCounterpointLine lower = LINE_OF(lower_notes);
    StandardCounterpointLine upper = LINE_OF(upper_notes);
    StandardCounterpointPair pair = {&upper, &lower};
    unsigned hard = 0, soft = 0;
    assert(standard_counterpoint_harmonic_check(&pair, &hard, &soft));
    assert(hard & (1u << STANDARD_COUNTERPOINT_HARMONIC_RULE_PARALLEL_OCTAVES));
    assert(!(hard & (1u << STANDARD_COUNTERPOINT_HARMONIC_RULE_NO_CROSSING)));
    assert(soft & (1u << STANDARD_COUNTERPOINT_HARMONIC_RULE_DIRECT_PERFECTS));
    assert(!(soft & (1u << STANDARD_COUNTERPOINT_HARMONIC_RULE_PARALLEL_OCTAVES)));
}

static void test_invalid_input(void) {
    const PitchSpelling *three[] = {P(C, 0, 3), P(D, 0, 3), P(E, 0, 3)};
    const PitchSpelling *two[] = {P(C, 0, 4), P(D, 0, 4)};
    const PitchSpelling *with_null[] = {P(C, 0, 4), NULL, P(E, 0, 4)};
    StandardCounterpointLine lower = LINE_OF(three);
    StandardCounterpointLine short_upper = LINE_OF(two);
    StandardCounterpointLine null_upper = LINE_OF(with_null);
    StandardCounterpointLine empty = {three, 0};
    StandardCounterpointPair mismatched = {&short_upper, &lower};
    StandardCounterpointPair with_null_note = {&null_upper, &lower};
    StandardCounterpointPair with_empty = {&empty, &empty};
    unsigned hard = 5, soft = 5;

    assert(!standard_counterpoint_harmonic_check(NULL, &hard, &soft));
    assert(!standard_counterpoint_harmonic_check(&mismatched, &hard, &soft));
    assert(!standard_counterpoint_harmonic_check(&with_null_note, &hard, &soft));
    assert(!standard_counterpoint_harmonic_check(&with_empty, &hard, &soft));
    assert(hard == 5 && soft == 5);
    assert(!standard_counterpoint_harmonic_rule_holds(STANDARD_COUNTERPOINT_HARMONIC_RULE_FIRST_INTERVAL, NULL));
    assert(!standard_counterpoint_harmonic_rule_holds(STANDARD_COUNTERPOINT_HARMONIC_RULE_COUNT, &mismatched));
}

int main(void) {
    test_rules_reference_framework();
    test_names();
    test_good_exercise_follows_every_rule();
    test_first_interval();
    test_last_interval();
    test_gap();
    test_no_crossing();
    test_parallel_octaves();
    test_parallel_fifths();
    test_direct_perfects();
    test_interior_unisons();
    test_climax_coordination();
    test_check_masks();
    test_invalid_input();
    printf("test_harmonic_rules (standard-counterpoint): ok\n");
    return 0;
}
