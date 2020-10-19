#include "major_scale_rules.h"

#include <stddef.h>

#include "rule_pair.h"
#include "tonal_pitch.h"

static const char *const NAMES[STANDARD_TONALITY_MAJOR_SCALE_RULE_COUNT] = {"major-scale-step-pattern",
                                                                             "major-scale-named-by-tonic"};

static StandardTonalityRulePair g_pairs[STANDARD_TONALITY_MAJOR_SCALE_RULE_COUNT];
static bool g_built[STANDARD_TONALITY_MAJOR_SCALE_RULE_COUNT];

static bool valid_rule(StandardTonalityMajorScaleRule which) {
    return which >= 0 && which < STANDARD_TONALITY_MAJOR_SCALE_RULE_COUNT;
}

static void ensure_built(StandardTonalityMajorScaleRule which) {
    if (!valid_rule(which) || g_built[which]) {
        return;
    }
    g_built[which] = true;
    standard_tonality_build_rule_pair(NAMES[which], &g_pairs[which]);
}

const Rule *standard_tonality_major_scale_rule(StandardTonalityMajorScaleRule which) {
    ensure_built(which);
    return valid_rule(which) ? g_pairs[which].rule : NULL;
}

const Constraint *standard_tonality_major_scale_constraint(StandardTonalityMajorScaleRule which) {
    ensure_built(which);
    return valid_rule(which) ? g_pairs[which].constraint : NULL;
}

static const int STEPS[STANDARD_TONALITY_MAJOR_SCALE_STEP_COUNT] = {2, 2, 1, 2, 2, 2, 1};

int standard_tonality_major_scale_step(size_t index) {
    return STEPS[index];
}

bool standard_tonality_is_major_scale(const PitchSpelling *const *notes, size_t count) {
    if (notes == NULL || count != STANDARD_TONALITY_MAJOR_SCALE_NOTE_COUNT) {
        return false;
    }
    for (size_t i = 0; i < count; i++) {
        if (notes[i] == NULL) {
            return false;
        }
    }

    PitchLetter letter = pitch_spelling_letter(notes[0]);
    for (size_t i = 0; i < STANDARD_TONALITY_MAJOR_SCALE_STEP_COUNT; i++) {
        letter = standard_tonality_next_letter(letter);
        if (pitch_spelling_letter(notes[i + 1]) != letter) {
            return false;
        }
        int distance = standard_tonality_semitone_number(notes[i + 1]) - standard_tonality_semitone_number(notes[i]);
        if (distance != STEPS[i]) {
            return false;
        }
    }
    return true;
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
    if (tonic == NULL) {
        return false;
    }

    PitchSpelling *built[STANDARD_TONALITY_MAJOR_SCALE_NOTE_COUNT];
    PitchLetter letter = pitch_spelling_letter(tonic);
    int letter_octave = pitch_spelling_octave(tonic);
    int target = standard_tonality_semitone_number(tonic);
    size_t created = 0;

    for (size_t i = 0; i < STANDARD_TONALITY_MAJOR_SCALE_NOTE_COUNT; i++) {
        if (i > 0) {
            PitchLetter next = standard_tonality_next_letter(letter);
            if (next == PITCH_LETTER_C) {
                letter_octave++;
            }
            letter = next;
            target += STEPS[i - 1];
        }
        int natural = STANDARD_TONALITY_SEMITONES_PER_OCTAVE * letter_octave + standard_tonality_letter_semitone(letter);
        if (pitch_spelling_create(letter, target - natural, letter_octave, &built[i]) != PITCH_SPELLING_OK) {
            goto fail;
        }
        created++;
    }

    for (size_t i = 0; i < STANDARD_TONALITY_MAJOR_SCALE_NOTE_COUNT; i++) {
        out[i] = built[i];
    }
    return true;

fail:
    for (size_t i = 0; i < created; i++) {
        pitch_spelling_destroy(built[i]);
    }
    return false;
}
