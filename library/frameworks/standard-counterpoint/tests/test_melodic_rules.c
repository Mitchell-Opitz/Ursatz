#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "counterpoint_test_util.h"
#include "melodic_rules.h"
#include "rule_check.h"
#include "eval_check.h"

/* Every call to the group's rule_holds below also runs the rule through constraint_evaluate. */
STANDARD_COUNTERPOINT_EVAL_TABLE(g_eval, "melodic", STANDARD_COUNTERPOINT_MELODIC_RULE_COUNT);
#define standard_counterpoint_melodic_rule_holds(which, a, b) \
    STANDARD_COUNTERPOINT_EVAL_HOLDS2(g_eval, standard_counterpoint_melodic_constraint, standard_counterpoint_melodic_rule_holds, StandardCounterpointLineLimits, which, a, b)

static bool holds_with(StandardCounterpointMelodicRule rule, const PitchSpelling *const *notes, size_t count,
                       const StandardCounterpointLimits *limits) {
    StandardCounterpointLine line = {notes, count};
    return standard_counterpoint_melodic_rule_holds(rule, &line, limits);
}

static bool holds(StandardCounterpointMelodicRule rule, const PitchSpelling *const *notes, size_t count) {
    StandardCounterpointLimits limits = standard_counterpoint_default_limits();
    return holds_with(rule, notes, count, &limits);
}

#define HOLDS(rule, array) holds(rule, array, sizeof(array) / sizeof((array)[0]))
#define HOLDS_WITH(rule, array, limits) holds_with(rule, array, sizeof(array) / sizeof((array)[0]), limits)

static bool is_hard(int rule) {
    return rule == STANDARD_COUNTERPOINT_MELODIC_RULE_CLAUSULA_VERA ||
           rule == STANDARD_COUNTERPOINT_MELODIC_RULE_FORBIDDEN_LEAP ||
           rule == STANDARD_COUNTERPOINT_MELODIC_RULE_FORBIDDEN_OUTLINE;
}

static void test_rules_reference_framework(void) {
    CHECK_RULES_REFERENCE_FRAMEWORK(standard_counterpoint_melodic_rule, standard_counterpoint_melodic_constraint,
                                    STANDARD_COUNTERPOINT_MELODIC_RULE_COUNT);
    for (int i = 0; i < STANDARD_COUNTERPOINT_MELODIC_RULE_COUNT; i++) {
        ConstraintType type = constraint_type(standard_counterpoint_melodic_constraint((StandardCounterpointMelodicRule)i));
        assert(type == (is_hard(i) ? CONSTRAINT_TYPE_HARD : CONSTRAINT_TYPE_SOFT));
    }
}

static void test_names(void) {
    assert(strcmp(standard_counterpoint_melodic_rule_name(STANDARD_COUNTERPOINT_MELODIC_RULE_CLAUSULA_VERA),
                  "clausula-vera") == 0);
    assert(strcmp(standard_counterpoint_melodic_rule_name(STANDARD_COUNTERPOINT_MELODIC_RULE_FORBIDDEN_OUTLINE),
                  "forbidden-outline") == 0);
    assert(standard_counterpoint_melodic_rule_name(STANDARD_COUNTERPOINT_MELODIC_RULE_COUNT) == NULL);
}

static void test_clausula_vera(void) {
    const PitchSpelling *by_step[] = {P(E, 0, 4), P(D, 0, 4), P(C, 0, 4)};
    const PitchSpelling *by_leap[] = {P(E, 0, 4), P(G, 0, 4), P(C, 0, 4)};
    const PitchSpelling *one[] = {P(C, 0, 4)};
    assert(HOLDS(STANDARD_COUNTERPOINT_MELODIC_RULE_CLAUSULA_VERA, by_step));
    assert(!HOLDS(STANDARD_COUNTERPOINT_MELODIC_RULE_CLAUSULA_VERA, by_leap));
    assert(!HOLDS(STANDARD_COUNTERPOINT_MELODIC_RULE_CLAUSULA_VERA, one));
}

static void test_repeated_notes(void) {
    const PitchSpelling *repeated[] = {P(C, 0, 4), P(C, 0, 4), P(D, 0, 4)};
    const PitchSpelling *moving[] = {P(C, 0, 4), P(D, 0, 4), P(E, 0, 4)};
    StandardCounterpointLimits two = standard_counterpoint_default_limits();
    two.max_consecutive_repeats = 2;
    assert(!HOLDS(STANDARD_COUNTERPOINT_MELODIC_RULE_REPEATED_NOTES, repeated));
    assert(HOLDS_WITH(STANDARD_COUNTERPOINT_MELODIC_RULE_REPEATED_NOTES, repeated, &two));
    assert(HOLDS(STANDARD_COUNTERPOINT_MELODIC_RULE_REPEATED_NOTES, moving));
}

