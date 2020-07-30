#include <assert.h>
#include <stdio.h>

#include "framework.h"
#include "structural_rules.h"

static void test_rules_reference_framework(void) {
    for (int i = 0; i < STANDARD_MARKINGS_STRUCTURAL_RULE_COUNT; i++) {
        const Rule *rule = standard_markings_structural_rule((StandardMarkingsStructuralRule)i);
        const Constraint *constraint = standard_markings_structural_constraint((StandardMarkingsStructuralRule)i);
        assert(rule != NULL && constraint != NULL);
        assert(framework_identity_equals(rule_framework(rule), standard_markings_framework()));
        assert(constraint_type(constraint) == CONSTRAINT_TYPE_HARD);
    }
    assert(standard_markings_structural_rule(STANDARD_MARKINGS_STRUCTURAL_RULE_COUNT) == NULL);
}

static void test_kind_names(void) {
    assert(standard_markings_structural_kind("fermata") == STRUCTURAL_KIND_FERMATA);
    assert(standard_markings_structural_kind("caesura") == STRUCTURAL_KIND_CAESURA);
    assert(standard_markings_structural_kind("breath-mark") == STRUCTURAL_KIND_BREATH_MARK);
    assert(standard_markings_structural_kind("repeat-start") == STRUCTURAL_KIND_REPEAT_START);
    assert(standard_markings_structural_kind("repeat-end") == STRUCTURAL_KIND_REPEAT_END);
    assert(standard_markings_structural_kind("ending") == STRUCTURAL_KIND_ENDING);
    assert(standard_markings_structural_kind("one-measure-repeat") == STRUCTURAL_KIND_ONE_MEASURE_REPEAT);
    assert(standard_markings_structural_kind("two-measure-repeat") == STRUCTURAL_KIND_TWO_MEASURE_REPEAT);
    assert(standard_markings_structural_kind("segno") == STRUCTURAL_KIND_SEGNO);
    assert(standard_markings_structural_kind("coda") == STRUCTURAL_KIND_CODA);
    assert(standard_markings_structural_kind("to-coda") == STRUCTURAL_KIND_TO_CODA);
    assert(standard_markings_structural_kind("fine") == STRUCTURAL_KIND_FINE);
    assert(standard_markings_structural_kind("dc-al-coda") == STRUCTURAL_KIND_DC_AL_CODA);
    assert(standard_markings_structural_kind("dc-al-fine") == STRUCTURAL_KIND_DC_AL_FINE);
    assert(standard_markings_structural_kind("ds-al-coda") == STRUCTURAL_KIND_DS_AL_CODA);
    assert(standard_markings_structural_kind("ds-al-fine") == STRUCTURAL_KIND_DS_AL_FINE);

    assert(standard_markings_structural_kind("FERMATA") == STRUCTURAL_KIND_FERMATA);
    assert(standard_markings_structural_kind("trill") == STRUCTURAL_KIND_UNKNOWN);
    assert(standard_markings_structural_kind("") == STRUCTURAL_KIND_UNKNOWN);
    assert(standard_markings_structural_kind(NULL) == STRUCTURAL_KIND_UNKNOWN);
}

/* Fermata holds a note longer; caesura and breath mark break the flow. */
static void test_fermata_caesura_breath(void) {
    assert(standard_markings_extends_duration(STRUCTURAL_KIND_FERMATA));
    assert(!standard_markings_extends_duration(STRUCTURAL_KIND_CAESURA));

    assert(standard_markings_interrupts_flow(STRUCTURAL_KIND_CAESURA));
    assert(standard_markings_interrupts_flow(STRUCTURAL_KIND_BREATH_MARK));
    assert(!standard_markings_interrupts_flow(STRUCTURAL_KIND_FERMATA));
    assert(!standard_markings_interrupts_flow(STRUCTURAL_KIND_UNKNOWN));
}

/* Endings are numbered from 1 (a third or fourth ending is common in some styles). */
static void test_numbered_endings(void) {
    assert(!standard_markings_marker_well_formed(STRUCTURAL_KIND_ENDING, 0));
    for (unsigned int number = 1; number <= 4; number++) {
        assert(standard_markings_marker_well_formed(STRUCTURAL_KIND_ENDING, number));
    }
    assert(standard_markings_marker_well_formed(STRUCTURAL_KIND_FERMATA, 0));
    assert(standard_markings_marker_well_formed(STRUCTURAL_KIND_REPEAT_END, 7)); /* parameter ignored */
    assert(!standard_markings_marker_well_formed(STRUCTURAL_KIND_UNKNOWN, 0));
}

static void test_measure_repeats(void) {
    assert(standard_markings_measure_repeat_span(STRUCTURAL_KIND_ONE_MEASURE_REPEAT) == 1);
    assert(standard_markings_measure_repeat_span(STRUCTURAL_KIND_TWO_MEASURE_REPEAT) == 2);
    assert(standard_markings_measure_repeat_span(STRUCTURAL_KIND_REPEAT_END) == 0);
}

static void test_jump_markers(void) {
    assert(standard_markings_jump_target(STRUCTURAL_KIND_DC_AL_CODA) == STRUCTURAL_JUMP_TO_START);
    assert(standard_markings_jump_target(STRUCTURAL_KIND_DC_AL_FINE) == STRUCTURAL_JUMP_TO_START);
    assert(standard_markings_jump_target(STRUCTURAL_KIND_DS_AL_CODA) == STRUCTURAL_JUMP_TO_SEGNO);
    assert(standard_markings_jump_target(STRUCTURAL_KIND_DS_AL_FINE) == STRUCTURAL_JUMP_TO_SEGNO);
    assert(standard_markings_jump_target(STRUCTURAL_KIND_FINE) == STRUCTURAL_JUMP_NONE);

    assert(standard_markings_jump_stop(STRUCTURAL_KIND_DC_AL_CODA) == STRUCTURAL_STOP_AT_CODA);
    assert(standard_markings_jump_stop(STRUCTURAL_KIND_DS_AL_CODA) == STRUCTURAL_STOP_AT_CODA);
    assert(standard_markings_jump_stop(STRUCTURAL_KIND_DC_AL_FINE) == STRUCTURAL_STOP_AT_FINE);
    assert(standard_markings_jump_stop(STRUCTURAL_KIND_DS_AL_FINE) == STRUCTURAL_STOP_AT_FINE);
    assert(standard_markings_jump_stop(STRUCTURAL_KIND_CODA) == STRUCTURAL_STOP_NONE);
}

int main(void) {
    test_rules_reference_framework();
    test_kind_names();
    test_fermata_caesura_breath();
    test_numbered_endings();
    test_measure_repeats();
    test_jump_markers();
    printf("test_structural_rules (standard-markings): ok\n");
    return 0;
}
