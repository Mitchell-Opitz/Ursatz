#include "harmony_prolongation.h"

#include <stddef.h>

static const char *const NAMES[HARMONY_PROLONGATION_LINE_COUNT] = {"do-ti-do", "do-re-mi", "do-re-do", "do-fa-mi"};

static const int BASS[HARMONY_PROLONGATION_LINE_COUNT][3] = {{1, 7, 1}, {1, 2, 3}, {1, 2, 1}, {1, 4, 3}};

bool harmony_prolongation_line_valid(HarmonyProlongationLine line) {
    return line >= 0 && line < HARMONY_PROLONGATION_LINE_COUNT;
}

const char *harmony_prolongation_line_name(HarmonyProlongationLine line) {
    return harmony_prolongation_line_valid(line) ? NAMES[line] : NULL;
}

bool harmony_prolongation_line_bass(HarmonyProlongationLine line, int out[3]) {
    if (out == NULL || !harmony_prolongation_line_valid(line)) {
        return false;
    }
    for (int i = 0; i < 3; i++) {
        out[i] = BASS[line][i];
    }
    return true;
}

/* A tonic triad, root position (inversion 0) or first inversion (inversion 1). */
static bool is_tonic(const HarmonyChord *chord, int inversion) {
    return chord->degree == 1 && chord->inversion == inversion && !chord->seventh && !chord->cadential;
}

bool harmony_prolongation_of(const HarmonyChord chords[3], HarmonyProlongationLine *out) {
    if (chords == NULL || out == NULL || !harmony_chord_valid(&chords[0]) || !harmony_chord_valid(&chords[1]) ||
        !harmony_chord_valid(&chords[2])) {
        return false;
    }
    const HarmonyChord *middle = &chords[1];
    if (!is_tonic(&chords[0], 0) || middle->degree != 5) {
        return false;
    }
    if (middle->inversion == 1 && is_tonic(&chords[2], 0)) {
        *out = HARMONY_PROLONGATION_DO_TI_DO; /* V6 (triad) or V6/5 (seventh) */
        return true;
    }
    if (!middle->seventh) {
        return false;
    }
    if (middle->inversion == 2 && is_tonic(&chords[2], 1)) {
        *out = HARMONY_PROLONGATION_DO_RE_MI;
        return true;
    }
    if (middle->inversion == 2 && is_tonic(&chords[2], 0)) {
        *out = HARMONY_PROLONGATION_DO_RE_DO;
        return true;
    }
    if (middle->inversion == 3 && is_tonic(&chords[2], 1)) {
        *out = HARMONY_PROLONGATION_DO_FA_MI;
        return true;
    }
    return false;
}
