#include "motive_rules.h"

#include "rule_pair.h"

static const char *const NAMES[STANDARD_FORM_MOTIVE_RULE_COUNT] = {
    "motive-short",
    "motive-default-kind",
    "motive-transformations"};

static RulePairTable g_table = RULE_PAIR_TABLE_INIT(standard_form_framework, "standard-form",
    NAMES, STANDARD_FORM_MOTIVE_RULE_COUNT);

static bool valid_rule(StandardFormMotiveRule which) {
    return which >= 0 && which < STANDARD_FORM_MOTIVE_RULE_COUNT;
}

const Rule *standard_form_motive_rule(StandardFormMotiveRule which) {
    const RulePair *pair = valid_rule(which) ? rule_pair_table_get(&g_table, (size_t)which) : NULL;
    return pair != NULL ? pair->rule : NULL;
}

const Constraint *standard_form_motive_constraint(StandardFormMotiveRule which) {
    const RulePair *pair = valid_rule(which) ? rule_pair_table_get(&g_table, (size_t)which) : NULL;
    return pair != NULL ? pair->constraint : NULL;
}

bool standard_form_motive_fits_idea(size_t motive_event_count, size_t idea_event_count) {
    return motive_event_count > 0 && motive_event_count < idea_event_count;
}

MotiveKind standard_form_motive_default_kind(void) {
    return MOTIVE_KIND_PITCH;
}

static const char *const TRANSFORMATION_NAMES[STANDARD_FORM_MOTIVE_TRANSFORMATION_COUNT] = {
    "enlargement", "contraction", "inversion", "displacement", "retrograde", "intervallic-manipulation",
    "embellishment"};

static const char *const TRANSFORMATION_TYPES[STANDARD_FORM_MOTIVE_TRANSFORMATION_COUNT] = {
    "augmentation", "diminution",  "inversion",  "displacement",
    "retrograde",   "intervallic_manipulation", "ornamentation"};

bool standard_form_motive_transformation_valid(StandardFormMotiveTransformation transformation) {
    return transformation >= 0 && transformation < STANDARD_FORM_MOTIVE_TRANSFORMATION_COUNT;
}

const char *standard_form_motive_transformation_name(StandardFormMotiveTransformation transformation) {
    return standard_form_motive_transformation_valid(transformation) ? TRANSFORMATION_NAMES[transformation] : NULL;
}

const char *standard_form_motive_transformation_type(StandardFormMotiveTransformation transformation) {
    return standard_form_motive_transformation_valid(transformation) ? TRANSFORMATION_TYPES[transformation] : NULL;
}
