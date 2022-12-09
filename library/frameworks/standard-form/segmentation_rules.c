#include "segmentation_rules.h"

#include "rule_pair.h"

static const char *const NAMES[STANDARD_FORM_SEGMENTATION_RULE_COUNT] = {
    "segmentation-step-order",
    "segmentation-idea-length",
    "segmentation-idea-division"};

static StandardFormRulePair g_pairs[STANDARD_FORM_SEGMENTATION_RULE_COUNT];
static bool g_built[STANDARD_FORM_SEGMENTATION_RULE_COUNT];

static bool valid_rule(StandardFormSegmentationRule which) {
    return which >= 0 && which < STANDARD_FORM_SEGMENTATION_RULE_COUNT;
}

static void ensure_built(StandardFormSegmentationRule which) {
    if (!valid_rule(which) || g_built[which]) {
        return;
    }
    g_built[which] = true;
    standard_form_build_rule_pair(NAMES[which], &g_pairs[which]);
}

const Rule *standard_form_segmentation_rule(StandardFormSegmentationRule which) {
    ensure_built(which);
    return valid_rule(which) ? g_pairs[which].rule : NULL;
}

const Constraint *standard_form_segmentation_constraint(StandardFormSegmentationRule which) {
    ensure_built(which);
    return valid_rule(which) ? g_pairs[which].constraint : NULL;
}

static const char *const STEP_NAMES[STANDARD_FORM_SEGMENTATION_STEP_COUNT] = {
    "identify-closure", "divide-into-ideas", "bracket-ideas", "verify-cadences"};

bool standard_form_segmentation_step_valid(StandardFormSegmentationStep step) {
    return step >= 0 && step < STANDARD_FORM_SEGMENTATION_STEP_COUNT;
}

const char *standard_form_segmentation_step_name(StandardFormSegmentationStep step) {
    return standard_form_segmentation_step_valid(step) ? STEP_NAMES[step] : NULL;
}

bool standard_form_segmentation_step_next(StandardFormSegmentationStep step, StandardFormSegmentationStep *out) {
    if (!standard_form_segmentation_step_valid(step) || step + 1 >= STANDARD_FORM_SEGMENTATION_STEP_COUNT) {
        return false;
    }
    *out = (StandardFormSegmentationStep)(step + 1);
    return true;
}

size_t standard_form_closure_points(const bool *cadence_after, size_t count, size_t *out, size_t out_capacity) {
    size_t found = 0;
    for (size_t i = 0; cadence_after != NULL && i < count; i++) {
        if (!cadence_after[i]) {
            continue;
        }
        if (out != NULL && found < out_capacity) {
            out[found] = i;
        }
        found++;
    }
    return found;
}

size_t standard_form_idea_typical_measures(void) {
    return 2;
}

bool standard_form_idea_length_valid(size_t measures) {
    return measures >= 1;
}

bool standard_form_ideas_fill_phrase(const size_t *idea_measures, size_t idea_count, size_t phrase_measures) {
    if (idea_measures == NULL || idea_count == 0) {
        return false;
    }
    size_t total = 0;
    for (size_t i = 0; i < idea_count; i++) {
        if (!standard_form_idea_length_valid(idea_measures[i])) {
            return false;
        }
        total += idea_measures[i];
    }
    return total == phrase_measures;
}
