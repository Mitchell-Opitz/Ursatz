#include "motive_rules.h"

#include "rule_pair.h"

static const char *const NAMES[STANDARD_FORM_MOTIVE_RULE_COUNT] = {
    "motive-short",
    "motive-default-kind",
    "motive-transformations"};

static StandardFormRulePair g_pairs[STANDARD_FORM_MOTIVE_RULE_COUNT];
static bool g_built[STANDARD_FORM_MOTIVE_RULE_COUNT];

static bool valid_rule(StandardFormMotiveRule which) {
    return which >= 0 && which < STANDARD_FORM_MOTIVE_RULE_COUNT;
}

static void ensure_built(StandardFormMotiveRule which) {
    if (!valid_rule(which) || g_built[which]) {
        return;
    }
    g_built[which] = true;
    standard_form_build_rule_pair(NAMES[which], &g_pairs[which]);
}

const Rule *standard_form_motive_rule(StandardFormMotiveRule which) {
    ensure_built(which);
    return valid_rule(which) ? g_pairs[which].rule : NULL;
}

const Constraint *standard_form_motive_constraint(StandardFormMotiveRule which) {
    ensure_built(which);
    return valid_rule(which) ? g_pairs[which].constraint : NULL;
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