static void test_pitch_occurrences(void) {
    const PitchSpelling *four[] = {P(C, 0, 4), P(D, 0, 4), P(C, 0, 4), P(E, 0, 4),
                                   P(C, 0, 4), P(F, 0, 4), P(C, 0, 4)};
    const PitchSpelling *three[] = {P(C, 0, 4), P(D, 0, 4), P(C, 0, 4), P(E, 0, 4), P(C, 0, 4)};
    const PitchSpelling *octaves[] = {P(C, 0, 4), P(C, 0, 5), P(C, 0, 4), P(C, 0, 5), P(C, 0, 4), P(C, 0, 5)};
    assert(!HOLDS(STANDARD_COUNTERPOINT_MELODIC_RULE_PITCH_OCCURRENCES, four));
    assert(HOLDS(STANDARD_COUNTERPOINT_MELODIC_RULE_PITCH_OCCURRENCES, three));
    /* The same letter in another octave is another pitch. */
    assert(HOLDS(STANDARD_COUNTERPOINT_MELODIC_RULE_PITCH_OCCURRENCES, octaves));
}

static void test_consecutive_leaps(void) {
    const PitchSpelling *four[] = {P(C, 0, 4), P(E, 0, 4), P(G, 0, 4), P(B, 0, 4), P(D, 0, 5)};
    const PitchSpelling *two[] = {P(C, 0, 4), P(E, 0, 4), P(G, 0, 4), P(A, 0, 4)};
    assert(!HOLDS(STANDARD_COUNTERPOINT_MELODIC_RULE_CONSECUTIVE_LEAPS, four));
    assert(HOLDS(STANDARD_COUNTERPOINT_MELODIC_RULE_CONSECUTIVE_LEAPS, two));
}

static void test_leap_density(void) {
    const PitchSpelling *dense[] = {P(C, 0, 4), P(E, 0, 4), P(G, 0, 4), P(E, 0, 4), P(G, 0, 4), P(A, 0, 4)};
    const PitchSpelling *sparse[] = {P(C, 0, 4), P(D, 0, 4), P(E, 0, 4), P(G, 0, 4), P(F, 0, 4), P(E, 0, 4)};
    const PitchSpelling *two_notes[] = {P(C, 0, 4), P(G, 0, 4)};
    StandardCounterpointLimits four = standard_counterpoint_default_limits();
    four.max_leaps_in_window = 4;
    assert(!HOLDS(STANDARD_COUNTERPOINT_MELODIC_RULE_LEAP_DENSITY, dense));
    assert(HOLDS_WITH(STANDARD_COUNTERPOINT_MELODIC_RULE_LEAP_DENSITY, dense, &four));
    assert(HOLDS(STANDARD_COUNTERPOINT_MELODIC_RULE_LEAP_DENSITY, sparse));
    assert(HOLDS(STANDARD_COUNTERPOINT_MELODIC_RULE_LEAP_DENSITY, two_notes));
}

static void test_repeated_generic(void) {
    const PitchSpelling *run[] = {P(C, 0, 4), P(D, 0, 4), P(E, 0, 4), P(F, 0, 4), P(G, 0, 4)};
    const PitchSpelling *three[] = {P(C, 0, 4), P(D, 0, 4), P(E, 0, 4), P(F, 0, 4)};
    /* Direction is ignored: four seconds in a row, up and down. */
    const PitchSpelling *wobble[] = {P(C, 0, 4), P(D, 0, 4), P(C, 0, 4), P(D, 0, 4), P(C, 0, 4)};
    const PitchSpelling *mixed[] = {P(C, 0, 4), P(D, 0, 4), P(F, 0, 4), P(G, 0, 4), P(A, 0, 4)};
    assert(!HOLDS(STANDARD_COUNTERPOINT_MELODIC_RULE_REPEATED_GENERIC, run));
    assert(HOLDS(STANDARD_COUNTERPOINT_MELODIC_RULE_REPEATED_GENERIC, three));
    assert(!HOLDS(STANDARD_COUNTERPOINT_MELODIC_RULE_REPEATED_GENERIC, wobble));
    assert(HOLDS(STANDARD_COUNTERPOINT_MELODIC_RULE_REPEATED_GENERIC, mixed));
}

