#include "theory/galant/galant_degree.h"

#include <stddef.h>

#include "model/pitch/midi_pitch.h"
#include "model/pitch/pitch_arithmetic.h"
#include "theory/tonality/tonal_key.h"

static int modulo(int value, int divisor) {
    int m = value % divisor;
    return m < 0 ? m + divisor : m;
}

bool galant_degree_of(const PitchSpelling *pitch, const PitchSpelling *tonic, GalantPitchDegree *out) {
    if (pitch == NULL || tonic == NULL || out == NULL) {
        return false;
    }
    int steps = modulo(pitch_spelling_step_number(pitch) - pitch_spelling_step_number(tonic), 7);
    int semitones = modulo(pitch_spelling_midi(pitch) - pitch_spelling_midi(tonic),
                           12);
    int alteration = semitones - standard_tonality_degree_semitone(STANDARD_TONALITY_MODE_MAJOR, steps + 1);
    if (alteration > 6) {
        alteration -= 12;
    } else if (alteration < -6) {
        alteration += 12;
    }
    out->degree = steps + 1;
    out->alteration = alteration;
    return true;
}

GalantPitchDegree galant_degree_of_semitones(int semitones) {
    GalantPitchDegree reading = {1, 0};
    standard_tonality_degree_of_semitone(pitch_class_of_midi(semitones), STANDARD_TONALITY_MODE_MAJOR, &reading.degree,
                                         &reading.alteration);
    return reading;
}

bool galant_degree_allows(const GalantDegree *wanted, const GalantPitchDegree *actual) {
    if (wanted == NULL || actual == NULL || wanted->degree != actual->degree) {
        return false;
    }
    switch (wanted->alteration) {
        case GALANT_ALTERATION_NATURAL:
            return actual->alteration == 0;
        case GALANT_ALTERATION_LOWERED:
            return actual->alteration == -1;
        case GALANT_ALTERATION_EITHER:
            return actual->alteration == 0 || actual->alteration == -1;
        case GALANT_ALTERATION_RAISED:
            return actual->alteration == 1;
        default:
            return false;
    }
}

