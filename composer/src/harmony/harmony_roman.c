#include "harmony/harmony_internal.h"

#include <string.h>

#include <standard-tonality/roman_numeral_grammar.h>

HarmonyMode harmony_mode_of(PlanKeyMode mode) {
    return mode == PLAN_MODE_MINOR ? HARMONY_MODE_MINOR : HARMONY_MODE_MAJOR;
}

/*
 * A chord as the library's numeral value (R-232). Case comes from the quality: the chords of the mode's scale that have
 * a minor third are lower case, a diminished chord and the leading-tone chord of a minor key are diminished (a seventh on
 * the diminished chord of a major key or on the second degree of a minor key is half-diminished, on the raised seventh
 * fully diminished).
 */
static StandardTonalityNumeralText numeral_of(PlanKeyMode mode, const HarmonyChord *chord) {
    static const bool MAJOR_MINOR_THIRD[8] = {false, false, true, true, false, false, true, false};
    static const bool MINOR_MINOR_THIRD[8] = {false, true, false, false, true, false, false, false};
    StandardTonalityNumeralText numeral;
    memset(&numeral, 0, sizeof(numeral));
    numeral.degree = chord->degree;
    numeral.inversion = chord->inversion;
    bool minor_mode = mode == PLAN_MODE_MINOR;
    bool diminished = minor_mode ? (chord->degree == 2 || chord->leading_tone) : chord->degree == 7;
    bool minor_third = minor_mode ? MINOR_MINOR_THIRD[chord->degree] : MAJOR_MINOR_THIRD[chord->degree];
    if (diminished) {
        numeral.quality = !chord->seventh ? STANDARD_TONALITY_NUMERAL_DIMINISHED
                          : chord->leading_tone ? STANDARD_TONALITY_NUMERAL_FULLY_DIMINISHED_SEVENTH
                                                : STANDARD_TONALITY_NUMERAL_HALF_DIMINISHED_SEVENTH;
    } else if (chord->seventh) {
        numeral.quality = minor_third ? STANDARD_TONALITY_NUMERAL_MINOR_SEVENTH
                                      : STANDARD_TONALITY_NUMERAL_DOMINANT_SEVENTH;
    } else {
        numeral.quality = minor_third ? STANDARD_TONALITY_NUMERAL_MINOR : STANDARD_TONALITY_NUMERAL_MAJOR;
    }
    return numeral;
}

bool harmony_roman_format(PlanKeyMode mode, const HarmonyChord *chord, char out[PLAN_ROMAN_NUMERAL_MAX]) {
    if (!chord || !out || !harmony_chord_valid(chord) || chord->half_diminished) {
        return false;
    }
    StandardTonalityNumeralText numeral = numeral_of(mode, chord);
    char text[PLAN_ROMAN_NUMERAL_MAX];
    if (!standard_tonality_numeral_format(&numeral, text, sizeof(text))) {
        return false;
    }
    memset(out, 0, PLAN_ROMAN_NUMERAL_MAX);
    memcpy(out, text, strlen(text));
    return true;
}

static bool quality_is_diminished_seventh(StandardTonalityNumeralQuality quality) {
    return quality == STANDARD_TONALITY_NUMERAL_HALF_DIMINISHED_SEVENTH ||
           quality == STANDARD_TONALITY_NUMERAL_FULLY_DIMINISHED_SEVENTH;
}

