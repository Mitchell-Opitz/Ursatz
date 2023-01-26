#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "internal_expansion_rules.h"
#include "rule_check.h"

static void test_rules_reference_framework(void) {
    CHECK_RULES_REFERENCE_FRAMEWORK(standard_form_internal_expansion_rule, standard_form_internal_expansion_constraint,
                                    STANDARD_FORM_INTERNAL_EXPANSION_RULE_COUNT);
}

/* After the phrase's beginning, before its cadence. */
static void test_location(void) {
    assert(standard_form_internal_expansion_inside(2, 8, 2));
    assert(standard_form_internal_expansion_inside(2, 8, 5));
    assert(standard_form_internal_expansion_inside(2, 8, 7));
    assert(!standard_form_internal_expansion_inside(2, 8, 1));
    assert(!standard_form_internal_expansion_inside(2, 8, 8));
    assert(!standard_form_internal_expansion_inside(2, 8, 9));
}

static void test_combination(void) {
    unsigned all = 0;
    for (int i = 0; i < INTERNAL_EXPANSION_TECHNIQUE_COUNT; i++) {
        unsigned bit = standard_form_internal_technique_bit((InternalExpansionTechnique)i);
        assert(bit != 0);
        assert((all & bit) == 0);
        all |= bit;
    }
    assert(standard_form_internal_technique_count(all) == INTERNAL_EXPANSION_TECHNIQUE_COUNT);
    assert(standard_form_internal_technique_count(0) == 0);
    assert(standard_form_internal_technique_bit(INTERNAL_EXPANSION_TECHNIQUE_COUNT) == 0);

    unsigned repetition = standard_form_internal_technique_bit(INTERNAL_EXPANSION_TECHNIQUE_REPETITION);
    unsigned stretching = standard_form_internal_technique_bit(INTERNAL_EXPANSION_TECHNIQUE_STRETCHING);
    assert(standard_form_internal_expansion_is_combined(repetition | stretching));
    assert(!standard_form_internal_expansion_is_combined(repetition));
    assert(!standard_form_internal_expansion_is_combined(0));
}

/* A presentation's repeated basic idea is predictable, so it is not an expansion. */
static void test_repetition(void) {
    assert(standard_form_repetition_creates_expansion(true, false));
    assert(!standard_form_repetition_creates_expansion(false, false));
    assert(!standard_form_repetition_creates_expansion(true, true));
    assert(!standard_form_repetition_creates_expansion(false, true));
}

static void test_one_more_time(void) {
    assert(strcmp(standard_form_omt_step_name(STANDARD_FORM_OMT_STEP_TRY_CADENCE), "try-cadence") == 0);
    assert(strcmp(standard_form_omt_step_name(STANDARD_FORM_OMT_STEP_EVADE_CADENCE), "evade-cadence") == 0);
    assert(strcmp(standard_form_omt_step_name(STANDARD_FORM_OMT_STEP_RETRY_CADENCE), "retry-cadence") == 0);
    assert(standard_form_omt_step_name(STANDARD_FORM_OMT_STEP_COUNT) == NULL);
    assert(!standard_form_omt_step_valid(STANDARD_FORM_OMT_STEP_COUNT));
    assert(!standard_form_omt_step_valid((StandardFormOneMoreTimeStep)-1));

    StandardFormOneMoreTimeStep step = STANDARD_FORM_OMT_STEP_TRY_CADENCE;
    assert(standard_form_omt_step_next(step, &step));
    assert(step == STANDARD_FORM_OMT_STEP_EVADE_CADENCE);
    assert(standard_form_omt_step_next(step, &step));
    assert(step == STANDARD_FORM_OMT_STEP_RETRY_CADENCE);
    assert(!standard_form_omt_step_next(step, &step));
    assert(step == STANDARD_FORM_OMT_STEP_RETRY_CADENCE);
    assert(!standard_form_omt_step_next(STANDARD_FORM_OMT_STEP_COUNT, &step));

    assert(standard_form_is_one_more_time(true, true, true));
    assert(!standard_form_is_one_more_time(true, true, false));
    assert(!standard_form_is_one_more_time(true, false, true));
    assert(!standard_form_is_one_more_time(false, true, true));
}

/* A detour returns to previous material before the cadence; a reroute does not. */
static void test_alternative_path(void) {
    assert(standard_form_classify_alternative_path(true) == ALTERNATIVE_PATH_KIND_DETOUR);
    assert(standard_form_classify_alternative_path(false) == ALTERNATIVE_PATH_KIND_REROUTE);
}

/* One-more-time has already tried and failed to cadence; alternative path has not. */
static void test_technique_choice(void) {
    assert(standard_form_one_more_time_or_alternative_path(true) == INTERNAL_EXPANSION_TECHNIQUE_ONE_MORE_TIME);
    assert(standard_form_one_more_time_or_alternative_path(false) == INTERNAL_EXPANSION_TECHNIQUE_ALTERNATIVE_PATH);
}

int main(void) {
    test_rules_reference_framework();
    test_location();
    test_combination();
    test_repetition();
    test_one_more_time();
    test_alternative_path();
    test_technique_choice();
    printf("test_internal_expansion_rules (standard-form): ok\n");
    return 0;
}
