#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "counterpoint_test_util.h"
#include "first_species_ends_rules.h"
#include "first_species_test_util.h"
#include "rule_check.h"

#define CANTUS_LOWER STANDARD_COUNTERPOINT_CANTUS_IN_LOWER
#define CANTUS_UPPER STANDARD_COUNTERPOINT_CANTUS_IN_UPPER

static bool holds(StandardCounterpointFirstSpeciesEndsRule rule, const PitchSpelling *const *upper_notes,
                  const PitchSpelling *const *lower_notes, size_t count, StandardCounterpointCantusPart cantus_part) {
    StandardCounterpointLine upper = {upper_notes, count};
    StandardCounterpointLine lower = {lower_notes, count};
    StandardCounterpointPair pair = {&upper, &lower};
    return standard_counterpoint_first_species_ends_rule_holds(rule, &pair, cantus_part, P(D, 0, 4));
}

#define HOLDS(rule, upper, lower, part) holds(rule, upper, lower, sizeof(upper) / sizeof((upper)[0]), part)

static void test_rules_reference_framework(void) {
    CHECK_RULES_REFERENCE_FRAMEWORK(standard_counterpoint_first_species_ends_rule,
                                    standard_counterpoint_first_species_ends_constraint,
                                    STANDARD_COUNTERPOINT_FIRST_SPECIES_ENDS_COUNT);
    for (int i = 0; i < STANDARD_COUNTERPOINT_FIRST_SPECIES_ENDS_COUNT; i++) {
        assert(constraint_type(standard_counterpoint_first_species_ends_constraint(
                   (StandardCounterpointFirstSpeciesEndsRule)i)) == CONSTRAINT_TYPE_HARD);
    }
}

static void test_names(void) {
    assert(strcmp(standard_counterpoint_first_species_ends_rule_name(STANDARD_COUNTERPOINT_FIRST_SPECIES_ENDS_BEGIN),
                  "begin") == 0);
    assert(strcmp(standard_counterpoint_first_species_ends_rule_name(
                      STANDARD_COUNTERPOINT_FIRST_SPECIES_ENDS_PENULTIMATE_THIRD_OR_SIXTH),
                  "penultimate-third-or-sixth") == 0);
    assert(standard_counterpoint_first_species_ends_rule_name(STANDARD_COUNTERPOINT_FIRST_SPECIES_ENDS_COUNT) == NULL);
}

static void test_good_exercise(void) {
    FIRST_SPECIES_LOWER(lower_notes);
    FIRST_SPECIES_UPPER(upper_notes);
    StandardCounterpointLine lower = LINE_OF(lower_notes);
    StandardCounterpointLine upper = LINE_OF(upper_notes);
    StandardCounterpointPair pair = {&upper, &lower};
    unsigned violations = 99;
    assert(standard_counterpoint_first_species_ends_check(&pair, CANTUS_LOWER, P(D, 0, 3), &violations));
    assert(violations == 0);
}

static void test_begin_above(void) {
    const PitchSpelling *lower[] = {P(D, 0, 3), P(E, 0, 3)};
    const PitchSpelling *on_do[] = {P(D, 0, 4), P(E, 0, 4)};
    const PitchSpelling *on_sol[] = {P(A, 0, 3), P(B, 0, 3)};
    const PitchSpelling *on_mi[] = {P(F, 0, 4), P(G, 0, 4)};
    const PitchSpelling *on_fa[] = {P(G, 0, 3), P(A, 0, 3)};
    const PitchSpelling *on_do_sharp[] = {P(D, 1, 4), P(E, 0, 4)};
    assert(HOLDS(STANDARD_COUNTERPOINT_FIRST_SPECIES_ENDS_BEGIN, on_do, lower, CANTUS_LOWER));
    assert(HOLDS(STANDARD_COUNTERPOINT_FIRST_SPECIES_ENDS_BEGIN, on_sol, lower, CANTUS_LOWER));
    assert(!HOLDS(STANDARD_COUNTERPOINT_FIRST_SPECIES_ENDS_BEGIN, on_mi, lower, CANTUS_LOWER));
    assert(!HOLDS(STANDARD_COUNTERPOINT_FIRST_SPECIES_ENDS_BEGIN, on_fa, lower, CANTUS_LOWER));
    assert(!HOLDS(STANDARD_COUNTERPOINT_FIRST_SPECIES_ENDS_BEGIN, on_do_sharp, lower, CANTUS_LOWER));
}

