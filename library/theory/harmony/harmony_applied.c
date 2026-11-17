#include "theory/harmony/harmony_applied.h"

#include <stdio.h>
#include <string.h>

#include "model/pitch/midi_pitch.h"
#include "theory/tonality/roman_numeral_grammar.h"
#include "theory/tonality/tonal_key.h"
#include "theory/tonality/triad_in_key_rules.h"

/* The applied chord's own numeral, by quality: degree 5 for a dominant, 7 for a leading-tone chord. */
static const struct {
    int degree;
    StandardTonalityNumeralQuality quality;
} QUALITY_NUMERALS[HARMONY_APPLIED_QUALITY_COUNT] = {
    {5, STANDARD_TONALITY_NUMERAL_MAJOR},
    {5, STANDARD_TONALITY_NUMERAL_DOMINANT_SEVENTH},
    {7, STANDARD_TONALITY_NUMERAL_DIMINISHED},
    {7, STANDARD_TONALITY_NUMERAL_FULLY_DIMINISHED_SEVENTH},
    {7, STANDARD_TONALITY_NUMERAL_HALF_DIMINISHED_SEVENTH},
};

bool harmony_applied_quality_valid(HarmonyAppliedQuality quality) {
    return quality >= 0 && quality < HARMONY_APPLIED_QUALITY_COUNT;
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

bool harmony_applied_label_inverted(HarmonyMode mode, const HarmonyAppliedChord *chord, int inversion, char *out,
                                    size_t size) {
    if (out == NULL || !harmony_applied_valid(mode, chord) || inversion < 0 ||
        inversion > (harmony_applied_quality_is_seventh(chord->quality) ? 3 : 1)) {
        return false;
    }
    /* The target is a triad of its key: the natural scale's, but with the major dominant of the harmonic minor. */
    StandardTonalityTriadKey key = mode == HARMONY_MODE_MAJOR ? STANDARD_TONALITY_TRIAD_KEY_MAJOR
                                   : chord->target == 5       ? STANDARD_TONALITY_TRIAD_KEY_MINOR_HARMONIC
                                                              : STANDARD_TONALITY_TRIAD_KEY_MINOR_NATURAL;
    StandardTonalityTriadQuality target_quality;
    if (!standard_tonality_triad_quality_in_key(key, chord->target, &target_quality)) {
        return false;
    }
    StandardTonalityNumeralText text;
    memset(&text, 0, sizeof(text));
    text.degree = QUALITY_NUMERALS[chord->quality].degree;
    text.quality = QUALITY_NUMERALS[chord->quality].quality;
    text.inversion = inversion;
    text.has_target = true;
    text.target_degree = chord->target;
    text.target_minor = target_quality == STANDARD_TONALITY_TRIAD_MINOR;
    return standard_tonality_numeral_format_styled(&text, STANDARD_TONALITY_NUMERAL_STYLE_PLAN, out, size);
}

bool harmony_applied_label(HarmonyMode mode, const HarmonyAppliedChord *chord, char *out, size_t size) {
    return harmony_applied_label_inverted(mode, chord, 0, out, size);
}

int harmony_applied_scale_semitone(HarmonyMode mode, int degree) {
    if (!harmony_mode_valid(mode) || degree < 1 || degree > 7) {
        return -1;
    }
    /* The major scale is the natural notes C D E F G A B; the minor key is its natural minor, the third, sixth and seventh a semitone down. */
    int lowered = mode == HARMONY_MODE_MINOR && (degree == 3 || degree == 6 || degree == 7) ? 1 : 0;
    return standard_tonality_natural_degree_semitone(degree) - lowered;
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

