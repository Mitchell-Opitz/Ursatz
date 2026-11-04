#include "galant_degree.h"

#include <stddef.h>

#include "pitch/midi_pitch.h"
#include "standard-counterpoint/counterpoint_pitch.h"

/* The semitones above the tonic of degrees 1 to 7 of the major scale. */
static const int MAJOR_SEMITONES[7] = {0, 2, 4, 5, 7, 9, 11};

static int modulo(int value, int divisor) {
    int m = value % divisor;
    return m < 0 ? m + divisor : m;
}

bool galant_degree_of(const PitchSpelling *pitch, const PitchSpelling *tonic, GalantPitchDegree *out) {
    if (pitch == NULL || tonic == NULL || out == NULL) {
        return false;
    }
    int steps = modulo(standard_counterpoint_step_number(pitch) - standard_counterpoint_step_number(tonic), 7);
    int semitones = modulo(standard_counterpoint_semitone_number(pitch) - standard_counterpoint_semitone_number(tonic),
                           12);
    int alteration = semitones - MAJOR_SEMITONES[steps];
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
    static const GalantPitchDegree TABLE[12] = {{1, 0}, {2, -1}, {2, 0}, {3, -1}, {3, 0}, {4, 0},
                                                {4, 1}, {5, 0},  {6, -1}, {6, 0}, {7, -1}, {7, 0}};
    return TABLE[pitch_class_of_midi(semitones)];
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

const char *galant_alteration_name(GalantAlteration alteration) {
    switch (alteration) {
        case GALANT_ALTERATION_NATURAL:
            return "natural";
        case GALANT_ALTERATION_LOWERED:
            return "lowered";
        case GALANT_ALTERATION_EITHER:
            return "either";
        case GALANT_ALTERATION_RAISED:
            return "raised";
        default:
            return NULL;
    }
}
