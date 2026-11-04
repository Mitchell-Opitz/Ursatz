#include "harmony_applied.h"

#include <stdio.h>
#include "pitch/midi_pitch.h"

static const char *const QUALITY_NAMES[HARMONY_APPLIED_QUALITY_COUNT] = {
    "major-triad", "dominant-seventh", "diminished-triad", "fully-diminished-seventh", "half-diminished-seventh"};

/* The label of the applied chord itself, by quality. */
static const char *const QUALITY_SYMBOLS[HARMONY_APPLIED_QUALITY_COUNT] = {"V", "V7", "viio", "viio7", "vii%7"};

/* Roman numerals of the diatonic triads, indexed by degree. */
static const char *const MAJOR_NUMERALS[8] = {"", "I", "ii", "iii", "IV", "V", "vi", "viio"};
static const char *const MINOR_NUMERALS[8] = {"", "i", "iio", "III", "iv", "V", "VI", "VII"};

bool harmony_applied_quality_valid(HarmonyAppliedQuality quality) {
    return quality >= 0 && quality < HARMONY_APPLIED_QUALITY_COUNT;
}

const char *harmony_applied_quality_name(HarmonyAppliedQuality quality) {
    return harmony_applied_quality_valid(quality) ? QUALITY_NAMES[quality] : NULL;
}

bool harmony_applied_quality_is_seventh(HarmonyAppliedQuality quality) {
    return quality == HARMONY_APPLIED_DOMINANT_SEVENTH || quality == HARMONY_APPLIED_FULLY_DIMINISHED_SEVENTH ||
           quality == HARMONY_APPLIED_HALF_DIMINISHED_SEVENTH;
}

bool harmony_applied_quality_kind(HarmonyAppliedQuality quality, AppliedChordKind *out) {
    if (out == NULL || !harmony_applied_quality_valid(quality)) {
        return false;
    }
    *out = quality <= HARMONY_APPLIED_DOMINANT_SEVENTH ? APPLIED_CHORD_KIND_SECONDARY_DOMINANT
                                                       : APPLIED_CHORD_KIND_SECONDARY_LEADING_TONE;
    return true;
}

bool harmony_applied_target_valid(HarmonyMode mode, int degree) {
    switch (mode) {
        case HARMONY_MODE_MAJOR:
            return degree >= 2 && degree <= 6;
        case HARMONY_MODE_MINOR:
            return degree >= 3 && degree <= 7;
        default:
            return false;
    }
}

bool harmony_applied_valid(HarmonyMode mode, const HarmonyAppliedChord *chord) {
    if (chord == NULL || !harmony_mode_valid(mode) || !applied_chord_kind_valid(chord->kind)) {
        return false;
    }
    AppliedChordKind kind;
    return harmony_applied_quality_kind(chord->quality, &kind) && kind == chord->kind &&
           harmony_applied_target_valid(mode, chord->target);
}

bool harmony_applied_function(HarmonyMode mode, const HarmonyAppliedChord *chord, HarmonicFunction *out) {
    if (out == NULL || !harmony_applied_valid(mode, chord)) {
        return false;
    }
    *out = HARMONIC_FUNCTION_DOMINANT;
    return true;
}

bool harmony_applied_label(HarmonyMode mode, const HarmonyAppliedChord *chord, char *out, size_t size) {
    if (out == NULL || !harmony_applied_valid(mode, chord)) {
        return false;
    }
    const char *target = mode == HARMONY_MODE_MAJOR ? MAJOR_NUMERALS[chord->target] : MINOR_NUMERALS[chord->target];
    char label[32];
    int length = snprintf(label, sizeof(label), "%s/%s", QUALITY_SYMBOLS[chord->quality], target);
    if (length < 0 || (size_t)length >= size) {
        return false;
    }
    snprintf(out, size, "%s", label);
    return true;
}

static const int MAJOR_SEMITONES[7] = {0, 2, 4, 5, 7, 9, 11};
static const int MINOR_SEMITONES[7] = {0, 2, 3, 5, 7, 8, 10};

int harmony_applied_scale_semitone(HarmonyMode mode, int degree) {
    if (!harmony_mode_valid(mode) || degree < 1 || degree > 7) {
        return -1;
    }
    return mode == HARMONY_MODE_MAJOR ? MAJOR_SEMITONES[degree - 1] : MINOR_SEMITONES[degree - 1];
}

bool harmony_applied_analyze(HarmonyMode mode, int root_semitone, HarmonyAppliedQuality quality,
                             HarmonyAppliedChord *out) {
    AppliedChordKind kind;
    if (out == NULL || !harmony_mode_valid(mode) || !harmony_applied_quality_kind(quality, &kind)) {
        return false;
    }
    int root = pitch_class_of_midi(root_semitone);
    int target_root = kind == APPLIED_CHORD_KIND_SECONDARY_DOMINANT ? (root + 5) % 12 : (root + 1) % 12;
    for (int degree = 1; degree <= 7; degree++) {
        if (harmony_applied_scale_semitone(mode, degree) == target_root) {
            if (!harmony_applied_target_valid(mode, degree)) {
                return false;
            }
            out->kind = kind;
            out->quality = quality;
            out->target = degree;
            return true;
        }
    }
    return false;
}

int harmony_applied_root_degree(HarmonyMode mode, const HarmonyAppliedChord *chord) {
    if (!harmony_applied_valid(mode, chord)) {
        return -1;
    }
    int up = chord->kind == APPLIED_CHORD_KIND_SECONDARY_DOMINANT ? 4 : 6;
    return (chord->target - 1 + up) % 7 + 1;
}

int harmony_applied_root_semitone(HarmonyMode mode, const HarmonyAppliedChord *chord) {
    if (!harmony_applied_valid(mode, chord)) {
        return -1;
    }
    int target_root = harmony_applied_scale_semitone(mode, chord->target);
    return chord->kind == APPLIED_CHORD_KIND_SECONDARY_DOMINANT ? (target_root + 7) % 12 : (target_root + 11) % 12;
}

bool harmony_applied_from_diatonic_root(HarmonyMode mode, int degree, HarmonyAppliedChord *out) {
    if (out == NULL || !harmony_mode_valid(mode) || degree < 1 || degree > 7) {
        return false;
    }
    int target = (degree - 1 + 3) % 7 + 1;
    if (!harmony_applied_target_valid(mode, target)) {
        return false;
    }
    out->kind = APPLIED_CHORD_KIND_SECONDARY_DOMINANT;
    out->quality = HARMONY_APPLIED_DOMINANT_SEVENTH;
    out->target = target;
    return true;
}
