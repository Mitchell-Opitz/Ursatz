#include "pitch/written_pitch.h"

#include <stdlib.h>

struct WrittenPitch {
    PitchSpelling *spelling;
};

WrittenPitchResult written_pitch_create(PitchSpelling *spelling, WrittenPitch **out) {
    if (spelling == NULL) {
        return WRITTEN_PITCH_ERR_NULL_SPELLING;
    }

    WrittenPitch *written_pitch = (WrittenPitch *)malloc(sizeof(WrittenPitch));
    if (written_pitch == NULL) {
        return WRITTEN_PITCH_ERR_ALLOC;
    }

    written_pitch->spelling = spelling;

    *out = written_pitch;
    return WRITTEN_PITCH_OK;
}

void written_pitch_destroy(WrittenPitch *written_pitch) {
    if (written_pitch == NULL) {
        return;
    }
    pitch_spelling_destroy(written_pitch->spelling);
    free(written_pitch);
}

const PitchSpelling *written_pitch_spelling(const WrittenPitch *written_pitch) {
    return written_pitch->spelling;
}

bool written_pitch_equals(const WrittenPitch *a, const WrittenPitch *b) {
    if (a == NULL || b == NULL) {
        return false;
    }
    return pitch_spelling_equals(a->spelling, b->spelling);
}