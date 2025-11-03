#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "cantus_firmus_rules.h"
#include "counterpoint_test_util.h"
#include "rule_check.h"
#include "eval_check.h"

/* Every call to the group's rule_holds below also runs the rule through constraint_evaluate. */
STANDARD_COUNTERPOINT_EVAL_TABLE(g_eval, "cantus-firmus", STANDARD_COUNTERPOINT_CANTUS_RULE_COUNT);
#define standard_counterpoint_cantus_rule_holds(which, a, b) \
    STANDARD_COUNTERPOINT_EVAL_HOLDS2(g_eval, standard_counterpoint_cantus_constraint, standard_counterpoint_cantus_preference, standard_counterpoint_cantus_rule_holds, StandardCounterpointLineTonic, which, a, b)

static bool holds(StandardCounterpointCantusRule rule, const PitchSpelling *const *notes, size_t count,
                  const PitchSpelling *tonic) {
    StandardCounterpointLine line = {notes, count};
    return standard_counterpoint_cantus_rule_holds(rule, &line, tonic);
}

#define HOLDS(rule, array, tonic) holds(rule, array, sizeof(array) / sizeof((array)[0]), tonic)

/* Fux's Dorian cantus firmus. */
#define FUX_D(name)                                                                                      \
    const PitchSpelling *name[] = {P(D, 0, 4), P(F, 0, 4), P(E, 0, 4), P(D, 0, 4), P(G, 0, 4), P(F, 0, 4), \
                                   P(A, 0, 4), P(G, 0, 4), P(F, 0, 4), P(E, 0, 4), P(D, 0, 4)}

static void test_rules_reference_framework(void) {
    CHECK_RULES_REFERENCE_FRAMEWORK(standard_counterpoint_cantus_rule, standard_counterpoint_cantus_constraint,
                                    STANDARD_COUNTERPOINT_CANTUS_RULE_COUNT);
    for (int i = 0; i < STANDARD_COUNTERPOINT_CANTUS_RULE_COUNT; i++) {
        ConstraintType type = constraint_type(standard_counterpoint_cantus_constraint((StandardCounterpointCantusRule)i));
        bool soft = i == STANDARD_COUNTERPOINT_CANTUS_RULE_SINGLE_CLIMAX ||
                    i == STANDARD_COUNTERPOINT_CANTUS_RULE_MOSTLY_STEPWISE ||
                    i == STANDARD_COUNTERPOINT_CANTUS_RULE_NO_REPEATED_MOTIVE;
        assert(type == (soft ? CONSTRAINT_TYPE_SOFT : CONSTRAINT_TYPE_HARD));
    }
}

static void test_names(void) {
    assert(strcmp(standard_counterpoint_cantus_rule_name(STANDARD_COUNTERPOINT_CANTUS_RULE_LENGTH), "length") == 0);
    assert(strcmp(standard_counterpoint_cantus_rule_name(STANDARD_COUNTERPOINT_CANTUS_RULE_LEADING_TONE),
                  "leading-tone") == 0);
    assert(standard_counterpoint_cantus_rule_name(STANDARD_COUNTERPOINT_CANTUS_RULE_COUNT) == NULL);
}

static void test_fux_cantus_firmus_is_well_formed(void) {
    FUX_D(notes);
    StandardCounterpointLine line = LINE_OF(notes);
    unsigned hard = 99, soft = 99;
    assert(standard_counterpoint_cantus_check(&line, P(D, 0, 4), &hard, &soft));
    assert(hard == 0 && soft == 0);
}

