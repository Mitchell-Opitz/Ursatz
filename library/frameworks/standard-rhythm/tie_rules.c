#include "tie_rules.h"

#include "rule_pair.h"

static const char *const NAMES[STANDARD_RHYTHM_TIE_RULE_COUNT] = {"tie-same-pitch", "tie-not-on-rests",
                                                                   "tie-no-rearticulation", "tie-duration-sum"};

static StandardRhythmRulePair g_pairs[STANDARD_RHYTHM_TIE_RULE_COUNT];
static bool g_built[STANDARD_RHYTHM_TIE_RULE_COUNT];

static bool valid_rule(StandardRhythmTieRule which) {
    return which >= 0 && which < STANDARD_RHYTHM_TIE_RULE_COUNT;
}

static void ensure_built(StandardRhythmTieRule which) {
    if (!valid_rule(which) || g_built[which]) {
        return;
    }
    g_built[which] = true;
    standard_rhythm_build_rule_pair(NAMES[which], &g_pairs[which]);
}

const Rule *standard_rhythm_tie_rule(StandardRhythmTieRule which) {
    ensure_built(which);
    return valid_rule(which) ? g_pairs[which].rule : NULL;
}

const Constraint *standard_rhythm_tie_constraint(StandardRhythmTieRule which) {
    ensure_built(which);
    return valid_rule(which) ? g_pairs[which].constraint : NULL;
}

bool standard_rhythm_tie_same_pitch(const PitchSpelling *first, const PitchSpelling *second) {
    if (first == NULL || second == NULL) {
        return false;
    }
    return pitch_spelling_equals(first, second);
}

bool standard_rhythm_tie_allowed_between(const PitchSpelling *first, const PitchSpelling *second) {
    return first != NULL && second != NULL;
}

bool standard_rhythm_tie_valid(const PitchSpelling *first, const PitchSpelling *second) {
    return standard_rhythm_tie_allowed_between(first, second) && standard_rhythm_tie_same_pitch(first, second);
}

bool standard_rhythm_tied_note_articulated(size_t index) {
    return index == 0;
}

bool standard_rhythm_tied_duration(const NoteValue *const *values, size_t count, Duration **out) {
    if (values == NULL || count == 0) {
        return false;
    }

    Duration *total = NULL;
    for (size_t i = 0; i < count; i++) {
        Duration *part = NULL;
        if (values[i] == NULL || note_value_duration(values[i], &part) != DURATION_OK) {
            duration_destroy(total);
            return false;
        }
        if (total == NULL) {
            total = part;
            continue;
        }
        Duration *sum = NULL;
        DurationResult result = duration_add(total, part, &sum);
        duration_destroy(total);
        duration_destroy(part);
        if (result != DURATION_OK) {
            return false;
        }
        total = sum;
    }

    *out = total;
    return true;
}
