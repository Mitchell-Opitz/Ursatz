#ifndef MUSIC_PITCH_SOUNDING_PITCH_H
#define MUSIC_PITCH_SOUNDING_PITCH_H

#include <stdbool.h>

#include "pitch/pitch_realization.h"

/*
 * SoundingPitch: pitch as it sounds, post-transposition. Wraps a
 * PitchRealization; carries no other fields. Immutable, no mutators.
 *
 * "Post-transposition" describes what this type represents (the
 * physically-sounding pitch after any transposing-instrument or
 * octave-transposition adjustment has been applied), not an operation
 * this type performs -- SoundingPitch does not itself model
 * transposition. WrittenPitch is the pre-transposition counterpart
 * (pitch/written_pitch.h); the transformation between the two is out of
 * scope for this layer (no Transformation/Activity machinery exists yet
 * to drive it).
 */

typedef struct SoundingPitch SoundingPitch;

typedef enum {
    SOUNDING_PITCH_OK = 0,
    SOUNDING_PITCH_ERR_NULL_REALIZATION,
    SOUNDING_PITCH_ERR_ALLOC
} SoundingPitchResult;

/* Takes ownership of realization on success (destroyed by sounding_pitch_destroy). */
SoundingPitchResult sounding_pitch_create(PitchRealization *realization, SoundingPitch **out);
void sounding_pitch_destroy(SoundingPitch *sounding_pitch);

const PitchRealization *sounding_pitch_realization(const SoundingPitch *sounding_pitch);

/* Value equality: forwards to pitch_realization_equals on the wrapped PitchRealization. */
bool sounding_pitch_equals(const SoundingPitch *a, const SoundingPitch *b);

#endif