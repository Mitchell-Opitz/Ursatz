#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "counterpoint_test_util.h"
#include "rule_check.h"
#include "tendency_rules.h"

static bool holds(StandardCounterpointTendency which, const PitchSpelling *const *notes, size_t count) {
    StandardCounterpointLine line = {notes, count};
    return standard_counterpoint_tendency_holds(which, &line);
}

#define HOLDS(which, array) holds(which, array, sizeof(array) / sizeof((array)[0]))

static void test_rules_reference_framework(void) {
    CHECK_RULES_REFERENCE_FRAMEWORK(standard_counterpoint_tendency_rule, standard_counterpoint_tendency_constraint,
                                    STANDARD_COUNTERPOINT_TENDENCY_COUNT);
    for (int i = 0; i < STANDARD_COUNTERPOINT_TENDENCY_COUNT; i++) {
        assert(constraint_type(standard_counterpoint_tendency_constraint((StandardCounterpointTendency)i)) ==
               CONSTRAINT_TYPE_SOFT);
    }
}

static void test_names(void) {
    assert(strcmp(standard_counterpoint_tendency_name(STANDARD_COUNTERPOINT_TENDENCY_PITCH_PROXIMITY),
                  "pitch-proximity") == 0);
    assert(strcmp(standard_counterpoint_tendency_name(STANDARD_COUNTERPOINT_TENDENCY_MELODIC_ARCH), "melodic-arch") == 0);
    assert(standard_counterpoint_tendency_name(STANDARD_COUNTERPOINT_TENDENCY_COUNT) == NULL);
}

static void test_pitch_proximity(void) {
    const PitchSpelling *steps[] = {P(C, 0, 4), P(D, 0, 4), P(E, 0, 4), P(D, 0, 4)};
    const PitchSpelling *leaps[] = {P(C, 0, 4), P(G, 0, 4), P(C, 0, 4)};
    const PitchSpelling *mixed[] = {P(C, 0, 4), P(D, 0, 4), P(G, 0, 4), P(F, 0, 4)};
    const PitchSpelling *large_leap[] = {P(C, 0, 4), P(D, 0, 4), P(E, 0, 4), P(F, 0, 4), P(C, 0, 5)};
    assert(HOLDS(STANDARD_COUNTERPOINT_TENDENCY_PITCH_PROXIMITY, steps));
    assert(!HOLDS(STANDARD_COUNTERPOINT_TENDENCY_PITCH_PROXIMITY, leaps));
    assert(HOLDS(STANDARD_COUNTERPOINT_TENDENCY_PITCH_PROXIMITY, mixed));
    /* Steps outnumber leaps, but the only leap is a large one. */
    assert(!HOLDS(STANDARD_COUNTERPOINT_TENDENCY_PITCH_PROXIMITY, large_leap));
}

static void test_step_declination(void) {
    const PitchSpelling *descending[] = {P(E, 0, 4), P(D, 0, 4), P(C, 0, 4)};
    const PitchSpelling *ascending[] = {P(C, 0, 4), P(D, 0, 4), P(E, 0, 4)};
    const PitchSpelling *leap_then_step[] = {P(C, 0, 4), P(E, 0, 4), P(D, 0, 4)};
    assert(HOLDS(STANDARD_COUNTERPOINT_TENDENCY_STEP_DECLINATION, descending));
    assert(!HOLDS(STANDARD_COUNTERPOINT_TENDENCY_STEP_DECLINATION, ascending));
    assert(HOLDS(STANDARD_COUNTERPOINT_TENDENCY_STEP_DECLINATION, leap_then_step));
}

static void test_step_inertia(void) {
    const PitchSpelling *continues[] = {P(C, 0, 4), P(D, 0, 4), P(E, 0, 4), P(F, 0, 4)};
    const PitchSpelling *reverses[] = {P(C, 0, 4), P(D, 0, 4), P(C, 0, 4), P(D, 0, 4), P(C, 0, 4)};
    const PitchSpelling *repeated[] = {P(C, 0, 4), P(C, 0, 4), P(D, 0, 4)};
    const PitchSpelling *one_move[] = {P(C, 0, 4), P(D, 0, 4)};
    assert(HOLDS(STANDARD_COUNTERPOINT_TENDENCY_STEP_INERTIA, continues));
    assert(!HOLDS(STANDARD_COUNTERPOINT_TENDENCY_STEP_INERTIA, reverses));
    assert(HOLDS(STANDARD_COUNTERPOINT_TENDENCY_STEP_INERTIA, repeated));
    assert(HOLDS(STANDARD_COUNTERPOINT_TENDENCY_STEP_INERTIA, one_move));
}

