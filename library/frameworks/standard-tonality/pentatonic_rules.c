#include "pentatonic_rules.h"

#include <stddef.h>

#include "major_scale_rules.h"
#include "minor_scale_rules.h"
#include "rule_pair.h"
#include "tonal_pitch.h"

static const char *const NAMES[STANDARD_TONALITY_PENTATONIC_RULE_COUNT] = {
    "pentatonic-intervals", "pentatonic-degrees",   "pentatonic-solfege",
    "pentatonic-spelling",  "pentatonic-black-keys", "pentatonic-minor-rotation"};

static RulePairTable g_table = RULE_PAIR_TABLE_INIT(standard_tonality_framework, "standard-tonality",
    NAMES, STANDARD_TONALITY_PENTATONIC_RULE_COUNT);

URSATZ_RULE_ACCESSORS(standard_tonality_pentatonic, StandardTonalityPentatonicRule, STANDARD_TONALITY_PENTATONIC_RULE_COUNT, g_table)

static bool valid_form(StandardTonalityPentatonicForm form) {
    return form >= 0 && form < STANDARD_TONALITY_PENTATONIC_FORM_COUNT;
}

static const int STEPS[STANDARD_TONALITY_PENTATONIC_FORM_COUNT][STANDARD_TONALITY_PENTATONIC_STEP_COUNT] = {
    {2, 2, 3, 2, 3},
    {3, 2, 2, 3, 2},
};

/* Degrees in the matching scale; the sixth entry (the octave tonic) is
   degree 8. */
static const int DEGREES[STANDARD_TONALITY_PENTATONIC_FORM_COUNT][STANDARD_TONALITY_PENTATONIC_NOTE_COUNT] = {
    {1, 2, 3, 5, 6, 8},
    {1, 3, 4, 5, 7, 8},
};

static const char *const SOLFEGE[STANDARD_TONALITY_PENTATONIC_FORM_COUNT][STANDARD_TONALITY_PENTATONIC_NOTE_COUNT] = {
    {"do", "re", "mi", "sol", "la", "do"},
    {"do", "me", "fa", "sol", "te", "do"},
};

int standard_tonality_pentatonic_step(StandardTonalityPentatonicForm form, size_t index) {
    return STEPS[form][index];
}

int standard_tonality_pentatonic_degree(StandardTonalityPentatonicForm form, size_t index) {
    return DEGREES[form][index];
}

const char *standard_tonality_pentatonic_solfege(StandardTonalityPentatonicForm form, size_t index) {
    if (!valid_form(form) || index >= STANDARD_TONALITY_PENTATONIC_NOTE_COUNT) {
        return NULL;
    }
    return SOLFEGE[form][index];
}

/* Letters between two scale degrees: degree d is d - 1 letters above
   the tonic. */
static int letter_advance(PitchLetter from, PitchLetter to) {
    int advance = 0;
    PitchLetter letter = from;
    while (letter != to) {
        letter = standard_tonality_next_letter(letter);
        advance++;
    }
    return advance;
}

bool standard_tonality_is_pentatonic(StandardTonalityPentatonicForm form, const PitchSpelling *const *notes,
                                     size_t count) {
    if (!valid_form(form) || notes == NULL || count != STANDARD_TONALITY_PENTATONIC_NOTE_COUNT) {
        return false;
    }
    for (size_t i = 0; i < count; i++) {
        if (notes[i] == NULL) {
            return false;
        }
    }
    for (size_t i = 0; i < STANDARD_TONALITY_PENTATONIC_STEP_COUNT; i++) {
        int distance = standard_tonality_semitone_number(notes[i + 1]) - standard_tonality_semitone_number(notes[i]);
        if (distance != STEPS[form][i]) {
            return false;
        }
        int expected_advance = DEGREES[form][i + 1] - DEGREES[form][i];
        if (letter_advance(pitch_spelling_letter(notes[i]), pitch_spelling_letter(notes[i + 1])) != expected_advance) {
            return false;
        }
    }
    return true;
}

bool standard_tonality_build_pentatonic(StandardTonalityPentatonicForm form, const PitchSpelling *tonic,
                                        PitchSpelling *out[STANDARD_TONALITY_PENTATONIC_NOTE_COUNT]) {
    if (!valid_form(form) || out == NULL) {
        return false;
    }

    PitchSpelling *scale[8];
    bool built = form == STANDARD_TONALITY_PENTATONIC_MAJOR
                     ? standard_tonality_build_major_scale(tonic, scale)
                     : standard_tonality_build_minor_scale(STANDARD_TONALITY_MINOR_NATURAL, tonic, scale);
    if (!built) {
        return false;
    }

    bool keep[8] = {false};
    for (size_t i = 0; i < STANDARD_TONALITY_PENTATONIC_NOTE_COUNT; i++) {
        size_t index = (size_t)DEGREES[form][i] - 1;
        keep[index] = true;
        out[i] = scale[index];
    }
    for (size_t i = 0; i < 8; i++) {
        if (!keep[i]) {
            pitch_spelling_destroy(scale[i]);
        }
    }
    return true;
}

bool standard_tonality_all_black_keys(const PitchSpelling *const *notes, size_t count) {
    if (notes == NULL || count == 0) {
        return false;
    }
    for (size_t i = 0; i < count; i++) {
        if (notes[i] == NULL) {
            return false;
        }
        int pitch_class = standard_tonality_semitone_number(notes[i]) % STANDARD_TONALITY_SEMITONES_PER_OCTAVE;
        if (pitch_class < 0) {
            pitch_class += STANDARD_TONALITY_SEMITONES_PER_OCTAVE;
        }
        if (pitch_class != 1 && pitch_class != 3 && pitch_class != 6 && pitch_class != 8 && pitch_class != 10) {
            return false;
        }
    }
    return true;
}