static void test_length(void) {
    const PitchSpelling *seven[] = {P(D, 0, 4), P(F, 0, 4), P(E, 0, 4), P(G, 0, 4), P(F, 0, 4), P(E, 0, 4), P(D, 0, 4)};
    const PitchSpelling *eight[] = {P(D, 0, 4), P(F, 0, 4), P(E, 0, 4), P(G, 0, 4),
                                    P(F, 0, 4), P(E, 0, 4), P(E, 0, 4), P(D, 0, 4)};
    const PitchSpelling *seventeen[] = {P(D, 0, 4), P(E, 0, 4), P(F, 0, 4), P(E, 0, 4), P(D, 0, 4), P(E, 0, 4),
                                        P(F, 0, 4), P(E, 0, 4), P(D, 0, 4), P(E, 0, 4), P(F, 0, 4), P(E, 0, 4),
                                        P(D, 0, 4), P(E, 0, 4), P(F, 0, 4), P(E, 0, 4), P(D, 0, 4)};
    assert(!HOLDS(STANDARD_COUNTERPOINT_CANTUS_RULE_LENGTH, seven, P(D, 0, 4)));
    assert(HOLDS(STANDARD_COUNTERPOINT_CANTUS_RULE_LENGTH, eight, P(D, 0, 4)));
    assert(!HOLDS(STANDARD_COUNTERPOINT_CANTUS_RULE_LENGTH, seventeen, P(D, 0, 4)));
}

static void test_tonic_ends(void) {
    FUX_D(good);
    const PitchSpelling *starts_wrong[] = {P(E, 0, 4), P(F, 0, 4), P(E, 0, 4), P(D, 0, 4)};
    const PitchSpelling *ends_wrong[] = {P(D, 0, 4), P(F, 0, 4), P(E, 0, 4), P(F, 0, 4)};
    const PitchSpelling *octave_up[] = {P(D, 0, 4), P(E, 0, 4), P(D, 0, 5)};
    assert(HOLDS(STANDARD_COUNTERPOINT_CANTUS_RULE_TONIC_ENDS, good, P(D, 0, 4)));
    assert(!HOLDS(STANDARD_COUNTERPOINT_CANTUS_RULE_TONIC_ENDS, starts_wrong, P(D, 0, 4)));
    assert(!HOLDS(STANDARD_COUNTERPOINT_CANTUS_RULE_TONIC_ENDS, ends_wrong, P(D, 0, 4)));
    /* Any octave of the tonic will do. */
    assert(HOLDS(STANDARD_COUNTERPOINT_CANTUS_RULE_TONIC_ENDS, octave_up, P(D, 0, 3)));
}

static void test_final_step(void) {
    FUX_D(good);
    const PitchSpelling *leap_in[] = {P(D, 0, 4), P(E, 0, 4), P(G, 0, 4), P(D, 0, 4)};
    const PitchSpelling *whole_step_below[] = {P(D, 0, 4), P(E, 0, 4), P(C, 0, 4), P(D, 0, 4)};
    const PitchSpelling *half_step_below[] = {P(D, 0, 4), P(E, 0, 4), P(C, 1, 4), P(D, 0, 4)};
    const PitchSpelling *half_step_above[] = {P(E, 0, 4), P(F, 0, 4), P(E, 0, 4)};
    const PitchSpelling *one_note[] = {P(D, 0, 4)};
    assert(HOLDS(STANDARD_COUNTERPOINT_CANTUS_RULE_FINAL_STEP, good, P(D, 0, 4)));
    assert(!HOLDS(STANDARD_COUNTERPOINT_CANTUS_RULE_FINAL_STEP, leap_in, P(D, 0, 4)));
    /* From below the approach must be a half step: the raised seventh. */
    assert(!HOLDS(STANDARD_COUNTERPOINT_CANTUS_RULE_FINAL_STEP, whole_step_below, P(D, 0, 4)));
    assert(HOLDS(STANDARD_COUNTERPOINT_CANTUS_RULE_FINAL_STEP, half_step_below, P(D, 0, 4)));
    /* From above either kind of step will do (Phrygian re to do). */
    assert(HOLDS(STANDARD_COUNTERPOINT_CANTUS_RULE_FINAL_STEP, half_step_above, P(E, 0, 4)));
    assert(!HOLDS(STANDARD_COUNTERPOINT_CANTUS_RULE_FINAL_STEP, one_note, P(D, 0, 4)));
}

