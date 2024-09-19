#ifndef URSATZ_FRAMEWORKS_SCALE_HARMONIZATION_SCALE_HARMONIZATION_H
#define URSATZ_FRAMEWORKS_SCALE_HARMONIZATION_SCALE_HARMONIZATION_H

#include <stdbool.h>
#include <stddef.h>

#include "pitch/pitch_spelling.h"
#include "scale_direction.h"

/*
 * The subject of every rule in scale-harmonization-v1: a harmonization of a
 * bass line. It is a run of stages, one for each bass note (or, in a pattern
 * with two harmonies over one bass note, one for each harmony), the direction
 * the bass runs, and the tonic of the key. At each stage:
 *
 *   pitches[0]        the bass
 *   pitches[1 ...]    the upper voices, from the lowest to the highest
 *
 * Rhythm is not modeled. A suspended upper voice shows itself as a pitch that is
 * held from the stage before while the bass moves. Pitches are borrowed, not
 * owned, and read as sounding pitches. voice_count is the number of voices at
 * every stage, the bass included, from 2 (a bass and one upper part, as in
 * the patterns of the sequences) to SCALE_MAX_VOICES (four in the Rule of the
 * Octave).
 */

#define SCALE_MAX_VOICES 6
#define SCALE_MAX_STAGES 64

typedef struct {
    const PitchSpelling *pitches[SCALE_MAX_VOICES];
} ScaleStage;

typedef struct {
    const ScaleStage *stages;
    size_t stage_count;
    size_t voice_count;
    ScaleDirection direction;
    const PitchSpelling *tonic;
} ScaleHarmonization;

/* True when the harmonization has one to SCALE_MAX_STAGES stages, two to
   SCALE_MAX_VOICES voices, a non-NULL pitch for every voice at every stage, a
   valid direction, and a tonic. */
bool scale_harmonization_valid(const ScaleHarmonization *harmonization);

/* The generic size of the interval from the bass up to `voice` (1 to
   voice_count - 1) at `stage`, with whole octaves removed and an octave kept as
   8: a third is 3, a tenth is 3, an octave or a unison is 8 (a unison is 1). False
   (*out untouched) for an invalid harmonization, an index out of range, or an
   upper voice that sounds below the bass. */
bool scale_interval_above_bass(const ScaleHarmonization *harmonization, size_t stage, size_t voice, int *out);

#endif
