#include "major_scale_rules.h"

#include <stddef.h>

#include "rule_pair.h"
#include "scale_pattern.h"

static const char *const NAMES[STANDARD_TONALITY_MAJOR_SCALE_RULE_COUNT] = {"major-scale-step-pattern",
                                                                             "major-scale-named-by-tonic"};

static RulePairTable g_table = RULE_PAIR_TABLE_INIT(standard_tonality_framework, "standard-tonality",
    NAMES, STANDARD_TONALITY_MAJOR_SCALE_RULE_COUNT);

static bool valid_rule(StandardTonalityMajorScaleRule which) {
    return which >= 0 && which < STANDARD_TONALITY_MAJOR_SCALE_RULE_COUNT;
}

const Rule *standard_tonality_major_scale_rule(StandardTonalityMajorScaleRule which) {
    const RulePair *pair = valid_rule(which) ? rule_pair_table_get(&g_table, (size_t)which) : NULL;
    return pair != NULL ? pair->rule : NULL;
}

const Constraint *standard_tonality_major_scale_constraint(StandardTonalityMajorScaleRule which) {
    const RulePair *pair = valid_rule(which) ? rule_pair_table_get(&g_table, (size_t)which) : NULL;
    return pair != NULL ? pair->constraint : NULL;
}

static const int STEPS[STANDARD_TONALITY_MAJOR_SCALE_STEP_COUNT] = {2, 2, 1, 2, 2, 2, 1};

int standard_tonality_major_scale_step(size_t index) {
    return STEPS[index];
}

bool standard_tonality_is_major_scale(const PitchSpelling *const *notes, size_t count) {
    return standard_tonality_is_scale_pattern(STEPS, STANDARD_TONALITY_MAJOR_SCALE_STEP_COUNT, notes, count);
}

bool standard_tonality_scale_closes_on_tonic(const PitchSpelling *first, const PitchSpelling *last) {
    if (first == NULL || last == NULL) {
        return false;
    }
    return pitch_spelling_letter(first) == pitch_spelling_letter(last) &&
           pitch_spelling_accidental(first) == pitch_spelling_accidental(last) &&
           pitch_spelling_octave(last) == pitch_spelling_octave(first) + 1;
}

bool standard_tonality_same_scale_name(const PitchSpelling *a, const PitchSpelling *b) {
    if (a == NULL || b == NULL) {
        return false;
    }
    return pitch_spelling_letter(a) == pitch_spelling_letter(b) &&
           pitch_spelling_accidental(a) == pitch_spelling_accidental(b);
}

bool standard_tonality_build_major_scale(const PitchSpelling *tonic,
                                         PitchSpelling *out[STANDARD_TONALITY_MAJOR_SCALE_NOTE_COUNT]) {
    return standard_tonality_build_scale_pattern(STEPS, STANDARD_TONALITY_MAJOR_SCALE_STEP_COUNT, tonic, out);
}
