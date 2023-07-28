#include "harmony/harmony_internal.h"

#include <string.h>

#define DIMINISHED "\xC2\xB0"

static const char *const MAJOR_NUMERALS[8] = {NULL, "I", "ii", "iii", "IV", "V", "vi", "vii" DIMINISHED};
static const char *const MINOR_NUMERALS[8] = {NULL, "i", "ii" DIMINISHED, "III", "iv", "V", "VI", "VII"};
/* The chord on the raised seventh degree of a minor key. */
static const char *const MINOR_LEADING_TONE = "vii" DIMINISHED;

HarmonyMode harmony_mode_of(PlanKeyMode mode) {
    return mode == PLAN_MODE_MINOR ? HARMONY_MODE_MINOR : HARMONY_MODE_MAJOR;
}

bool harmony_roman_format(PlanKeyMode mode, const HarmonyChord *chord, char out[PLAN_ROMAN_NUMERAL_MAX]) {
    if (!chord || !out || !harmony_chord_valid(chord) || chord->inversion != 0 || chord->cadential) {
        return false;
    }
    const char *base = NULL;
    if (mode == PLAN_MODE_MINOR) {
        base = chord->leading_tone ? MINOR_LEADING_TONE : MINOR_NUMERALS[chord->degree];
    } else {
        base = MAJOR_NUMERALS[chord->degree];
    }
    size_t length = strlen(base);
    if (length + 2 > PLAN_ROMAN_NUMERAL_MAX) {
        return false;
    }
    memset(out, 0, PLAN_ROMAN_NUMERAL_MAX);
    memcpy(out, base, length);
    if (chord->seventh) {
        out[length] = '7';
    }
    return true;
}

bool harmony_roman_parse(PlanKeyMode mode, const char *roman, HarmonyChord *out) {
    if (!roman || !out) {
        return false;
    }
    size_t length = strlen(roman);
    bool seventh = length > 1 && roman[length - 1] == '7';
    if (seventh) {
        length--;
    }
    const char *const *numerals = mode == PLAN_MODE_MINOR ? MINOR_NUMERALS : MAJOR_NUMERALS;
    for (int degree = 1; degree <= 7; degree++) {
        if (strlen(numerals[degree]) == length && memcmp(numerals[degree], roman, length) == 0) {
            HarmonyChord chord;
            memset(&chord, 0, sizeof(chord));
            chord.degree = degree;
            chord.seventh = seventh;
            *out = chord;
            return harmony_chord_valid(&chord);
        }
    }
    if (mode == PLAN_MODE_MINOR && strlen(MINOR_LEADING_TONE) == length &&
        memcmp(MINOR_LEADING_TONE, roman, length) == 0) {
        HarmonyChord chord;
        memset(&chord, 0, sizeof(chord));
        chord.degree = 7;
        chord.leading_tone = true;
        chord.seventh = seventh;
        *out = chord;
        return harmony_chord_valid(&chord);
    }
    return false;
}
