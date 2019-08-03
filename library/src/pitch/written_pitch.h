#ifndef MUSIC_PITCH_WRITTEN_PITCH_H
#define MUSIC_PITCH_WRITTEN_PITCH_H

#include <stdbool.h>

#include "pitch/pitch_spelling.h"

/*
 * WrittenPitch: pitch as notated, pre-transposition. Wraps a
 * PitchSpelling; carries no other fields. Immutable, no mutators.
 *
 * "Pre-transposition" describes what this type represents (the notated
 * pitch before any transposing-instrument or octave-transposition
 * adjustment is applied), not an operation this type performs --
 * WrittenPitch does not itself model transposition. SoundingPitch is the
 * post-transposition counterpart (pitch/sounding_pitch.h); the
 * transformation between the two is out of scope for this layer (no
 * Transformation/Activity machinery exists yet to drive it).
 */

typedef struct WrittenPitch WrittenPitch;

typedef enum {
    WRITTEN_PITCH_OK = 0,
    WRITTEN_PITCH_ERR_NULL_SPELLING,
    WRITTEN_PITCH_ERR_ALLOC
} WrittenPitchResult;

/* Takes ownership of spelling on success (destroyed by written_pitch_destroy). */
WrittenPitchResult written_pitch_create(PitchSpelling *spelling, WrittenPitch **out);
void written_pitch_destroy(WrittenPitch *written_pitch);

const PitchSpelling *written_pitch_spelling(const WrittenPitch *written_pitch);

/* Value equality: forwards to pitch_spelling_equals on the wrapped PitchSpelling. */
bool written_pitch_equals(const WrittenPitch *a, const WrittenPitch *b);

#endif