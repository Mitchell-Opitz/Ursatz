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

/* The bass scale degree of an accepted middle chord, or 0 for any other chord. The middle
   chord is V6 or an inverted V7, or the leading-tone chord taking the place of the V with
   the same bass note: ti (V6, V6/5, vii7), re (V4/3, vii6 the triad in first inversion,
   vii6/5), or fa (V4/2, vii4/3). The leading-tone triad is only ever in first inversion,
   and vii4/2 (le in the bass) is not modeled. */
static int middle_bass(const HarmonyChord *m) {
    if (m->degree == 5) {
        if (m->inversion == 1) {
            return 7; /* V6 (the triad) or V6/5 */
        }
        if (!m->seventh) {
            return 0;
        }
        return m->inversion == 2 ? 2 : (m->inversion == 3 ? 4 : 0);
    }
    if (m->degree == 7) {
        if (m->inversion == 0) {
            return m->seventh ? 7 : 0; /* vii7 */
        }
        if (m->inversion == 1) {
            return 2; /* vii6 (the triad) or vii6/5 */
        }
        return m->inversion == 2 && m->seventh ? 4 : 0; /* vii4/3 */
    }
    return 0;
}

bool harmony_prolongation_of(const HarmonyChord chords[3], HarmonyProlongationLine *out) {
    if (chords == NULL || out == NULL || !harmony_chord_valid(&chords[0]) || !harmony_chord_valid(&chords[1]) ||
        !harmony_chord_valid(&chords[2]) || !is_tonic(&chords[0], 0)) {
        return false;
    }
    int bass = middle_bass(&chords[1]);
    if (bass == 7 && is_tonic(&chords[2], 0)) {
        *out = HARMONY_PROLONGATION_DO_TI_DO;
        return true;
    }
    if (bass == 2 && is_tonic(&chords[2], 1)) {
        *out = HARMONY_PROLONGATION_DO_RE_MI;
        return true;
    }
    if (bass == 2 && is_tonic(&chords[2], 0)) {
        *out = HARMONY_PROLONGATION_DO_RE_DO;
        return true;
    }
    if (bass == 4 && is_tonic(&chords[2], 1)) {
        *out = HARMONY_PROLONGATION_DO_FA_MI;
        return true;
    }
    return false;
}
