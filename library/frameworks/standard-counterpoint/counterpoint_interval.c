#include "counterpoint_interval.h"

#include <stddef.h>

#include "counterpoint_pitch.h"

/* Semitones of the perfect (index 0, 3, 4, 7) and major (index 1, 2, 5, 6)
   interval at each simple step count 0 to 7. */
static const int PERFECT_SEMITONES[8] = {0, -1, -1, 5, 7, -1, -1, 12};
static const int MAJOR_SEMITONES[8] = {-1, 2, 4, -1, -1, 9, 11, -1};

static bool is_perfect_kind(int steps) {
    return steps == 0 || steps == 3 || steps == 4 || steps == 7;
}

bool standard_counterpoint_interval_between(const PitchSpelling *a, const PitchSpelling *b,
                                            StandardCounterpointInterval *out) {
    if (a == NULL || b == NULL || out == NULL) {
        return false;
    }
    const PitchSpelling *low = a;
    const PitchSpelling *high = b;
    if (standard_counterpoint_direction(a, b) < 0) {
        low = b;
        high = a;
    }

    int steps = standard_counterpoint_step_number(high) - standard_counterpoint_step_number(low);
    int semitones = standard_counterpoint_semitone_number(high) - standard_counterpoint_semitone_number(low);
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
    int simple_semitones = semitones - STANDARD_COUNTERPOINT_SEMITONES_PER_OCTAVE * octaves;

    StandardCounterpointQuality quality;
    if (is_perfect_kind(simple_steps)) {
        int delta = simple_semitones - PERFECT_SEMITONES[simple_steps];
        quality = delta == 0   ? STANDARD_COUNTERPOINT_QUALITY_PERFECT
                  : delta > 0 ? STANDARD_COUNTERPOINT_QUALITY_AUGMENTED
                              : STANDARD_COUNTERPOINT_QUALITY_DIMINISHED;
    } else {
        int delta = simple_semitones - MAJOR_SEMITONES[simple_steps];
        quality = delta == 0    ? STANDARD_COUNTERPOINT_QUALITY_MAJOR
                  : delta == -1 ? STANDARD_COUNTERPOINT_QUALITY_MINOR
                  : delta > 0   ? STANDARD_COUNTERPOINT_QUALITY_AUGMENTED
                                : STANDARD_COUNTERPOINT_QUALITY_DIMINISHED;
    }

    out->size = steps + 1;
    out->simple_size = simple_steps + 1;
    out->quality = quality;
    return true;
}

bool standard_counterpoint_interval_is_step(const StandardCounterpointInterval *interval) {
    return interval != NULL && interval->size == 2;
}

bool standard_counterpoint_interval_is_leap(const StandardCounterpointInterval *interval) {
    return interval != NULL && interval->size >= 3;
}