bool harmony_roman_parse(PlanKeyMode mode, const char *roman, HarmonyChord *out) {
    StandardTonalityNumeralText numeral;
    if (!roman || !out || !standard_tonality_numeral_parse(roman, &numeral) || numeral.alteration != 0 ||
        numeral.has_target) {
        return false;
    }
    HarmonyChord chord;
    memset(&chord, 0, sizeof(chord));
    chord.degree = numeral.degree;
    chord.seventh = standard_tonality_numeral_quality_is_seventh(numeral.quality);
    chord.inversion = numeral.inversion;
    /* The diminished chord on 7 of a minor key is the one on the raised seventh degree. */
    chord.leading_tone = mode == PLAN_MODE_MINOR && numeral.degree == 7 &&
                         (numeral.quality == STANDARD_TONALITY_NUMERAL_DIMINISHED ||
                          quality_is_diminished_seventh(numeral.quality));
    if (!harmony_chord_valid(&chord)) {
        return false;
    }
    /* Only the numerals of this mode: the same chord must be written the same way (either diminished seventh reads). */
    StandardTonalityNumeralText expected = numeral_of(mode, &chord);
    if (expected.quality != numeral.quality &&
        !(quality_is_diminished_seventh(expected.quality) && quality_is_diminished_seventh(numeral.quality))) {
        return false;
    }
    *out = chord;
    return true;
}

bool harmony_roman_format_applied_inverted(PlanKeyMode mode, const HarmonyAppliedChord *applied, int inversion,
                                           char out[PLAN_ROMAN_NUMERAL_MAX]) {
    static const char *const SEVENTH_FIGURES[3] = {"6/5", "4/3", "4/2"};
    char label[PLAN_ROMAN_NUMERAL_MAX];
    if (!applied || !out || inversion < 0 || inversion > 3 ||
        !harmony_applied_label(harmony_mode_of(mode), applied, label, sizeof(label))) {
        return false;
    }
    bool seventh = harmony_applied_quality_is_seventh(applied->quality);
    /* A triad is written in root position or first inversion, a seventh chord in any. */
    if (inversion > (seventh ? 3 : 1)) {
        return false;
    }
    memset(out, 0, PLAN_ROMAN_NUMERAL_MAX);
    if (inversion == 0) {
        memcpy(out, label, strlen(label));
        return true;
    }
    /* The figures go between the chord and its target: "V7/V" becomes "V6/5/V". */
    const char *slash = strrchr(label, '/');
    const char *figure = seventh ? SEVENTH_FIGURES[inversion - 1] : "6";
    size_t head = (size_t)(slash - label) - (seventh ? 1 : 0);
    size_t tail = strlen(slash);
    if (head + strlen(figure) + tail >= PLAN_ROMAN_NUMERAL_MAX) {
        return false;
    }
    memcpy(out, label, head);
    memcpy(out + head, figure, strlen(figure));
    memcpy(out + head + strlen(figure), slash, tail);
    return true;
}

bool harmony_roman_parse_applied_inverted(PlanKeyMode mode, const char *roman, HarmonyAppliedChord *out,
                                          int *inversion) {
    if (!roman || !out || !inversion) {
        return false;
    }
    /* The labels are the framework's, so each valid one is generated and compared. */
    for (int quality = 0; quality < HARMONY_APPLIED_QUALITY_COUNT; quality++) {
        for (int target = 1; target <= 7; target++) {
            for (int position = 0; position <= 3; position++) {
                HarmonyAppliedChord applied;
                char label[PLAN_ROMAN_NUMERAL_MAX];
                memset(&applied, 0, sizeof(applied));
                applied.quality = (HarmonyAppliedQuality)quality;
                applied.target = target;
                if (!harmony_applied_quality_kind(applied.quality, &applied.kind) ||
                    !harmony_applied_valid(harmony_mode_of(mode), &applied) ||
                    !harmony_roman_format_applied_inverted(mode, &applied, position, label)) {
                    continue;
                }
                if (strcmp(label, roman) == 0) {
                    *out = applied;
                    *inversion = position;
                    return true;
                }
            }
        }
    }
    return false;
}

bool harmony_roman_format_applied(PlanKeyMode mode, const HarmonyAppliedChord *applied,
                                  char out[PLAN_ROMAN_NUMERAL_MAX]) {
    return harmony_roman_format_applied_inverted(mode, applied, 0, out);
}

bool harmony_roman_parse_applied(PlanKeyMode mode, const char *roman, HarmonyAppliedChord *out) {
    int inversion;
    return harmony_roman_parse_applied_inverted(mode, roman, out, &inversion) && inversion == 0;
}
