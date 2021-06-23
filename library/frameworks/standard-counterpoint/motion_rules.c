#include "motion_rules.h"

#include <stddef.h>

#include "counterpoint_pitch.h"
#include "rule_pair.h"

static const char *const NAMES[STANDARD_COUNTERPOINT_MOTION_RULE_COUNT] = {
    "motion-contrary", "motion-similar", "motion-parallel", "motion-oblique", "motion-static"};

static StandardCounterpointRulePair g_pairs[STANDARD_COUNTERPOINT_MOTION_RULE_COUNT];
static bool g_built[STANDARD_COUNTERPOINT_MOTION_RULE_COUNT];

static bool valid_rule(StandardCounterpointMotionRule which) {
    return which >= 0 && which < STANDARD_COUNTERPOINT_MOTION_RULE_COUNT;
}

static void ensure_built(StandardCounterpointMotionRule which) {
    if (!valid_rule(which) || g_built[which]) {
        return;
    }
    g_built[which] = true;
    standard_counterpoint_build_rule_pair(NAMES[which], CONSTRAINT_TYPE_HARD, &g_pairs[which]);
}

const Rule *standard_counterpoint_motion_rule(StandardCounterpointMotionRule which) {
    ensure_built(which);
    return valid_rule(which) ? g_pairs[which].rule : NULL;
}

const Constraint *standard_counterpoint_motion_constraint(StandardCounterpointMotionRule which) {
    ensure_built(which);
    return valid_rule(which) ? g_pairs[which].constraint : NULL;
}

bool standard_counterpoint_classify_motion(const PitchSpelling *lower_from, const PitchSpelling *lower_to,
                                           const PitchSpelling *upper_from, const PitchSpelling *upper_to,
                                           MotionType *out) {
    if (lower_from == NULL || lower_to == NULL || upper_from == NULL || upper_to == NULL || out == NULL) {
        return false;
    }
    bool lower_moves = !standard_counterpoint_same_pitch(lower_from, lower_to);
    bool upper_moves = !standard_counterpoint_same_pitch(upper_from, upper_to);

    if (!lower_moves && !upper_moves) {
        *out = MOTION_TYPE_STATIC;
    } else if (lower_moves != upper_moves) {
        *out = MOTION_TYPE_OBLIQUE;
    } else if (standard_counterpoint_direction(lower_from, lower_to) !=
               standard_counterpoint_direction(upper_from, upper_to)) {
        *out = MOTION_TYPE_CONTRARY;
    } else {
        int lower_steps = standard_counterpoint_step_number(lower_to) - standard_counterpoint_step_number(lower_from);
        int upper_steps = standard_counterpoint_step_number(upper_to) - standard_counterpoint_step_number(upper_from);
        *out = lower_steps == upper_steps ? MOTION_TYPE_PARALLEL : MOTION_TYPE_SIMILAR;
    }
    return true;
}

bool standard_counterpoint_pair_motion(const StandardCounterpointPair *pair, size_t index, MotionType *out) {
    if (!standard_counterpoint_pair_valid(pair) || index + 1 >= pair->lower->count) {
        return false;
    }
    return standard_counterpoint_classify_motion(pair->lower->notes[index], pair->lower->notes[index + 1],
                                                 pair->upper->notes[index], pair->upper->notes[index + 1], out);
}