static void test_begin_below(void) {
    /* The cantus firmus is the upper part, the counterpoint the lower. */
    const PitchSpelling *upper[] = {P(D, 0, 4), P(E, 0, 4)};
    const PitchSpelling *on_do[] = {P(D, 0, 3), P(E, 0, 3)};
    const PitchSpelling *on_sol[] = {P(A, 0, 3), P(B, 0, 3)};
    const PitchSpelling *on_sol_below[] = {P(A, 0, 2), P(B, 0, 2)};
    const PitchSpelling *on_fa[] = {P(G, 0, 2), P(A, 0, 2)};
    assert(HOLDS(STANDARD_COUNTERPOINT_FIRST_SPECIES_ENDS_BEGIN, upper, on_do, CANTUS_UPPER));
    /* Sol below the cantus firmus is a dissonant fourth. */
    assert(!HOLDS(STANDARD_COUNTERPOINT_FIRST_SPECIES_ENDS_BEGIN, upper, on_sol, CANTUS_UPPER));
    assert(!HOLDS(STANDARD_COUNTERPOINT_FIRST_SPECIES_ENDS_BEGIN, upper, on_sol_below, CANTUS_UPPER));
    assert(!HOLDS(STANDARD_COUNTERPOINT_FIRST_SPECIES_ENDS_BEGIN, upper, on_fa, CANTUS_UPPER));
}

static void test_end_on_tonic(void) {
    const PitchSpelling *lower[] = {P(E, 0, 3), P(D, 0, 3)};
    const PitchSpelling *ends_on_do[] = {P(C, 1, 4), P(D, 0, 4)};
    const PitchSpelling *ends_on_re[] = {P(C, 1, 4), P(E, 0, 4)};
    const PitchSpelling *octave_down[] = {P(C, 1, 4), P(D, 0, 3)};
    assert(HOLDS(STANDARD_COUNTERPOINT_FIRST_SPECIES_ENDS_END_ON_TONIC, ends_on_do, lower, CANTUS_LOWER));
    assert(!HOLDS(STANDARD_COUNTERPOINT_FIRST_SPECIES_ENDS_END_ON_TONIC, ends_on_re, lower, CANTUS_LOWER));
    assert(HOLDS(STANDARD_COUNTERPOINT_FIRST_SPECIES_ENDS_END_ON_TONIC, octave_down, lower, CANTUS_LOWER));
}

static void test_contrary_stepwise_approach(void) {
    const PitchSpelling *lower[] = {P(E, 0, 3), P(D, 0, 3)};
    const PitchSpelling *clausula[] = {P(C, 1, 4), P(D, 0, 4)};
    const PitchSpelling *similar[] = {P(F, 0, 4), P(E, 0, 4)};
    const PitchSpelling *leap[] = {P(A, 0, 3), P(D, 0, 4)};
    const PitchSpelling *oblique_lower[] = {P(D, 0, 3), P(D, 0, 3)};
    const PitchSpelling *oblique[] = {P(C, 1, 4), P(D, 0, 4)};
    const PitchSpelling *one_note_lower[] = {P(D, 0, 3)};
    const PitchSpelling *one_note[] = {P(D, 0, 4)};
    assert(HOLDS(STANDARD_COUNTERPOINT_FIRST_SPECIES_ENDS_CONTRARY_STEPWISE_APPROACH, clausula, lower, CANTUS_LOWER));
    assert(!HOLDS(STANDARD_COUNTERPOINT_FIRST_SPECIES_ENDS_CONTRARY_STEPWISE_APPROACH, similar, lower, CANTUS_LOWER));
    assert(!HOLDS(STANDARD_COUNTERPOINT_FIRST_SPECIES_ENDS_CONTRARY_STEPWISE_APPROACH, leap, lower, CANTUS_LOWER));
    assert(!HOLDS(STANDARD_COUNTERPOINT_FIRST_SPECIES_ENDS_CONTRARY_STEPWISE_APPROACH, oblique, oblique_lower,
                  CANTUS_LOWER));
    /* A single position has no approach. */
    assert(!HOLDS(STANDARD_COUNTERPOINT_FIRST_SPECIES_ENDS_CONTRARY_STEPWISE_APPROACH, one_note, one_note_lower,
                  CANTUS_LOWER));
}

