#include "theory/tonality/interval.h"

#include <stddef.h>

#include "model/pitch/midi_pitch.h"
#include "model/pitch/pitch_arithmetic.h"
#include "theory/tonality/tonal_key.h"

/* Semitones of the perfect (steps 0, 3, 4, 7) or major (steps 1, 2, 5, 6) interval at a simple step count 0 to 7: the major
   scale's degree steps + 1, and the octave (steps 7) is twelve. */
static int scale_semitones(int steps) {
    return steps == 7 ? PITCH_SEMITONES_PER_OCTAVE : standard_tonality_degree_semitone(STANDARD_TONALITY_MODE_MAJOR, steps + 1);
}

static bool is_perfect_kind(int steps) {
    return steps == 0 || steps == 3 || steps == 4 || steps == 7;
}

bool standard_tonality_interval_between(const PitchSpelling *a, const PitchSpelling *b, StandardTonalityInterval *out) {
    if (a == NULL || b == NULL || out == NULL) {
        return false;
    }
    const PitchSpelling *low = a;
    const PitchSpelling *high = b;
    if (pitch_spelling_direction(a, b) < 0) {
        low = b;
        high = a;
    }

    int steps = pitch_spelling_step_number(high) - pitch_spelling_step_number(low);
    int semitones = pitch_spelling_midi(high) - pitch_spelling_midi(low);
    if (steps < 0) {
        return false;
    }

    int simple_steps = 0;
    int octaves = 0;
    if (steps > 0) {
        simple_steps = steps % 7;
        octaves = steps / 7;
        if (simple_steps == 0) {
            simple_steps = 7;
            octaves -= 1;
        }
    }
    int simple_semitones = semitones - PITCH_SEMITONES_PER_OCTAVE * octaves;

    StandardTonalityIntervalQuality quality;
    if (is_perfect_kind(simple_steps)) {
        int delta = simple_semitones - scale_semitones(simple_steps);
        quality = delta == 0   ? STANDARD_TONALITY_INTERVAL_QUALITY_PERFECT
                  : delta > 0 ? STANDARD_TONALITY_INTERVAL_QUALITY_AUGMENTED
                              : STANDARD_TONALITY_INTERVAL_QUALITY_DIMINISHED;
    } else {
        int delta = simple_semitones - scale_semitones(simple_steps);
        quality = delta == 0    ? STANDARD_TONALITY_INTERVAL_QUALITY_MAJOR
                  : delta == -1 ? STANDARD_TONALITY_INTERVAL_QUALITY_MINOR
                  : delta > 0   ? STANDARD_TONALITY_INTERVAL_QUALITY_AUGMENTED
                                : STANDARD_TONALITY_INTERVAL_QUALITY_DIMINISHED;
    }

    out->size = steps + 1;
    out->simple_size = simple_steps + 1;
    out->quality = quality;
    return true;
}

bool standard_tonality_interval_is_step(const StandardTonalityInterval *interval) {
    return interval != NULL && interval->size == 2;
}

bool standard_tonality_interval_is_leap(const StandardTonalityInterval *interval) {
    return interval != NULL && interval->size >= 3;
}
