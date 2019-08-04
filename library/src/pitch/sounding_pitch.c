#include "pitch/sounding_pitch.h"

#include <stdlib.h>

struct SoundingPitch {
    PitchRealization *realization;
};

SoundingPitchResult sounding_pitch_create(PitchRealization *realization, SoundingPitch **out) {
    if (realization == NULL) {
        return SOUNDING_PITCH_ERR_NULL_REALIZATION;
    }

    SoundingPitch *sounding_pitch = (SoundingPitch *)malloc(sizeof(SoundingPitch));
    if (sounding_pitch == NULL) {
        return SOUNDING_PITCH_ERR_ALLOC;
    }

    sounding_pitch->realization = realization;

    *out = sounding_pitch;
    return SOUNDING_PITCH_OK;
}

void sounding_pitch_destroy(SoundingPitch *sounding_pitch) {
    if (sounding_pitch == NULL) {
        return;
    }
    pitch_realization_destroy(sounding_pitch->realization);
    free(sounding_pitch);
}

const PitchRealization *sounding_pitch_realization(const SoundingPitch *sounding_pitch) {
    return sounding_pitch->realization;
}

bool sounding_pitch_equals(const SoundingPitch *a, const SoundingPitch *b) {
    if (a == NULL || b == NULL) {
        return false;
    }
    return pitch_realization_equals(a->realization, b->realization);
}