static void test_penultimate_third_or_sixth(void) {
    const PitchSpelling *sixth_lower[] = {P(E, 0, 3), P(D, 0, 3)};
    const PitchSpelling *sixth[] = {P(C, 1, 4), P(D, 0, 4)};
    const PitchSpelling *third_lower[] = {P(F, 0, 3), P(D, 0, 3)};
    const PitchSpelling *third[] = {P(A, 0, 3), P(D, 0, 4)};
    const PitchSpelling *fifth_lower[] = {P(G, 0, 3), P(D, 0, 3)};
    const PitchSpelling *fifth[] = {P(D, 0, 4), P(D, 0, 4)};
    const PitchSpelling *fourth[] = {P(C, 0, 4), P(D, 0, 4)};
    assert(HOLDS(STANDARD_COUNTERPOINT_FIRST_SPECIES_ENDS_PENULTIMATE_THIRD_OR_SIXTH, sixth, sixth_lower,
                 CANTUS_LOWER));
    assert(HOLDS(STANDARD_COUNTERPOINT_FIRST_SPECIES_ENDS_PENULTIMATE_THIRD_OR_SIXTH, third, third_lower,
                 CANTUS_LOWER));
    assert(!HOLDS(STANDARD_COUNTERPOINT_FIRST_SPECIES_ENDS_PENULTIMATE_THIRD_OR_SIXTH, fifth, fifth_lower,
                  CANTUS_LOWER));
    assert(!HOLDS(STANDARD_COUNTERPOINT_FIRST_SPECIES_ENDS_PENULTIMATE_THIRD_OR_SIXTH, fourth, fifth_lower,
                  CANTUS_LOWER));
}

static void test_check_masks(void) {
    /* Begins on sol, but ends on re with a leap and a fourth before it. */
    const PitchSpelling *lower_notes[] = {P(D, 0, 3), P(E, 0, 3), P(D, 0, 3)};
    const PitchSpelling *upper_notes[] = {P(A, 0, 3), P(A, 0, 3), P(E, 0, 4)};
    StandardCounterpointLine lower = LINE_OF(lower_notes);
    StandardCounterpointLine upper = LINE_OF(upper_notes);
    StandardCounterpointPair pair = {&upper, &lower};
    unsigned violations = 0;
    assert(standard_counterpoint_first_species_ends_check(&pair, CANTUS_LOWER, P(D, 0, 4), &violations));
    assert(!(violations & (1u << STANDARD_COUNTERPOINT_FIRST_SPECIES_ENDS_BEGIN)));
    assert(violations & (1u << STANDARD_COUNTERPOINT_FIRST_SPECIES_ENDS_END_ON_TONIC));
    assert(violations & (1u << STANDARD_COUNTERPOINT_FIRST_SPECIES_ENDS_CONTRARY_STEPWISE_APPROACH));
    assert(violations & (1u << STANDARD_COUNTERPOINT_FIRST_SPECIES_ENDS_PENULTIMATE_THIRD_OR_SIXTH));
}

static void test_invalid_input(void) {
    const PitchSpelling *lower_notes[] = {P(E, 0, 3), P(D, 0, 3)};
    const PitchSpelling *upper_notes[] = {P(C, 1, 4), P(D, 0, 4)};
    const PitchSpelling *short_notes[] = {P(C, 1, 4)};
    StandardCounterpointLine lower = LINE_OF(lower_notes);
    StandardCounterpointLine upper = LINE_OF(upper_notes);
    StandardCounterpointLine short_upper = LINE_OF(short_notes);
    StandardCounterpointPair pair = {&upper, &lower};
    StandardCounterpointPair mismatched = {&short_upper, &lower};
    unsigned violations = 5;

    assert(!standard_counterpoint_first_species_ends_check(NULL, CANTUS_LOWER, P(D, 0, 4), &violations));
    assert(!standard_counterpoint_first_species_ends_check(&mismatched, CANTUS_LOWER, P(D, 0, 4), &violations));
    assert(!standard_counterpoint_first_species_ends_check(&pair, CANTUS_LOWER, NULL, &violations));
    assert(!standard_counterpoint_first_species_ends_check(&pair, (StandardCounterpointCantusPart)5, P(D, 0, 4),
                                                           &violations));
    assert(!standard_counterpoint_first_species_ends_check(&pair, CANTUS_LOWER, P(D, 0, 4), NULL));
    assert(violations == 5);
    assert(!standard_counterpoint_first_species_ends_rule_holds(STANDARD_COUNTERPOINT_FIRST_SPECIES_ENDS_COUNT, &pair,
                                                                CANTUS_LOWER, P(D, 0, 4)));
}

int main(void) {
    test_rules_reference_framework();
    test_names();
    test_good_exercise();
    test_begin_above();
    test_begin_below();
    test_end_on_tonic();
    test_contrary_stepwise_approach();
    test_penultimate_third_or_sixth();
    test_check_masks();
    test_invalid_input();
    printf("test_first_species_ends_rules (standard-counterpoint): ok\n");
    return 0;
}