static void test_melodic_consonance(void) {
    FUX_D(good);
    const PitchSpelling *seventh[] = {P(D, 0, 4), P(C, 0, 5), P(D, 0, 4)};
    const PitchSpelling *tritone[] = {P(F, 0, 4), P(B, 0, 4), P(F, 0, 4)};
    assert(HOLDS(STANDARD_COUNTERPOINT_CANTUS_RULE_MELODIC_CONSONANCE, good, P(D, 0, 4)));
    assert(!HOLDS(STANDARD_COUNTERPOINT_CANTUS_RULE_MELODIC_CONSONANCE, seventh, P(D, 0, 4)));
    assert(!HOLDS(STANDARD_COUNTERPOINT_CANTUS_RULE_MELODIC_CONSONANCE, tritone, P(F, 0, 4)));
}

static void test_range(void) {
    FUX_D(good);
    const PitchSpelling *tenth[] = {P(D, 0, 4), P(F, 0, 5), P(D, 0, 4)};
    const PitchSpelling *eleventh[] = {P(D, 0, 4), P(G, 0, 5), P(D, 0, 4)};
    assert(HOLDS(STANDARD_COUNTERPOINT_CANTUS_RULE_RANGE, good, P(D, 0, 4)));
    assert(HOLDS(STANDARD_COUNTERPOINT_CANTUS_RULE_RANGE, tenth, P(D, 0, 4)));
    assert(!HOLDS(STANDARD_COUNTERPOINT_CANTUS_RULE_RANGE, eleventh, P(D, 0, 4)));
}

static void test_single_climax(void) {
    FUX_D(good);
    const PitchSpelling *twice[] = {P(D, 0, 4), P(A, 0, 4), P(G, 0, 4), P(A, 0, 4), P(D, 0, 4)};
    assert(HOLDS(STANDARD_COUNTERPOINT_CANTUS_RULE_SINGLE_CLIMAX, good, P(D, 0, 4)));
    assert(!HOLDS(STANDARD_COUNTERPOINT_CANTUS_RULE_SINGLE_CLIMAX, twice, P(D, 0, 4)));
}

static void test_leap_recovery(void) {
    FUX_D(good);
    const PitchSpelling *continues[] = {P(D, 0, 4), P(A, 0, 4), P(B, 0, 4), P(D, 0, 4)};
    const PitchSpelling *recovers_by_leap[] = {P(D, 0, 4), P(A, 0, 4), P(F, 0, 4), P(D, 0, 4)};
    const PitchSpelling *recovers[] = {P(D, 0, 4), P(A, 0, 4), P(G, 0, 4), P(D, 0, 4)};
    const PitchSpelling *leap_last[] = {P(D, 0, 4), P(A, 0, 4)};
    assert(HOLDS(STANDARD_COUNTERPOINT_CANTUS_RULE_LEAP_RECOVERY, good, P(D, 0, 4)));
    assert(!HOLDS(STANDARD_COUNTERPOINT_CANTUS_RULE_LEAP_RECOVERY, continues, P(D, 0, 4)));
    assert(!HOLDS(STANDARD_COUNTERPOINT_CANTUS_RULE_LEAP_RECOVERY, recovers_by_leap, P(D, 0, 4)));
    assert(HOLDS(STANDARD_COUNTERPOINT_CANTUS_RULE_LEAP_RECOVERY, recovers, P(D, 0, 4)));
    /* Nothing follows a final leap, so nothing is left unrecovered. */
    assert(HOLDS(STANDARD_COUNTERPOINT_CANTUS_RULE_LEAP_RECOVERY, leap_last, P(D, 0, 4)));
}

