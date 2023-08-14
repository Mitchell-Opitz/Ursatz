#include "harmony/harmony_internal.h"

#include <string.h>

#define DIMINISHED "\xC2\xB0"

static const char *const MAJOR_NUMERALS[8] = {NULL, "I", "ii", "iii", "IV", "V", "vi", "vii" DIMINISHED};
static const char *const MINOR_NUMERALS[8] = {NULL, "i", "ii" DIMINISHED, "III", "iv", "V", "VI", "VII"};
/* The chord on the raised seventh degree of a minor key. */
static const char *const MINOR_LEADING_TONE = "vii" DIMINISHED;

/* The figures after a numeral: triads in three positions, sevenths in four. */
typedef struct {
    const char *text;
    bool seventh;
    int inversion;
} Figure;

static const Figure FIGURES[] = {{"", false, 0},    {"6", false, 1},    {"6/4", false, 2}, {"7", true, 0},
                                 {"6/5", true, 1}, {"4/3", true, 2}, {"4/2", true, 3}};

HarmonyMode harmony_mode_of(PlanKeyMode mode) {
    return mode == PLAN_MODE_MINOR ? HARMONY_MODE_MINOR : HARMONY_MODE_MAJOR;
}

bool harmony_roman_format(PlanKeyMode mode, const HarmonyChord *chord, char out[PLAN_ROMAN_NUMERAL_MAX]) {
    if (!chord || !out || !harmony_chord_valid(chord) || chord->half_diminished) {
        return false;
    }
    const char *base = NULL;
    if (mode == PLAN_MODE_MINOR) {
        base = chord->leading_tone ? MINOR_LEADING_TONE : MINOR_NUMERALS[chord->degree];
    } else {
        base = MAJOR_NUMERALS[chord->degree];
    }
    for (size_t i = 0; i < sizeof(FIGURES) / sizeof(FIGURES[0]); i++) {
        if (FIGURES[i].seventh != chord->seventh || FIGURES[i].inversion != chord->inversion) {
            continue;
        }
        size_t base_length = strlen(base);
        size_t figure_length = strlen(FIGURES[i].text);
        if (base_length + figure_length >= PLAN_ROMAN_NUMERAL_MAX) {
            return false;
        }
        memset(out, 0, PLAN_ROMAN_NUMERAL_MAX);
        memcpy(out, base, base_length);
        memcpy(out + base_length, FIGURES[i].text, figure_length);
        return true;
    }
    return false;
}

/* Whether text is exactly numeral followed by a figure; sets *figure to it. */
static bool numeral_with_figure(const char *text, const char *numeral, const Figure **figure) {
    size_t length = strlen(numeral);
    if (strncmp(text, numeral, length) != 0) {
        return false;
    }
    for (size_t i = 0; i < sizeof(FIGURES) / sizeof(FIGURES[0]); i++) {
        if (strcmp(text + length, FIGURES[i].text) == 0) {
            *figure = &FIGURES[i];
            return true;
        }
    }
    return false;
}

bool harmony_roman_parse(PlanKeyMode mode, const char *roman, HarmonyChord *out) {
    if (!roman || !out) {
        return false;
    }
    const char *const *numerals = mode == PLAN_MODE_MINOR ? MINOR_NUMERALS : MAJOR_NUMERALS;
    const Figure *figure = NULL;
    HarmonyChord chord;
    memset(&chord, 0, sizeof(chord));
    for (int degree = 1; degree <= 7 && !figure; degree++) {
        if (numeral_with_figure(roman, numerals[degree], &figure)) {
            chord.degree = degree;
        }
    }
    if (!figure && mode == PLAN_MODE_MINOR && numeral_with_figure(roman, MINOR_LEADING_TONE, &figure)) {
        chord.degree = 7;
        chord.leading_tone = true;
    }
    if (!figure) {
        return false;
    }
    chord.seventh = figure->seventh;
    chord.inversion = figure->inversion;
    if (!harmony_chord_valid(&chord)) {
        return false;
    }
    *out = chord;
    return true;
}

bool harmony_roman_format_applied(PlanKeyMode mode, const HarmonyAppliedChord *applied,
                                  char out[PLAN_ROMAN_NUMERAL_MAX]) {
    char label[PLAN_ROMAN_NUMERAL_MAX];
    if (!applied || !out || !harmony_applied_label(harmony_mode_of(mode), applied, label, sizeof(label))) {
        return false;
    }
    memset(out, 0, PLAN_ROMAN_NUMERAL_MAX);
    memcpy(out, label, strlen(label));
    return true;
}

bool harmony_roman_parse_applied(PlanKeyMode mode, const char *roman, HarmonyAppliedChord *out) {
    if (!roman || !out) {
        return false;
    }
    /* The labels are the framework's, so each valid one is generated and compared. */
    for (int quality = 0; quality < HARMONY_APPLIED_QUALITY_COUNT; quality++) {
        for (int target = 1; target <= 7; target++) {
            HarmonyAppliedChord applied;
            char label[PLAN_ROMAN_NUMERAL_MAX];
            memset(&applied, 0, sizeof(applied));
            applied.quality = (HarmonyAppliedQuality)quality;
            applied.target = target;
            if (!harmony_applied_quality_kind(applied.quality, &applied.kind) ||
                !harmony_applied_valid(harmony_mode_of(mode), &applied) ||
                !harmony_applied_label(harmony_mode_of(mode), &applied, label, sizeof(label))) {
                continue;
            }
            if (strcmp(label, roman) == 0) {
                *out = applied;
                return true;
            }
        }
    }
    return false;
}
