#include <assert.h>
#include <stdio.h>

#include "beam_stem_rules.h"
#include "framework.h"

#define MIDDLE STAFF_POSITION_MIDDLE_LINE

static StaffPosition *position(int steps) {
    StaffPosition *p = NULL;
    assert(staff_position_create(steps, &p) == STAFF_POSITION_OK);
    return p;
}

static void test_rules_reference_framework(void) {
    for (int i = 0; i < STANDARD_NOTATION_BEAM_STEM_RULE_COUNT; i++) {
        const Rule *rule = standard_notation_beam_stem_rule((StandardNotationBeamStemRule)i);
        const Constraint *constraint = standard_notation_beam_stem_constraint((StandardNotationBeamStemRule)i);
        assert(rule != NULL && constraint != NULL);
        assert(framework_identity_equals(rule_framework(rule), standard_notation_framework()));
        assert(constraint_type(constraint) == CONSTRAINT_TYPE_HARD);
    }
    assert(standard_notation_beam_stem_rule(STANDARD_NOTATION_BEAM_STEM_RULE_COUNT) == NULL);
}

/* The note farthest from the middle line decides: above means down-stems. */
static void test_farthest_above_points_down(void) {
    StaffPosition *near_above = position(MIDDLE + 1);
    StaffPosition *far_above = position(MIDDLE + 3);
    StaffPosition *near_below = position(MIDDLE - 1);
    const StaffPosition *group[] = {near_above, far_above, near_below};

    assert(standard_notation_beamed_stem_direction_option_count(group, 3) == 1);
    assert(standard_notation_beamed_stem_direction_default(group, 3) == STEM_DIRECTION_DOWN);
    assert(standard_notation_beamed_stem_direction_valid(group, 3, STEM_DIRECTION_DOWN));
    assert(!standard_notation_beamed_stem_direction_valid(group, 3, STEM_DIRECTION_UP));

    staff_position_destroy(near_above);
    staff_position_destroy(far_above);
    staff_position_destroy(near_below);
}

/* Farthest below means up-stems, even when other notes are above the middle line. */
static void test_farthest_below_points_up(void) {
    StaffPosition *above = position(MIDDLE + 1);
    StaffPosition *far_below = position(MIDDLE - 3);
    const StaffPosition *group[] = {above, far_below};

    assert(standard_notation_beamed_stem_direction_option_count(group, 2) == 1);
    assert(standard_notation_beamed_stem_direction_default(group, 2) == STEM_DIRECTION_UP);
    assert(standard_notation_beamed_stem_direction_valid(group, 2, STEM_DIRECTION_UP));
    assert(!standard_notation_beamed_stem_direction_valid(group, 2, STEM_DIRECTION_DOWN));

    staff_position_destroy(above);
    staff_position_destroy(far_below);
}

/* Equally far above and below, or all on the middle line: either way. */
static void test_equidistant_allows_either(void) {
    StaffPosition *above = position(MIDDLE + 3);
    StaffPosition *below = position(MIDDLE - 3);
    const StaffPosition *group[] = {above, below};
    assert(standard_notation_beamed_stem_direction_option_count(group, 2) == 2);
    assert(standard_notation_beamed_stem_direction_default(group, 2) == STEM_DIRECTION_DOWN);
    assert(standard_notation_beamed_stem_direction_valid(group, 2, STEM_DIRECTION_DOWN));
    assert(standard_notation_beamed_stem_direction_valid(group, 2, STEM_DIRECTION_UP));
    staff_position_destroy(above);
    staff_position_destroy(below);

    StaffPosition *middle = position(MIDDLE);
    const StaffPosition *on_middle[] = {middle, middle};
    assert(standard_notation_beamed_stem_direction_option_count(on_middle, 2) == 2);
    assert(standard_notation_beamed_stem_direction_valid(on_middle, 2, STEM_DIRECTION_UP));
    assert(standard_notation_beamed_stem_direction_valid(on_middle, 2, STEM_DIRECTION_DOWN));
    staff_position_destroy(middle);
}

/* A one-note group follows the single-note stem rule. */
static void test_single_note_matches_stem_rule(void) {
    for (int steps = -4; steps <= 12; steps++) {
        StaffPosition *p = position(steps);
        const StaffPosition *group[] = {p};
        assert(standard_notation_beamed_stem_direction_option_count(group, 1) ==
               standard_notation_stem_direction_option_count(p));
        assert(standard_notation_beamed_stem_direction_default(group, 1) ==
               standard_notation_stem_direction_default(p));
        assert(standard_notation_beamed_stem_direction_valid(group, 1, STEM_DIRECTION_UP) ==
               standard_notation_stem_direction_valid(p, STEM_DIRECTION_UP));
        assert(standard_notation_beamed_stem_direction_valid(group, 1, STEM_DIRECTION_DOWN) ==
               standard_notation_stem_direction_valid(p, STEM_DIRECTION_DOWN));
        staff_position_destroy(p);
    }
}

static void test_declines(void) {
    StaffPosition *p = position(MIDDLE + 1);
    const StaffPosition *with_null[] = {p, NULL};

    assert(standard_notation_beamed_stem_direction_option_count(NULL, 0) == 0);
    assert(standard_notation_beamed_stem_direction_option_count(with_null, 0) == 0);
    assert(standard_notation_beamed_stem_direction_option_count(with_null, 2) == 0);
    assert(!standard_notation_beamed_stem_direction_valid(NULL, 0, STEM_DIRECTION_DOWN));
    assert(!standard_notation_beamed_stem_direction_valid(with_null, 2, STEM_DIRECTION_DOWN));

    staff_position_destroy(p);
}

int main(void) {
    test_rules_reference_framework();
    test_farthest_above_points_down();
    test_farthest_below_points_up();
    test_equidistant_allows_either();
    test_single_note_matches_stem_rule();
    test_declines();
    printf("test_beam_stem_rules (standard-notation): ok\n");
    return 0;
}
