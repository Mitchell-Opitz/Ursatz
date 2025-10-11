#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "counterpoint_test_util.h"
#include "first_species_independence_rules.h"
#include "rule_check.h"
#include "eval_check.h"

/* Every call to the group's rule_holds below also runs the rule through constraint_evaluate. */
STANDARD_COUNTERPOINT_EVAL_TABLE(g_eval, "first-species-independence", STANDARD_COUNTERPOINT_FIRST_SPECIES_INDEPENDENCE_COUNT);
#define standard_counterpoint_first_species_independence_rule_holds(which, a, b) \
    STANDARD_COUNTERPOINT_EVAL_HOLDS2(g_eval, standard_counterpoint_first_species_independence_constraint, standard_counterpoint_first_species_independence_rule_holds, StandardCounterpointPairCantus, which, a, b)

#define CANTUS_LOWER STANDARD_COUNTERPOINT_CANTUS_IN_LOWER
#define CANTUS_UPPER STANDARD_COUNTERPOINT_CANTUS_IN_UPPER

static bool holds(StandardCounterpointFirstSpeciesIndependenceRule rule, const PitchSpelling *const *upper_notes,
                  const PitchSpelling *const *lower_notes, size_t count, StandardCounterpointCantusPart cantus_part) {
    StandardCounterpointLine upper = {upper_notes, count};
    StandardCounterpointLine lower = {lower_notes, count};
    StandardCounterpointPair pair = {&upper, &lower};
    return standard_counterpoint_first_species_independence_rule_holds(rule, &pair, cantus_part);
}

#define HOLDS(rule, upper, lower, part) holds(rule, upper, lower, sizeof(upper) / sizeof((upper)[0]), part)

static void test_rules_reference_framework(void) {
    CHECK_RULES_REFERENCE_FRAMEWORK(standard_counterpoint_first_species_independence_rule,
                                    standard_counterpoint_first_species_independence_constraint,
                                    STANDARD_COUNTERPOINT_FIRST_SPECIES_INDEPENDENCE_COUNT);
    for (int i = 0; i < STANDARD_COUNTERPOINT_FIRST_SPECIES_INDEPENDENCE_COUNT; i++) {
        ConstraintType type = constraint_type(standard_counterpoint_first_species_independence_constraint(
            (StandardCounterpointFirstSpeciesIndependenceRule)i));
        bool hard = i == STANDARD_COUNTERPOINT_FIRST_SPECIES_INDEPENDENCE_SINGLE_REPEAT;
        assert(type == (hard ? CONSTRAINT_TYPE_HARD : CONSTRAINT_TYPE_SOFT));
    }
}

static void test_names(void) {
    assert(strcmp(standard_counterpoint_first_species_independence_rule_name(
                      STANDARD_COUNTERPOINT_FIRST_SPECIES_INDEPENDENCE_SINGLE_REPEAT),
                  "single-repeat") == 0);
    assert(strcmp(standard_counterpoint_first_species_independence_rule_name(
                      STANDARD_COUNTERPOINT_FIRST_SPECIES_INDEPENDENCE_EMERGENCY_GAP),
                  "emergency-gap") == 0);
    assert(standard_counterpoint_first_species_independence_rule_name(
               STANDARD_COUNTERPOINT_FIRST_SPECIES_INDEPENDENCE_COUNT) == NULL);
}

static void test_single_repeat(void) {
    const PitchSpelling *lower[] = {P(C, 0, 3), P(D, 0, 3), P(E, 0, 3), P(F, 0, 3)};
    const PitchSpelling *none[] = {P(E, 0, 3), P(F, 0, 3), P(G, 0, 3), P(A, 0, 3)};
    const PitchSpelling *once[] = {P(E, 0, 3), P(E, 0, 3), P(G, 0, 3), P(A, 0, 3)};
    const PitchSpelling *twice[] = {P(E, 0, 3), P(E, 0, 3), P(G, 0, 3), P(G, 0, 3)};
    assert(HOLDS(STANDARD_COUNTERPOINT_FIRST_SPECIES_INDEPENDENCE_SINGLE_REPEAT, none, lower, CANTUS_LOWER));
    assert(HOLDS(STANDARD_COUNTERPOINT_FIRST_SPECIES_INDEPENDENCE_SINGLE_REPEAT, once, lower, CANTUS_LOWER));
    assert(!HOLDS(STANDARD_COUNTERPOINT_FIRST_SPECIES_INDEPENDENCE_SINGLE_REPEAT, twice, lower, CANTUS_LOWER));
    /* Repeats in the cantus firmus are not the counterpoint's: here the
       upper part is the counterpoint and has none. */
    assert(HOLDS(STANDARD_COUNTERPOINT_FIRST_SPECIES_INDEPENDENCE_SINGLE_REPEAT, none, twice, CANTUS_LOWER));
    /* With the cantus firmus above, the counterpoint is the lower part. */
    assert(!HOLDS(STANDARD_COUNTERPOINT_FIRST_SPECIES_INDEPENDENCE_SINGLE_REPEAT, none, twice, CANTUS_UPPER));
}

static void test_no_repeat(void) {
    const PitchSpelling *lower[] = {P(C, 0, 3), P(D, 0, 3), P(E, 0, 3)};
    const PitchSpelling *none[] = {P(E, 0, 3), P(F, 0, 3), P(G, 0, 3)};
    const PitchSpelling *once[] = {P(E, 0, 3), P(E, 0, 3), P(G, 0, 3)};
    assert(HOLDS(STANDARD_COUNTERPOINT_FIRST_SPECIES_INDEPENDENCE_NO_REPEAT, none, lower, CANTUS_LOWER));
    assert(!HOLDS(STANDARD_COUNTERPOINT_FIRST_SPECIES_INDEPENDENCE_NO_REPEAT, once, lower, CANTUS_LOWER));
}