static void test_melodic_regression(void) {
    const PitchSpelling *turns_back[] = {P(C, 0, 4), P(E, 0, 4), P(G, 0, 4), P(F, 0, 4), P(D, 0, 4), P(C, 0, 4)};
    const PitchSpelling *runs_on_up[] = {P(C, 0, 4), P(D, 0, 4), P(E, 0, 4), P(F, 0, 4),
                                         P(G, 0, 4), P(A, 0, 4), P(B, 0, 4), P(C, 0, 5)};
    const PitchSpelling *runs_on_down[] = {P(E, 0, 4), P(D, 0, 4), P(C, 0, 4), P(B, 0, 3), P(C, 0, 4)};
    const PitchSpelling *flat[] = {P(C, 0, 4), P(C, 0, 4)};
    assert(HOLDS(STANDARD_COUNTERPOINT_TENDENCY_MELODIC_REGRESSION, turns_back));
    /* A4 is in the top quarter and goes on up. */
    assert(!HOLDS(STANDARD_COUNTERPOINT_TENDENCY_MELODIC_REGRESSION, runs_on_up));
    /* C4 is in the bottom quarter and goes on down. */
    assert(!HOLDS(STANDARD_COUNTERPOINT_TENDENCY_MELODIC_REGRESSION, runs_on_down));
    assert(HOLDS(STANDARD_COUNTERPOINT_TENDENCY_MELODIC_REGRESSION, flat));
}

static void test_melodic_arch(void) {
    const PitchSpelling *arch[] = {P(C, 0, 4), P(E, 0, 4), P(G, 0, 4), P(F, 0, 4), P(D, 0, 4), P(C, 0, 4)};
    const PitchSpelling *ends_on_climax[] = {P(C, 0, 4), P(D, 0, 4), P(E, 0, 4)};
    const PitchSpelling *starts_on_climax[] = {P(E, 0, 4), P(D, 0, 4), P(C, 0, 4)};
    const PitchSpelling *late[] = {P(C, 0, 4), P(D, 0, 4), P(E, 0, 4), P(F, 0, 4), P(G, 0, 4),
                                   P(A, 0, 4), P(B, 0, 4), P(C, 0, 5), P(D, 0, 5), P(C, 0, 5)};
    const PitchSpelling *early[] = {P(C, 0, 4), P(G, 0, 4), P(F, 0, 4), P(E, 0, 4), P(D, 0, 4),
                                    P(C, 0, 4), P(D, 0, 4), P(E, 0, 4), P(D, 0, 4), P(C, 0, 4)};
    const PitchSpelling *returns_to_climax[] = {P(C, 0, 4), P(E, 0, 4), P(G, 0, 4), P(F, 0, 4), P(G, 0, 4)};
    assert(HOLDS(STANDARD_COUNTERPOINT_TENDENCY_MELODIC_ARCH, arch));
    assert(!HOLDS(STANDARD_COUNTERPOINT_TENDENCY_MELODIC_ARCH, ends_on_climax));
    assert(!HOLDS(STANDARD_COUNTERPOINT_TENDENCY_MELODIC_ARCH, starts_on_climax));
    assert(!HOLDS(STANDARD_COUNTERPOINT_TENDENCY_MELODIC_ARCH, late));
    assert(!HOLDS(STANDARD_COUNTERPOINT_TENDENCY_MELODIC_ARCH, early));
    assert(!HOLDS(STANDARD_COUNTERPOINT_TENDENCY_MELODIC_ARCH, returns_to_climax));
}

static void test_check_masks(void) {
    const PitchSpelling *smooth[] = {P(C, 0, 4), P(D, 0, 4), P(E, 0, 4), P(G, 0, 4),
                                     P(F, 0, 4), P(E, 0, 4), P(D, 0, 4), P(C, 0, 4)};
    const PitchSpelling *rising_leaps[] = {P(C, 0, 4), P(E, 0, 4), P(G, 0, 4)};
    StandardCounterpointLine smooth_line = LINE_OF(smooth);
    StandardCounterpointLine leaps_line = LINE_OF(rising_leaps);
    unsigned mask = 99;

    assert(standard_counterpoint_tendency_check(&smooth_line, &mask));
    assert(mask == 0);

    assert(standard_counterpoint_tendency_check(&leaps_line, &mask));
    assert(mask == ((1u << STANDARD_COUNTERPOINT_TENDENCY_PITCH_PROXIMITY) |
                    (1u << STANDARD_COUNTERPOINT_TENDENCY_MELODIC_ARCH)));
}

static void test_invalid_input(void) {
    const PitchSpelling *notes[] = {P(C, 0, 4), P(D, 0, 4)};
    const PitchSpelling *with_null[] = {P(C, 0, 4), NULL};
    StandardCounterpointLine line = LINE_OF(notes);
    StandardCounterpointLine empty = {notes, 0};
    StandardCounterpointLine null_note = LINE_OF(with_null);
    unsigned mask = 5;

    assert(!standard_counterpoint_tendency_check(NULL, &mask));
    assert(!standard_counterpoint_tendency_check(&empty, &mask));
    assert(!standard_counterpoint_tendency_check(&null_note, &mask));
    assert(!standard_counterpoint_tendency_check(&line, NULL));
    assert(mask == 5);
    assert(!standard_counterpoint_tendency_holds(STANDARD_COUNTERPOINT_TENDENCY_COUNT, &line));
    assert(!standard_counterpoint_tendency_holds(STANDARD_COUNTERPOINT_TENDENCY_STEP_INERTIA, NULL));
}

int main(void) {
    test_rules_reference_framework();
    test_names();
    test_pitch_proximity();
    test_step_declination();
    test_step_inertia();
    test_melodic_regression();
    test_melodic_arch();
    test_check_masks();
    test_invalid_input();
    printf("test_tendency_rules (standard-counterpoint): ok\n");
    return 0;
}
