#include "minor_scale_degree_rules.h"

#include <stdio.h>

#include "rule_pair.h"
#include "scale_degree_rules.h"
#include "tonal_pitch.h"

static const char *const NAMES[STANDARD_TONALITY_MINOR_DEGREE_RULE_COUNT] = {
    "minor-scale-degree-solfege", "minor-scale-degree-names", "minor-scale-degree-subtonic",
    "minor-scale-degree-lowered"};

static StandardTonalityRulePair g_pairs[STANDARD_TONALITY_MINOR_DEGREE_RULE_COUNT];
static bool g_built[STANDARD_TONALITY_MINOR_DEGREE_RULE_COUNT];

static bool valid_rule(StandardTonalityMinorDegreeRule which) {
    return which >= 0 && which < STANDARD_TONALITY_MINOR_DEGREE_RULE_COUNT;
}

static void ensure_built(StandardTonalityMinorDegreeRule which) {
    if (!valid_rule(which) || g_built[which]) {
        return;
    }
    g_built[which] = true;
    standard_tonality_build_rule_pair(NAMES[which], &g_pairs[which]);
}

const Rule *standard_tonality_minor_degree_rule(StandardTonalityMinorDegreeRule which) {
    ensure_built(which);
    return valid_rule(which) ? g_pairs[which].rule : NULL;
}

const Constraint *standard_tonality_minor_degree_constraint(StandardTonalityMinorDegreeRule which) {
    ensure_built(which);
    return valid_rule(which) ? g_pairs[which].constraint : NULL;
}

static bool valid_form(StandardTonalityMinorForm form) {
    return form >= 0 && form < STANDARD_TONALITY_MINOR_FORM_COUNT;
}

static bool valid_degree(int degree) {
    return degree >= 1 && degree <= STANDARD_TONALITY_SCALE_DEGREE_CLOSING_TONIC;
}

static int degree_index(int degree) {
    return degree == STANDARD_TONALITY_SCALE_DEGREE_CLOSING_TONIC ? 0 : degree - 1;
}

static const char *const SOLFEGE[STANDARD_TONALITY_MINOR_FORM_COUNT][STANDARD_TONALITY_SCALE_DEGREE_COUNT] = {
    {"do", "re", "me", "fa", "sol", "le", "te"},
    {"do", "re", "me", "fa", "sol", "le", "ti"},
    {"do", "re", "me", "fa", "sol", "la", "ti"},
    {"do", "re", "me", "fa", "sol", "le", "te"},
};

const char *standard_tonality_minor_solfege(StandardTonalityMinorForm form, int degree) {
    if (!valid_form(form) || !valid_degree(degree)) {
        return NULL;
    }
    return SOLFEGE[form][degree_index(degree)];
}

const char *standard_tonality_minor_degree_name(StandardTonalityMinorForm form, int degree) {
    if (!valid_form(form) || !valid_degree(degree)) {
        return NULL;
    }
    if (degree == 7) {
        /* te is the subtonic; ti the leading tone. */
        return SOLFEGE[form][6][1] == 'e' ? "subtonic" : "leading tone";
    }
    return standard_tonality_degree_name(degree);
}

bool standard_tonality_is_subtonic(const PitchSpelling *tonic, const PitchSpelling *note) {
    if (tonic == NULL || note == NULL) {
        return false;
    }
    return standard_tonality_semitone_number(tonic) - standard_tonality_semitone_number(note) == 2;
}

static const int LOWERED[STANDARD_TONALITY_MINOR_FORM_COUNT][3] = {
    {3, 6, 7},
    {3, 6, 0},
    {3, 0, 0},
    {3, 6, 7},
};

size_t standard_tonality_minor_lowered_degrees(StandardTonalityMinorForm form, int out[3]) {
    if (!valid_form(form) || out == NULL) {
        return 0;
    }
    size_t count = 0;
    for (size_t i = 0; i < 3; i++) {
        if (LOWERED[form][i] != 0) {
            out[count++] = LOWERED[form][i];
        }
    }
    return count;
}

bool standard_tonality_minor_degree_is_lowered(StandardTonalityMinorForm form, int degree) {
    if (!valid_form(form)) {
        return false;
    }
    for (size_t i = 0; i < 3; i++) {
        if (LOWERED[form][i] == degree) {
            return true;
        }
    }
    return false;
}

int standard_tonality_minor_scale_degree_of(StandardTonalityMinorForm form, const PitchSpelling *tonic,
                                            const PitchSpelling *note) {
    if (!valid_form(form) || tonic == NULL || note == NULL) {
        return 0;
    }
    /* A descending scale is listed from the top; its degrees are
       natural minor's. */
    StandardTonalityMinorForm ascending =
        form == STANDARD_TONALITY_MINOR_MELODIC_DESCENDING ? STANDARD_TONALITY_MINOR_NATURAL : form;

    PitchSpelling *scale[STANDARD_TONALITY_MINOR_SCALE_NOTE_COUNT];
    if (!standard_tonality_build_minor_scale(ascending, tonic, scale)) {
        return 0;
    }

    int found = 0;
    for (int i = 0; i < STANDARD_TONALITY_SCALE_DEGREE_COUNT; i++) {
        if (pitch_spelling_letter(scale[i]) == pitch_spelling_letter(note) &&
            pitch_spelling_accidental(scale[i]) == pitch_spelling_accidental(note)) {
            found = i + 1;
            break;
        }
    }

    for (int i = 0; i < STANDARD_TONALITY_MINOR_SCALE_NOTE_COUNT; i++) {
        pitch_spelling_destroy(scale[i]);
    }
    return found;
}

ScaleDegreeResult standard_tonality_build_minor_scale_degree(StandardTonalityMinorForm form, int degree,
                                                             EntityID *scale_target_id, PitchIdentity *pitch,
                                                             ScaleDegree **out) {
    const char *label = standard_tonality_minor_degree_name(form, degree);
    if (label == NULL) {
        return SCALE_DEGREE_ERR_NULL_LABEL;
    }

    char interpretation_id_value[128];
    snprintf(interpretation_id_value, sizeof(interpretation_id_value), "standard-tonality-minor-scale-degree-%d-%d",
             (int)form, degree);

    return standard_tonality_build_labeled_scale_degree(label, interpretation_id_value, "minor-scale-degree-vocabulary",
                                                        scale_target_id, pitch, out);
}
