#include <assert.h>
#include <stdio.h>

#include "framework.h"
#include "pitch/clef_resolution.h"
#include "staff_clef_rules.h"

static StaffPosition *position(int steps) {
    StaffPosition *p = NULL;
    assert(staff_position_create(steps, &p) == STAFF_POSITION_OK);
    return p;
}

static void test_rules_reference_framework(void) {
    for (int i = 0; i < STANDARD_NOTATION_STAFF_RULE_COUNT; i++) {
        const Rule *rule = standard_notation_staff_rule((StandardNotationStaffRule)i);
        const Constraint *constraint = standard_notation_staff_constraint((StandardNotationStaffRule)i);
        assert(rule != NULL && constraint != NULL);
        assert(framework_identity_equals(rule_framework(rule), standard_notation_framework()));
        assert(framework_identity_equals(constraint_framework(constraint), standard_notation_framework()));
        assert(constraint_type(constraint) == CONSTRAINT_TYPE_HARD);
        /* Cached: same instance on every call. */
        assert(rule == standard_notation_staff_rule((StandardNotationStaffRule)i));
    }
    assert(standard_notation_staff_rule(STANDARD_NOTATION_STAFF_RULE_COUNT) == NULL);
    assert(standard_notation_staff_constraint((StandardNotationStaffRule)-1) == NULL);
}

static void test_five_lines(void) {
    assert(standard_notation_staff_line_count() == 5);
}

static void test_clef_reference(void) {
    Clef *treble = NULL, *bass = NULL, *alto = NULL, *tenor = NULL;
    assert(clef_create_treble(&treble) == CLEF_OK);
    assert(clef_create_bass(&bass) == CLEF_OK);
    assert(clef_create_alto(&alto) == CLEF_OK);
    assert(clef_create_tenor(&tenor) == CLEF_OK);

    StaffPosition *line2 = position(2), *line3 = position(4), *line4 = position(6);
    assert(standard_notation_clef_places_reference(treble, line2));
    assert(!standard_notation_clef_places_reference(treble, line3));
    assert(standard_notation_clef_places_reference(alto, line3));
    assert(standard_notation_clef_places_reference(bass, line4));
    assert(standard_notation_clef_places_reference(tenor, line4));
    assert(!standard_notation_clef_places_reference(NULL, line2));
    assert(!standard_notation_clef_places_reference(treble, NULL));

    staff_position_destroy(line2);
    staff_position_destroy(line3);
    staff_position_destroy(line4);
    clef_destroy(treble);
    clef_destroy(bass);
    clef_destroy(alto);
    clef_destroy(tenor);
}

static void test_pitch_height(void) {
    Clef *clefs[4];
    assert(clef_create_treble(&clefs[0]) == CLEF_OK);
    assert(clef_create_bass(&clefs[1]) == CLEF_OK);
    assert(clef_create_alto(&clefs[2]) == CLEF_OK);
    assert(clef_create_tenor(&clefs[3]) == CLEF_OK);

    for (int c = 0; c < 4; c++) {
        for (int lo = -4; lo <= 12; lo++) {
            for (int hi = -4; hi <= 12; hi++) {
                StaffPosition *a = position(lo), *b = position(hi);
                assert(standard_notation_pitch_height_respected(clefs[c], a, b));
                staff_position_destroy(a);
                staff_position_destroy(b);
            }
        }
        clef_destroy(clefs[c]);
    }
}

/* A neutral clef assigns no reference pitch and orders no pitches. */
static void test_neutral_clef(void) {
    Clef *neutral = NULL;
    assert(clef_create_neutral(&neutral) == CLEF_OK);

    StaffPosition *a = position(2), *b = position(4);
    assert(!standard_notation_clef_places_reference(neutral, a));
    assert(!standard_notation_pitch_height_respected(neutral, a, b));

    staff_position_destroy(a);
    staff_position_destroy(b);
    clef_destroy(neutral);
}

int main(void) {
    test_rules_reference_framework();
    test_five_lines();
    test_clef_reference();
    test_pitch_height();
    test_neutral_clef();
    printf("test_staff_clef_rules (standard-notation): ok\n");
    return 0;
}