static void test_consecutive_leaps(void) {
    FUX_D(good);
    const PitchSpelling *three[] = {P(C, 0, 4), P(E, 0, 4), P(G, 0, 4), P(B, 0, 4), P(C, 0, 5)};
    const PitchSpelling *same_direction[] = {P(C, 0, 4), P(E, 0, 4), P(A, 0, 4), P(G, 0, 4)};
    const PitchSpelling *triad[] = {P(E, 0, 4), P(G, 0, 4), P(B, 0, 4), P(A, 0, 4)};
    const PitchSpelling *opposite[] = {P(D, 0, 4), P(F, 0, 4), P(D, 0, 4), P(E, 0, 4)};
    const PitchSpelling *three_thirds[] = {P(C, 0, 4), P(E, 0, 4), P(G, 0, 4), P(B, 0, 4), P(A, 0, 4)};
    assert(HOLDS(STANDARD_COUNTERPOINT_CANTUS_RULE_CONSECUTIVE_LEAPS, good, P(D, 0, 4)));
    assert(!HOLDS(STANDARD_COUNTERPOINT_CANTUS_RULE_CONSECUTIVE_LEAPS, three, P(C, 0, 4)));
    assert(!HOLDS(STANDARD_COUNTERPOINT_CANTUS_RULE_CONSECUTIVE_LEAPS, same_direction, P(C, 0, 4)));
    /* Two thirds outlining a perfect fifth are a consonant triad. */
    assert(HOLDS(STANDARD_COUNTERPOINT_CANTUS_RULE_CONSECUTIVE_LEAPS, triad, P(E, 0, 4)));
    assert(HOLDS(STANDARD_COUNTERPOINT_CANTUS_RULE_CONSECUTIVE_LEAPS, opposite, P(D, 0, 4)));
    /* Three leaps in a row are too many even when each pair is a triad. */
    assert(!HOLDS(STANDARD_COUNTERPOINT_CANTUS_RULE_CONSECUTIVE_LEAPS, three_thirds, P(C, 0, 4)));
}

static void test_leading_tone(void) {
    FUX_D(good);
    const PitchSpelling *resolves[] = {P(C, 0, 4), P(B, 0, 3), P(C, 0, 4)};
    const PitchSpelling *falls[] = {P(C, 0, 4), P(B, 0, 3), P(A, 0, 3), P(C, 0, 4)};
    const PitchSpelling *unraised[] = {P(C, 0, 4), P(B, -1, 3), P(C, 0, 4)};
    const PitchSpelling *subtonic_falls[] = {P(C, 0, 4), P(B, -1, 3), P(A, 0, 3), P(C, 0, 4)};
    const PitchSpelling *ends_on_leading[] = {P(C, 0, 4), P(D, 0, 4), P(B, 0, 3)};
    assert(HOLDS(STANDARD_COUNTERPOINT_CANTUS_RULE_LEADING_TONE, good, P(D, 0, 4)));
    assert(!HOLDS(STANDARD_COUNTERPOINT_CANTUS_RULE_LEADING_TONE, falls, P(C, 0, 4)));
    /* B3 up to C4 is the leading tone resolving; B3 down to A3 is not. */
    assert(HOLDS(STANDARD_COUNTERPOINT_CANTUS_RULE_LEADING_TONE, resolves, P(C, 0, 4)));
    assert(!HOLDS(STANDARD_COUNTERPOINT_CANTUS_RULE_LEADING_TONE, unraised, P(C, 0, 4)));
    assert(HOLDS(STANDARD_COUNTERPOINT_CANTUS_RULE_LEADING_TONE, subtonic_falls, P(C, 0, 4)));
    assert(HOLDS(STANDARD_COUNTERPOINT_CANTUS_RULE_LEADING_TONE, ends_on_leading, P(C, 0, 4)));
}

static void test_mostly_stepwise(void) {
    FUX_D(good);
    const PitchSpelling *leaps[] = {P(C, 0, 4), P(E, 0, 4), P(G, 0, 4), P(E, 0, 4), P(C, 0, 4)};
    assert(HOLDS(STANDARD_COUNTERPOINT_CANTUS_RULE_MOSTLY_STEPWISE, good, P(D, 0, 4)));
    assert(!HOLDS(STANDARD_COUNTERPOINT_CANTUS_RULE_MOSTLY_STEPWISE, leaps, P(C, 0, 4)));
}

