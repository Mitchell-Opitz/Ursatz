#include "theory/counterpoint/motion_rules.h"

#include <stddef.h>

#include "model/pitch/pitch_arithmetic.h"
#include "model/pitch/pitch_spelling.h"
#include "theory/counterpoint/rule_table.h"

bool standard_counterpoint_classify_motion(const PitchSpelling *lower_from, const PitchSpelling *lower_to,
                                           const PitchSpelling *upper_from, const PitchSpelling *upper_to,
                                           MotionType *out) {
    if (lower_from == NULL || lower_to == NULL || upper_from == NULL || upper_to == NULL || out == NULL) {
        return false;
    }
    bool lower_moves = !pitch_spelling_equals(lower_from, lower_to);
    bool upper_moves = !pitch_spelling_equals(upper_from, upper_to);

    if (!lower_moves && !upper_moves) {
        *out = MOTION_TYPE_STATIC;
    } else if (lower_moves != upper_moves) {
        *out = MOTION_TYPE_OBLIQUE;
    } else if (pitch_spelling_direction(lower_from, lower_to) !=
               pitch_spelling_direction(upper_from, upper_to)) {
        *out = MOTION_TYPE_CONTRARY;
    } else {
        int lower_steps = pitch_spelling_step_number(lower_to) - pitch_spelling_step_number(lower_from);
        int upper_steps = pitch_spelling_step_number(upper_to) - pitch_spelling_step_number(upper_from);
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
