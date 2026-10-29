#include "minor_scale_rules.h"

#include <stddef.h>

#include "major_scale_rules.h"
#include "rule_pair.h"
#include "scale_pattern.h"
#include "tonal_pitch.h"

static const char *const NAMES[STANDARD_TONALITY_MINOR_SCALE_RULE_COUNT] = {
    "minor-scale-natural", "minor-scale-harmonic", "minor-scale-melodic", "minor-scale-third-lowered"};

static RulePairTable g_table = RULE_PAIR_TABLE_INIT(standard_tonality_framework, "standard-tonality",
    NAMES, STANDARD_TONALITY_MINOR_SCALE_RULE_COUNT);

static bool valid_rule(StandardTonalityMinorScaleRule which) {
    return which >= 0 && which < STANDARD_TONALITY_MINOR_SCALE_RULE_COUNT;
}

const Rule *standard_tonality_minor_scale_rule(StandardTonalityMinorScaleRule which) {
    const RulePair *pair = valid_rule(which) ? rule_pair_table_get(&g_table, (size_t)which) : NULL;
    return pair != NULL ? pair->rule : NULL;
}

const Constraint *standard_tonality_minor_scale_constraint(StandardTonalityMinorScaleRule which) {
    const RulePair *pair = valid_rule(which) ? rule_pair_table_get(&g_table, (size_t)which) : NULL;
    return pair != NULL ? pair->constraint : NULL;
}

/* Steps in playing order. The melodic descending drops W W H W W H W are
   the natural ascending steps reversed. */
static const int STEPS[STANDARD_TONALITY_MINOR_FORM_COUNT][STANDARD_TONALITY_MINOR_SCALE_STEP_COUNT] = {
    {2, 1, 2, 2, 1, 2, 2},
    {2, 1, 2, 2, 1, 3, 1},
    {2, 1, 2, 2, 2, 2, 1},
    {2, 2, 1, 2, 2, 1, 2},
};

static bool valid_form(StandardTonalityMinorForm form) {
    return form >= 0 && form < STANDARD_TONALITY_MINOR_FORM_COUNT;
}

static bool is_descending(StandardTonalityMinorForm form) {
    return form == STANDARD_TONALITY_MINOR_MELODIC_DESCENDING;
}

int standard_tonality_minor_scale_step(StandardTonalityMinorForm form, size_t index) {
    return STEPS[form][index];
}

bool standard_tonality_is_minor_scale(StandardTonalityMinorForm form, const PitchSpelling *const *notes,
                                      size_t count) {
    if (!valid_form(form) || notes == NULL || count != STANDARD_TONALITY_MINOR_SCALE_NOTE_COUNT) {
        return false;
    }
    if (!is_descending(form)) {
        return standard_tonality_is_scale_pattern(STEPS[form], STANDARD_TONALITY_MINOR_SCALE_STEP_COUNT, notes, count);
    }

    /* A descending scale is natural minor read from the top: reverse it
       and check the natural ascending pattern. */
    const PitchSpelling *reversed[STANDARD_TONALITY_MINOR_SCALE_NOTE_COUNT];
    for (size_t i = 0; i < count; i++) {
        reversed[i] = notes[count - 1 - i];
    }
    return standard_tonality_is_scale_pattern(STEPS[STANDARD_TONALITY_MINOR_NATURAL],
                                              STANDARD_TONALITY_MINOR_SCALE_STEP_COUNT, reversed, count);
}

bool standard_tonality_build_minor_scale(StandardTonalityMinorForm form, const PitchSpelling *tonic,
                                         PitchSpelling *out[STANDARD_TONALITY_MINOR_SCALE_NOTE_COUNT]) {
    if (!valid_form(form) || out == NULL) {
        return false;
    }

    PitchSpelling *built[STANDARD_TONALITY_MINOR_SCALE_NOTE_COUNT];
    StandardTonalityMinorForm ascending_form = is_descending(form) ? STANDARD_TONALITY_MINOR_NATURAL : form;
    if (!standard_tonality_build_scale_pattern(STEPS[ascending_form], STANDARD_TONALITY_MINOR_SCALE_STEP_COUNT, tonic,
                                               built)) {
        return false;
    }

    for (size_t i = 0; i < STANDARD_TONALITY_MINOR_SCALE_NOTE_COUNT; i++) {
        out[i] = is_descending(form) ? built[STANDARD_TONALITY_MINOR_SCALE_NOTE_COUNT - 1 - i] : built[i];
    }
    return true;
}

bool standard_tonality_minor_third_lowered(const PitchSpelling *tonic) {
    if (tonic == NULL) {
        return false;
    }

    PitchSpelling *major[STANDARD_TONALITY_MAJOR_SCALE_NOTE_COUNT];
    if (!standard_tonality_build_major_scale(tonic, major)) {
        return false;
    }
    PitchSpelling *minor[STANDARD_TONALITY_MINOR_SCALE_NOTE_COUNT];
    if (!standard_tonality_build_minor_scale(STANDARD_TONALITY_MINOR_NATURAL, tonic, minor)) {
        for (size_t i = 0; i < STANDARD_TONALITY_MAJOR_SCALE_NOTE_COUNT; i++) {
            pitch_spelling_destroy(major[i]);
        }
        return false;
    }

    bool lowered = pitch_spelling_letter(major[2]) == pitch_spelling_letter(minor[2]) &&
                   standard_tonality_semitone_number(major[2]) - standard_tonality_semitone_number(minor[2]) == 1;

    for (size_t i = 0; i < STANDARD_TONALITY_MAJOR_SCALE_NOTE_COUNT; i++) {
        pitch_spelling_destroy(major[i]);
        pitch_spelling_destroy(minor[i]);
    }
    return lowered;
}