static void test_no_repeated_motive(void) {
    FUX_D(good);
    const PitchSpelling *lick[] = {P(D, 0, 4), P(E, 0, 4), P(F, 0, 4), P(G, 0, 4), P(F, 0, 4),
                                   P(D, 0, 4), P(E, 0, 4), P(F, 0, 4), P(G, 0, 4), P(D, 0, 4)};
    assert(HOLDS(STANDARD_COUNTERPOINT_CANTUS_RULE_NO_REPEATED_MOTIVE, good, P(D, 0, 4)));
    assert(!HOLDS(STANDARD_COUNTERPOINT_CANTUS_RULE_NO_REPEATED_MOTIVE, lick, P(D, 0, 4)));
}

static void test_check_masks(void) {
    /* Starts on E, ends on a leap to F, climaxes twice, range of an eleventh. */
    const PitchSpelling *bad[] = {P(E, 0, 4), P(B, 0, 4), P(E, 0, 5), P(B, 0, 5), P(E, 0, 5), P(B, 0, 5)};
    StandardCounterpointLine line = LINE_OF(bad);
    unsigned hard = 0, soft = 0;
    assert(standard_counterpoint_cantus_check(&line, P(D, 0, 4), &hard, &soft));
    assert(hard & (1u << STANDARD_COUNTERPOINT_CANTUS_RULE_LENGTH));
    assert(hard & (1u << STANDARD_COUNTERPOINT_CANTUS_RULE_TONIC_ENDS));
    assert(hard & (1u << STANDARD_COUNTERPOINT_CANTUS_RULE_FINAL_STEP));
    assert(hard & (1u << STANDARD_COUNTERPOINT_CANTUS_RULE_RANGE));
    assert(soft & (1u << STANDARD_COUNTERPOINT_CANTUS_RULE_SINGLE_CLIMAX));
    /* A soft rule never sets a hard bit, nor a hard rule a soft bit. */
    assert(!(hard & (1u << STANDARD_COUNTERPOINT_CANTUS_RULE_SINGLE_CLIMAX)));
    assert(!(soft & (1u << STANDARD_COUNTERPOINT_CANTUS_RULE_LENGTH)));
}

static void test_invalid_input(void) {
    FUX_D(good);
    StandardCounterpointLine line = LINE_OF(good);
    StandardCounterpointLine empty = {good, 0};
    const PitchSpelling *with_null[] = {P(D, 0, 4), NULL, P(D, 0, 4)};
    StandardCounterpointLine null_note = LINE_OF(with_null);
    unsigned hard = 7, soft = 7;

    assert(!standard_counterpoint_cantus_check(NULL, P(D, 0, 4), &hard, &soft));
    assert(!standard_counterpoint_cantus_check(&empty, P(D, 0, 4), &hard, &soft));
    assert(!standard_counterpoint_cantus_check(&null_note, P(D, 0, 4), &hard, &soft));
    assert(!standard_counterpoint_cantus_check(&line, NULL, &hard, &soft));
    assert(!standard_counterpoint_cantus_check(&line, P(D, 0, 4), NULL, &soft));
    assert(!standard_counterpoint_cantus_check(&line, P(D, 0, 4), &hard, NULL));
    assert(hard == 7 && soft == 7);
    assert(!standard_counterpoint_cantus_rule_holds(STANDARD_COUNTERPOINT_CANTUS_RULE_COUNT, &line, P(D, 0, 4)));
    assert(!standard_counterpoint_cantus_rule_holds(STANDARD_COUNTERPOINT_CANTUS_RULE_LENGTH, &line, NULL));
}

int main(void) {
    test_rules_reference_framework();
    test_names();
    test_fux_cantus_firmus_is_well_formed();
    test_length();
    test_tonic_ends();
    test_final_step();
    test_melodic_consonance();
    test_range();
    test_single_climax();
    test_leap_recovery();
    test_consecutive_leaps();
    test_leading_tone();
    test_mostly_stepwise();
    test_no_repeated_motive();
    test_check_masks();
    test_invalid_input();
    printf("test_cantus_firmus_rules (standard-counterpoint): ok\n");
    return 0;
}
