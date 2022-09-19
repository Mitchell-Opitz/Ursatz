#include "harmony_six_four.h"

#include <stddef.h>

#include "harmony_functions.h"
#include "harmony_steps.h"

bool harmony_six_four_valid(const HarmonySixFour *six_four) {
    if (six_four == NULL || !harmony_mode_valid(six_four->mode)) {
        return false;
    }
    for (int i = 0; i < 3; i++) {
        if (!harmony_chord_valid(&six_four->chords[i]) || !beat_strength_valid(six_four->beat[i])) {
            return false;
        }
    }
    return true;
}

static int bass_degree(const HarmonySixFour *s, int chord) {
    return harmony_step_degree(s->steps[chord][VOICE_PART_BASS]);
}

/* The root a fourth above a bass degree: degree + 3, wrapping. */
static int root_above(int bass) {
    return (bass - 1 + 3) % 7 + 1;
}

bool harmony_six_four_middle_is_six_four(const HarmonySixFour *s) {
    if (!harmony_six_four_valid(s)) {
        return false;
    }
    const HarmonyChord *m = &s->chords[1];
    return m->inversion == 2 && !m->seventh && m->degree == root_above(bass_degree(s, 1));
}

static int sign(int n) {
    return n > 0 ? 1 : (n < 0 ? -1 : 0);
}

static int magnitude(int n) {
    return n < 0 ? -n : n;
}

static bool same_root_position_chord(const HarmonyChord *a, const HarmonyChord *b) {
    return a->degree == b->degree && a->inversion == 0 && b->inversion == 0 && !a->seventh && !b->seventh;
}

static bool upper_voices_sustain(const HarmonySixFour *s) {
    for (int v = 0; v < VOICE_PART_BASS; v++) {
        if (s->steps[0][v] != s->steps[1][v] || s->steps[1][v] != s->steps[2][v]) {
            return false;
        }
    }
    return true;
}

bool harmony_six_four_classify(const HarmonySixFour *s, SixFourType *out) {
    if (out == NULL || !harmony_six_four_middle_is_six_four(s)) {
        return false;
    }
    if (s->chords[1].cadential) {
        *out = SIX_FOUR_TYPE_CADENTIAL;
        return true;
    }
    int first = s->steps[1][VOICE_PART_BASS] - s->steps[0][VOICE_PART_BASS];
    int second = s->steps[2][VOICE_PART_BASS] - s->steps[1][VOICE_PART_BASS];

    if (first == 0 && second == 0 && same_root_position_chord(&s->chords[0], &s->chords[2]) &&
        s->chords[0].degree == bass_degree(s, 1)) {
        *out = SIX_FOUR_TYPE_NEIGHBOR;
        return true;
    }
    if (magnitude(first) == 1 && magnitude(second) == 1 && sign(first) == sign(second)) {
        HarmonicFunction before, after;
        if (harmony_function_of(s->mode, &s->chords[0], &before) &&
            harmony_function_of(s->mode, &s->chords[2], &after) && before == after) {
            *out = SIX_FOUR_TYPE_PASSING;
            return true;
        }
        return false;
    }
    if (magnitude(first) > 1 && s->chords[0].degree == s->chords[1].degree &&
        s->chords[1].degree == s->chords[2].degree && upper_voices_sustain(s)) {
        *out = SIX_FOUR_TYPE_ARPEGGIATING;
        return true;
    }
    return false;
}
