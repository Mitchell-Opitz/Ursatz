#ifndef URSATZ_GALANT_SCHEMAS_TESTS_GALANT_SPEC_EXEMPLAR_H
#define URSATZ_GALANT_SCHEMAS_TESTS_GALANT_SPEC_EXEMPLAR_H

#include "galant_exemplar.h"
#include "galant_prototype_spec.h"

/* An exemplar of a spec in C: each stage's melody and bass are the pitches of
   the degrees the spec gives (a raised degree a semitone above the major scale,
   a lowered one below, and "either" the major scale's own), the melody in
   octave 5 and the bass in octave 3, on the beats the spec gives. Set up in
   place with spec_exemplar. */
typedef struct {
    GalantExemplarStage stages[SPEC_MAX];
    GalantExemplar exemplar;
} SpecExemplar;

static inline const PitchSpelling *spec_pitch(const SpecDegree *degree, int octave) {
    static const PitchLetter LETTERS[7] = {PITCH_LETTER_C, PITCH_LETTER_D, PITCH_LETTER_E, PITCH_LETTER_F,
                                           PITCH_LETTER_G, PITCH_LETTER_A, PITCH_LETTER_B};
    int accidental = degree->alteration == GALANT_ALTERATION_RAISED    ? 1
                     : degree->alteration == GALANT_ALTERATION_LOWERED ? -1
                                                                       : 0;
    return note(LETTERS[degree->degree - 1], accidental, octave);
}

static inline void spec_exemplar(SpecExemplar *e, const PrototypeSpec *spec) {
    size_t n = strlen(spec->strengths);
    for (size_t i = 0; i < n; i++) {
        e->stages[i].melody = spec_pitch(&spec->melody[i], 5);
        e->stages[i].bass = spec_pitch(&spec->bass[i], 3);
        e->stages[i].strength = spec->strengths[i] == 'S' ? BEAT_STRENGTH_STRONG : BEAT_STRENGTH_WEAK;
        e->stages[i].melody_next = spec->melody_next[i].degree != 0 ? spec_pitch(&spec->melody_next[i], 5) : NULL;
    }
    e->exemplar = (GalantExemplar){e->stages, n, P(C, 0, 4)};
}

#endif