static void test_climax(void) {
    const PitchSpelling *unique[] = {P(C, 0, 4), P(E, 0, 4), P(D, 0, 4), P(C, 0, 4)};
    const PitchSpelling *twice[] = {P(C, 0, 4), P(E, 0, 4), P(D, 0, 4), P(E, 0, 4), P(C, 0, 4)};
    const PitchSpelling *ends_high[] = {P(C, 0, 4), P(D, 0, 4), P(E, 0, 4)};
    const PitchSpelling *by_leap[] = {P(C, 0, 4), P(D, 0, 4), P(A, 0, 4), P(G, 0, 4)};
    const PitchSpelling *by_step[] = {P(C, 0, 4), P(D, 0, 4), P(E, 0, 4), P(D, 0, 4)};
    const PitchSpelling *starts_high[] = {P(E, 0, 4), P(D, 0, 4), P(C, 0, 4)};

    assert(HOLDS(STANDARD_COUNTERPOINT_MELODIC_RULE_UNIQUE_CLIMAX, unique));
    assert(!HOLDS(STANDARD_COUNTERPOINT_MELODIC_RULE_UNIQUE_CLIMAX, twice));

    assert(HOLDS(STANDARD_COUNTERPOINT_MELODIC_RULE_CLIMAX_NOT_FINAL, unique));
    assert(!HOLDS(STANDARD_COUNTERPOINT_MELODIC_RULE_CLIMAX_NOT_FINAL, ends_high));

    assert(HOLDS(STANDARD_COUNTERPOINT_MELODIC_RULE_CLIMAX_BY_LEAP, by_leap));
    assert(!HOLDS(STANDARD_COUNTERPOINT_MELODIC_RULE_CLIMAX_BY_LEAP, by_step));
    /* A climax on the first note is not reached by anything. */
    assert(!HOLDS(STANDARD_COUNTERPOINT_MELODIC_RULE_CLIMAX_BY_LEAP, starts_high));
}

static bool leap_ok(const PitchSpelling *from, const PitchSpelling *to) {
    const PitchSpelling *notes[] = {from, to};
    return HOLDS(STANDARD_COUNTERPOINT_MELODIC_RULE_FORBIDDEN_LEAP, notes);
}

static void test_forbidden_leap(void) {
    /* Allowed: steps, thirds, fourths, fifths, octaves, and the ascending
       minor sixth. */
    assert(leap_ok(P(C, 0, 4), P(D, 0, 4)));
    assert(leap_ok(P(C, 0, 4), P(E, 0, 4)));
    assert(leap_ok(P(C, 0, 4), P(F, 0, 4)));
    assert(leap_ok(P(C, 0, 4), P(G, 0, 4)));
    assert(leap_ok(P(G, 0, 4), P(C, 0, 4)));
    assert(leap_ok(P(C, 0, 4), P(C, 0, 5)));
    assert(leap_ok(P(C, 0, 4), P(A, -1, 4)));

    /* Forbidden: the descending minor sixth and the major sixth. */
    assert(!leap_ok(P(A, -1, 4), P(C, 0, 4)));
    assert(!leap_ok(P(C, 0, 4), P(A, 0, 4)));
    assert(!leap_ok(P(A, 0, 4), P(C, 0, 4)));
    /* Any seventh. */
    assert(!leap_ok(P(C, 0, 4), P(B, -1, 4)));
    assert(!leap_ok(P(C, 0, 4), P(B, 0, 4)));
    /* Augmented and diminished intervals, steps included. */
    assert(!leap_ok(P(C, 0, 4), P(F, 1, 4)));
    assert(!leap_ok(P(C, 0, 4), P(G, -1, 4)));
    assert(!leap_ok(P(C, 0, 4), P(D, 1, 4)));
    /* Larger than an octave. */
    assert(!leap_ok(P(C, 0, 4), P(E, 0, 5)));
}

