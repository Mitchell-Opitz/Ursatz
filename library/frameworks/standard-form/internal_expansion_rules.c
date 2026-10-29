#include "internal_expansion_rules.h"

#include "rule_pair.h"
#include <stddef.h>

static const char *const NAMES[STANDARD_FORM_INTERNAL_EXPANSION_RULE_COUNT] = {
    "internal-expansion-location",
    "internal-expansion-combination",
    "internal-expansion-repetition",
    "internal-expansion-one-more-time",
    "internal-expansion-alternative-path",
    "internal-expansion-technique-choice"};

static RulePairTable g_table = RULE_PAIR_TABLE_INIT(standard_form_framework, "standard-form",
    NAMES, STANDARD_FORM_INTERNAL_EXPANSION_RULE_COUNT);

static bool valid_rule(StandardFormInternalExpansionRule which) {
    return which >= 0 && which < STANDARD_FORM_INTERNAL_EXPANSION_RULE_COUNT;
}

const Rule *standard_form_internal_expansion_rule(StandardFormInternalExpansionRule which) {
    const RulePair *pair = valid_rule(which) ? rule_pair_table_get(&g_table, (size_t)which) : NULL;
    return pair != NULL ? pair->rule : NULL;
}

const Constraint *standard_form_internal_expansion_constraint(StandardFormInternalExpansionRule which) {
    const RulePair *pair = valid_rule(which) ? rule_pair_table_get(&g_table, (size_t)which) : NULL;
    return pair != NULL ? pair->constraint : NULL;
}

bool standard_form_internal_expansion_inside(size_t beginning_end, size_t cadence_start, size_t position) {
    return position >= beginning_end && position < cadence_start;
}

unsigned standard_form_internal_technique_bit(InternalExpansionTechnique technique) {
    return internal_expansion_technique_valid(technique) ? 1u << (unsigned)technique : 0u;
}

size_t standard_form_internal_technique_count(unsigned techniques) {
    size_t count = 0;
    for (int i = 0; i < INTERNAL_EXPANSION_TECHNIQUE_COUNT; i++) {
        if (techniques & standard_form_internal_technique_bit((InternalExpansionTechnique)i)) {
            count++;
        }
    }
    return count;
}

bool standard_form_internal_expansion_is_combined(unsigned techniques) {
    return standard_form_internal_technique_count(techniques) > 1;
}

bool standard_form_repetition_creates_expansion(bool unexpected, bool is_presentation_basic_idea_repeat) {
    return unexpected && !is_presentation_basic_idea_repeat;
}

static const char *const OMT_STEP_NAMES[STANDARD_FORM_OMT_STEP_COUNT] = {"try-cadence", "evade-cadence",
                                                                         "retry-cadence"};

bool standard_form_omt_step_valid(StandardFormOneMoreTimeStep step) {
    return step >= 0 && step < STANDARD_FORM_OMT_STEP_COUNT;
}

const char *standard_form_omt_step_name(StandardFormOneMoreTimeStep step) {
    return standard_form_omt_step_valid(step) ? OMT_STEP_NAMES[step] : NULL;
}

bool standard_form_omt_step_next(StandardFormOneMoreTimeStep step, StandardFormOneMoreTimeStep *out) {
    if (!standard_form_omt_step_valid(step) || step + 1 >= STANDARD_FORM_OMT_STEP_COUNT) {
        return false;
    }
    *out = (StandardFormOneMoreTimeStep)(step + 1);
    return true;
}

bool standard_form_is_one_more_time(bool tried, bool evaded, bool retried) {
    return tried && evaded && retried;
}

AlternativePathKind standard_form_classify_alternative_path(bool returns_to_previous_material) {
    return returns_to_previous_material ? ALTERNATIVE_PATH_KIND_DETOUR : ALTERNATIVE_PATH_KIND_REROUTE;
}

InternalExpansionTechnique standard_form_one_more_time_or_alternative_path(bool cadence_tried_and_failed) {
    return cadence_tried_and_failed ? INTERNAL_EXPANSION_TECHNIQUE_ONE_MORE_TIME
                                    : INTERNAL_EXPANSION_TECHNIQUE_ALTERNATIVE_PATH;
}