static void test_no_overlap(void) {
    const PitchSpelling *lower[] = {P(C, 0, 3), P(D, 0, 3)};
    const PitchSpelling *clear[] = {P(E, 0, 3), P(F, 0, 3)};
    const PitchSpelling *lower_leaps_past[] = {P(C, 0, 4), P(F, 0, 4)};
    const PitchSpelling *upper_over[] = {P(E, 0, 4), P(G, 0, 4)};
    const PitchSpelling *lower_c_then_d[] = {P(C, 0, 4), P(D, 0, 4)};
    const PitchSpelling *upper_under[] = {P(E, 0, 4), P(B, 0, 3)};
    assert(HOLDS(STANDARD_COUNTERPOINT_FIRST_SPECIES_INDEPENDENCE_NO_OVERLAP, clear, lower, CANTUS_LOWER));
    /* The upper part sings E4; the lower part may not sing F4 in the next bar. */
    assert(!HOLDS(STANDARD_COUNTERPOINT_FIRST_SPECIES_INDEPENDENCE_NO_OVERLAP, upper_over, lower_leaps_past,
                  CANTUS_LOWER));
    /* The upper part's next note falls below the lower part's current note. */
    assert(!HOLDS(STANDARD_COUNTERPOINT_FIRST_SPECIES_INDEPENDENCE_NO_OVERLAP, upper_under, lower_c_then_d,
                  CANTUS_LOWER));
}

static void test_emergency_gap(void) {
    const PitchSpelling *lower[] = {P(C, 0, 3), P(C, 0, 3), P(C, 0, 3), P(C, 0, 3)};
    const PitchSpelling *tenths[] = {P(E, 0, 4), P(E, 0, 4), P(E, 0, 4), P(E, 0, 4)};
    const PitchSpelling *two_wide[] = {P(G, 0, 4), P(G, 0, 4), P(E, 0, 3), P(E, 0, 3)};
    const PitchSpelling *three_wide[] = {P(G, 0, 4), P(G, 0, 4), P(G, 0, 4), P(E, 0, 3)};
    /* A tenth is not past the tenth; a twelfth, for two notes, is allowed. */
    assert(HOLDS(STANDARD_COUNTERPOINT_FIRST_SPECIES_INDEPENDENCE_EMERGENCY_GAP, tenths, lower, CANTUS_LOWER));
    assert(HOLDS(STANDARD_COUNTERPOINT_FIRST_SPECIES_INDEPENDENCE_EMERGENCY_GAP, two_wide, lower, CANTUS_LOWER));
    assert(!HOLDS(STANDARD_COUNTERPOINT_FIRST_SPECIES_INDEPENDENCE_EMERGENCY_GAP, three_wide, lower, CANTUS_LOWER));
}

static void test_check_masks(void) {
    /* The counterpoint repeats G4 twice, and the lower part leaps past E4. */
    const PitchSpelling *lower_notes[] = {P(C, 0, 4), P(F, 0, 4), P(F, 0, 4), P(F, 0, 4)};
    const PitchSpelling *upper_notes[] = {P(E, 0, 4), P(G, 0, 4), P(G, 0, 4), P(G, 0, 4)};
    StandardCounterpointLine lower = LINE_OF(lower_notes);
    StandardCounterpointLine upper = LINE_OF(upper_notes);
    StandardCounterpointPair pair = {&upper, &lower};
    unsigned hard = 0, soft = 0;
    assert(standard_counterpoint_first_species_independence_check(&pair, CANTUS_LOWER, &hard, &soft));
    assert(hard == (1u << STANDARD_COUNTERPOINT_FIRST_SPECIES_INDEPENDENCE_SINGLE_REPEAT));
    assert(soft & (1u << STANDARD_COUNTERPOINT_FIRST_SPECIES_INDEPENDENCE_NO_REPEAT));
    assert(soft & (1u << STANDARD_COUNTERPOINT_FIRST_SPECIES_INDEPENDENCE_NO_OVERLAP));
    assert(!(soft & (1u << STANDARD_COUNTERPOINT_FIRST_SPECIES_INDEPENDENCE_EMERGENCY_GAP)));
}

static void test_invalid_input(void) {
    const PitchSpelling *three[] = {P(C, 0, 3), P(D, 0, 3), P(E, 0, 3)};
    const PitchSpelling *two[] = {P(C, 0, 4), P(D, 0, 4)};
    StandardCounterpointLine lower = LINE_OF(three);
    StandardCounterpointLine short_upper = LINE_OF(two);
    StandardCounterpointPair mismatched = {&short_upper, &lower};
    unsigned hard = 5, soft = 5;

    assert(!standard_counterpoint_first_species_independence_check(NULL, CANTUS_LOWER, &hard, &soft));
    assert(!standard_counterpoint_first_species_independence_check(&mismatched, CANTUS_LOWER, &hard, &soft));
    assert(!standard_counterpoint_first_species_independence_check(&mismatched, (StandardCounterpointCantusPart)5, &hard,
                                                                   &soft));
    assert(hard == 5 && soft == 5);
    assert(!standard_counterpoint_first_species_independence_rule_holds(
        STANDARD_COUNTERPOINT_FIRST_SPECIES_INDEPENDENCE_COUNT, &mismatched, CANTUS_LOWER));
}

int main(void) {
    test_rules_reference_framework();
    test_names();
    test_single_repeat();
    test_no_repeat();
    test_no_overlap();
    test_emergency_gap();
    test_check_masks();
    test_invalid_input();
    printf("test_first_species_independence_rules (standard-counterpoint): ok\n");
    return 0;
}