static void test_forbidden_outline(void) {
    const PitchSpelling *fifth[] = {P(C, 0, 4), P(D, 0, 4), P(E, 0, 4), P(F, 0, 4), P(G, 0, 4)};
    const PitchSpelling *tritone[] = {P(F, 0, 4), P(G, 0, 4), P(A, 0, 4), P(B, 0, 4)};
    const PitchSpelling *seventh[] = {P(C, 0, 4), P(D, 0, 4), P(E, 0, 4), P(F, 0, 4), P(G, 0, 4), P(A, 0, 4), P(B, 0, 4)};
    const PitchSpelling *descending[] = {P(B, 0, 4), P(A, 0, 4), P(G, 0, 4), P(F, 0, 4)};
    const PitchSpelling *diminished[] = {P(B, 0, 3), P(C, 0, 4), P(D, 0, 4), P(E, 0, 4), P(F, 0, 4)};
    const PitchSpelling *turns_back[] = {P(G, 0, 4), P(A, 0, 4), P(B, 0, 4), P(C, 0, 5), P(B, 0, 4)};
    assert(HOLDS(STANDARD_COUNTERPOINT_MELODIC_RULE_FORBIDDEN_OUTLINE, fifth));
    assert(!HOLDS(STANDARD_COUNTERPOINT_MELODIC_RULE_FORBIDDEN_OUTLINE, tritone));
    assert(!HOLDS(STANDARD_COUNTERPOINT_MELODIC_RULE_FORBIDDEN_OUTLINE, seventh));
    assert(!HOLDS(STANDARD_COUNTERPOINT_MELODIC_RULE_FORBIDDEN_OUTLINE, descending));
    assert(!HOLDS(STANDARD_COUNTERPOINT_MELODIC_RULE_FORBIDDEN_OUTLINE, diminished));
    assert(HOLDS(STANDARD_COUNTERPOINT_MELODIC_RULE_FORBIDDEN_OUTLINE, turns_back));
}

static void test_check_masks(void) {
    /* A repeated C, a leap of a seventh, and a line that ends on its climax. */
    const PitchSpelling *notes[] = {P(C, 0, 4), P(C, 0, 4), P(B, 0, 4), P(C, 0, 5)};
    StandardCounterpointLine line = LINE_OF(notes);
    StandardCounterpointLimits limits = standard_counterpoint_default_limits();
    unsigned hard = 0, soft = 0;
    assert(standard_counterpoint_melodic_check(&line, &limits, &hard, &soft));
    assert(hard & (1u << STANDARD_COUNTERPOINT_MELODIC_RULE_FORBIDDEN_LEAP));
    assert(!(hard & (1u << STANDARD_COUNTERPOINT_MELODIC_RULE_CLAUSULA_VERA)));
    assert(soft & (1u << STANDARD_COUNTERPOINT_MELODIC_RULE_REPEATED_NOTES));
    assert(soft & (1u << STANDARD_COUNTERPOINT_MELODIC_RULE_CLIMAX_NOT_FINAL));
    assert(!(soft & (1u << STANDARD_COUNTERPOINT_MELODIC_RULE_FORBIDDEN_LEAP)));
}

static void test_clean_line_passes(void) {
    const PitchSpelling *notes[] = {P(C, 0, 4), P(E, 0, 4), P(D, 0, 4), P(G, 0, 4), P(E, 0, 4),
                                    P(F, 0, 4), P(D, 0, 4), P(E, 0, 4), P(D, 0, 4), P(C, 0, 4)};
    StandardCounterpointLine line = LINE_OF(notes);
    StandardCounterpointLimits limits = standard_counterpoint_default_limits();
    unsigned hard = 99, soft = 99;
    assert(standard_counterpoint_melodic_check(&line, &limits, &hard, &soft));
    assert(hard == 0 && soft == 0);
}

static void test_invalid_input(void) {
    const PitchSpelling *notes[] = {P(C, 0, 4), P(D, 0, 4)};
    const PitchSpelling *with_null[] = {P(C, 0, 4), NULL};
    StandardCounterpointLine line = LINE_OF(notes);
    StandardCounterpointLine empty = {notes, 0};
    StandardCounterpointLine null_note = LINE_OF(with_null);
    StandardCounterpointLimits limits = standard_counterpoint_default_limits();
    unsigned hard = 5, soft = 5;

    assert(!standard_counterpoint_melodic_check(NULL, &limits, &hard, &soft));
    assert(!standard_counterpoint_melodic_check(&empty, &limits, &hard, &soft));
    assert(!standard_counterpoint_melodic_check(&null_note, &limits, &hard, &soft));
    assert(!standard_counterpoint_melodic_check(&line, NULL, &hard, &soft));
    assert(!standard_counterpoint_melodic_check(&line, &limits, NULL, &soft));
    assert(!standard_counterpoint_melodic_check(&line, &limits, &hard, NULL));
    assert(hard == 5 && soft == 5);
    assert(!standard_counterpoint_melodic_rule_holds(STANDARD_COUNTERPOINT_MELODIC_RULE_COUNT, &line, &limits));
}

int main(void) {
    test_rules_reference_framework();
    test_names();
    test_clausula_vera();
    test_repeated_notes();
    test_pitch_occurrences();
    test_consecutive_leaps();
    test_leap_density();
    test_repeated_generic();
    test_climax();
    test_forbidden_leap();
    test_forbidden_outline();
    test_check_masks();
    test_clean_line_passes();
    test_invalid_input();
    printf("test_melodic_rules (standard-counterpoint): ok\n");
    return 0;
}
