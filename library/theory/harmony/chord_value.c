#include "theory/harmony/chord_value.h"

#include <string.h>

const ChordValueInterval CHORD_VALUE_SERIES_TWO[CHORD_VALUE_RANKS] = {
    {"octave", 12, 1, CHORD_VALUE_ROOT_LOWER},      {"fifth", 7, 2, CHORD_VALUE_ROOT_LOWER},         {"fourth", 5, 3, CHORD_VALUE_ROOT_UPPER},
    {"major-third", 4, 4, CHORD_VALUE_ROOT_LOWER},  {"minor-sixth", 8, 5, CHORD_VALUE_ROOT_UPPER},   {"minor-third", 3, 6, CHORD_VALUE_ROOT_LOWER},
    {"major-sixth", 9, 7, CHORD_VALUE_ROOT_UPPER},  {"major-second", 2, 8, CHORD_VALUE_ROOT_UPPER},  {"minor-seventh", 10, 9, CHORD_VALUE_ROOT_LOWER},
    {"minor-second", 1, 10, CHORD_VALUE_ROOT_UPPER}, {"major-seventh", 11, 11, CHORD_VALUE_ROOT_LOWER}, {"tritone", 6, 12, CHORD_VALUE_ROOT_NONE},
};

#define GROUP(code, group, rank, tritones, sharp, root) \
    { code, group, rank, CHORD_VALUE_TRITONES_##tritones, CHORD_VALUE_SHARP_##sharp, CHORD_VALUE_GROUP_ROOT_##root }

const ChordValueGroup CHORD_VALUE_GROUPS[CHORD_VALUE_RANKS] = {
    GROUP("ia", 1, 1, NONE, NONE, BASS),    GROUP("ib", 1, 2, NONE, NONE, ABOVE),   GROUP("iia", 2, 3, ONE, MILD, BASS),
    GROUP("iib1", 2, 4, ONE, MILD, BASS),   GROUP("iib2", 2, 5, ONE, MILD, ABOVE),  GROUP("iib3", 2, 6, SEVERAL, MILD, ANY),
    GROUP("iiia", 3, 7, NONE, ANY, BASS),   GROUP("iiib", 3, 8, NONE, ANY, ABOVE),  GROUP("iva", 4, 9, ANY, SHARP, BASS),
    GROUP("ivb", 4, 10, ANY, SHARP, ABOVE), GROUP("v", 5, 11, NONE, ANY, NONE),     GROUP("vi", 6, 12, ANY, NONE, NONE),
};

#define RANK_IA 0
#define RANK_IB 1
#define RANK_IIA 2
#define RANK_IIB1 3
#define RANK_IIB2 4
#define RANK_IIB3 5
#define RANK_IIIA 6
#define RANK_IIIB 7
#define RANK_IVA 8
#define RANK_IVB 9
#define RANK_V 10
#define RANK_VI 11

#define PITCH_CLASSES 12

/* Whether the set of pitch classes (a bit mask) is `shape` (a mask on 0) moved up by some number of semitones. */
static bool is_transposition(unsigned mask, unsigned shape) {
    for (int step = 0; step < PITCH_CLASSES; step++) {
        unsigned moved = ((shape << step) | (shape >> (PITCH_CLASSES - step))) & 0xFFFu;
        if (moved == mask) {
            return true;
        }
    }
    return false;
}

/* Three tones, as sounding, that are two perfect fourths one above the other (or a fourth and its octave displacements): C F Bb, not C D G. */
static bool stacked_fourths(const int tones[3]) {
    int sorted[3] = {tones[0], tones[1], tones[2]};
    for (int i = 0; i < 2; i++) {
        for (int j = i + 1; j < 3; j++) {
            if (sorted[j] < sorted[i]) {
                int swap = sorted[i];
                sorted[i] = sorted[j];
                sorted[j] = swap;
            }
        }
    }
    return (sorted[1] - sorted[0]) % PITCH_CLASSES == 5 && (sorted[2] - sorted[1]) % PITCH_CLASSES == 5;
}

static int series_rank_of(int semitones) {
    for (int i = 0; i < CHORD_VALUE_RANKS; i++) {
        if (CHORD_VALUE_SERIES_TWO[i].semitones == semitones) {
            return i;
        }
    }
    return CHORD_VALUE_RANKS - 1;
}

bool chord_value_classify(const int *pitches, size_t count, ChordValue *out) {
    if (pitches == NULL || out == NULL || count == 0) {
        return false;
    }
    /* One tone per pitch class, the lowest sounding, in the order of the pitches given (the lowest pitch overall is the bass). */
    int tones[PITCH_CLASSES];
    size_t tone_count = 0;
    int bass = pitches[0];
    for (size_t i = 0; i < count; i++) {
        bass = pitches[i] < bass ? pitches[i] : bass;
    }
    for (size_t i = 0; i < count; i++) {
        size_t at = 0;
        while (at < tone_count && ((tones[at] % PITCH_CLASSES) + PITCH_CLASSES) % PITCH_CLASSES != ((pitches[i] % PITCH_CLASSES) + PITCH_CLASSES) % PITCH_CLASSES) {
            at++;
        }
        if (at == tone_count) {
            tones[tone_count++] = pitches[i];
        } else if (pitches[i] < tones[at]) {
            tones[at] = pitches[i];
        }
    }
    if (tone_count < 3) {
        return false;
    }
    unsigned mask = 0;
    for (size_t i = 0; i < tone_count; i++) {
        mask |= 1u << (((tones[i] % PITCH_CLASSES) + PITCH_CLASSES) % PITCH_CLASSES);
    }

    /* The pairs of tones, lower to upper: the tritones, the sharpest interval, and the best by Series 2 (the lower pair when two tie). */
    int tritones = 0;
    int sharp = CHORD_VALUE_SHARP_NONE;
    int best_rank = CHORD_VALUE_RANKS;
    int best_root = -1;
    int best_low = 0;
    bool only_sevenths = true; /* the mild intervals are minor sevenths only */
    for (size_t a = 0; a < tone_count; a++) {
        for (size_t b = a + 1; b < tone_count; b++) {
            int low = tones[a] < tones[b] ? tones[a] : tones[b];
            int high = tones[a] < tones[b] ? tones[b] : tones[a];
            int d = (high - low) % PITCH_CLASSES;
            tritones += d == 6;
            if (d == 1 || d == 11) {
                sharp = CHORD_VALUE_SHARP_SHARP;
            } else if (d == 2 || d == 10) {
                sharp = sharp == CHORD_VALUE_SHARP_NONE ? CHORD_VALUE_SHARP_MILD : sharp;
                only_sevenths = only_sevenths && d == 10;
            }
            int rank = series_rank_of(d);
            if (rank < best_rank || (rank == best_rank && low < best_low)) {
                best_rank = rank;
                best_low = low;
                int root_tone = CHORD_VALUE_SERIES_TWO[rank].root == CHORD_VALUE_ROOT_UPPER ? high : low;
                best_root = root_tone;
            }
        }
    }
    int root_pc = best_root >= 0 ? ((best_root % PITCH_CLASSES) + PITCH_CLASSES) % PITCH_CLASSES : -1;
    bool in_bass = best_root >= 0 && ((best_root % PITCH_CLASSES) + PITCH_CLASSES) % PITCH_CLASSES == ((bass % PITCH_CLASSES) + PITCH_CLASSES) % PITCH_CLASSES;

    int group;
    bool rootless = false;
    if (tone_count == 3 && (is_transposition(mask, 0x111u) || stacked_fourths(tones))) { /* an augmented triad, two superposed fourths */
        group = RANK_V;
        rootless = true;
    } else if ((tone_count == 3 && is_transposition(mask, 0x049u)) || (tone_count == 4 && is_transposition(mask, 0x249u))) { /* {0,3,6}, {0,3,6,9} */
        group = RANK_VI;
        rootless = true;
    } else if (tritones == 0) {
        bool triad = tone_count == 3 && sharp == CHORD_VALUE_SHARP_NONE && (is_transposition(mask, 0x091u) || is_transposition(mask, 0x089u));
        group = triad ? (in_bass ? RANK_IA : RANK_IB) : (in_bass ? RANK_IIIA : RANK_IIIB);
    } else if (sharp == CHORD_VALUE_SHARP_SHARP) {
        group = in_bass ? RANK_IVA : RANK_IVB;
    } else if (tritones > 1) {
        group = RANK_IIB3;
    } else if (!in_bass) {
        group = RANK_IIB2;
    } else {
        group = only_sevenths ? RANK_IIA : RANK_IIB1;
    }
    out->group = CHORD_VALUE_GROUPS[group].code;
    out->rank = CHORD_VALUE_GROUPS[group].rank;
    out->root_pitch_class = rootless ? -1 : root_pc;
    out->root_in_bass = rootless ? false : in_bass;
    return true;
}
