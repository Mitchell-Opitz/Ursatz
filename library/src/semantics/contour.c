#include "semantics/contour.h"

#include <stdlib.h>

struct Contour {
    PitchIdentity **pitch_ids;
    size_t pitch_id_count;
    ContourDirection *directions;
    size_t direction_count;
};

ContourResult contour_create(PitchIdentity **pitch_ids, size_t pitch_id_count, ContourDirection *directions,
                              size_t direction_count, Contour **out) {
    if (pitch_ids == NULL) {
        return CONTOUR_ERR_NULL_PITCH_IDS;
    }
    if (pitch_id_count < 2) {
        return CONTOUR_ERR_TOO_FEW_PITCH_IDS;
    }
    for (size_t i = 0; i < pitch_id_count; i++) {
        if (pitch_ids[i] == NULL) {
            return CONTOUR_ERR_NULL_PITCH_ID;
        }
    }
    if (directions == NULL) {
        return CONTOUR_ERR_NULL_DIRECTIONS;
    }
    if (direction_count != pitch_id_count - 1) {
        return CONTOUR_ERR_DIRECTION_COUNT_MISMATCH;
    }

    Contour *contour = (Contour *)malloc(sizeof(Contour));
    if (contour == NULL) {
        return CONTOUR_ERR_ALLOC;
    }

    PitchIdentity **owned_pitch_ids = (PitchIdentity **)malloc(sizeof(PitchIdentity *) * pitch_id_count);
    if (owned_pitch_ids == NULL) {
        free(contour);
        return CONTOUR_ERR_ALLOC;
    }
    for (size_t i = 0; i < pitch_id_count; i++) {
        owned_pitch_ids[i] = pitch_ids[i];
    }

    ContourDirection *owned_directions = (ContourDirection *)malloc(sizeof(ContourDirection) * direction_count);
    if (owned_directions == NULL) {
        free(owned_pitch_ids);
        free(contour);
        return CONTOUR_ERR_ALLOC;
    }
    for (size_t i = 0; i < direction_count; i++) {
        owned_directions[i] = directions[i];
    }

    contour->pitch_ids = owned_pitch_ids;
    contour->pitch_id_count = pitch_id_count;
    contour->directions = owned_directions;
    contour->direction_count = direction_count;

    *out = contour;
    return CONTOUR_OK;
}

void contour_destroy(Contour *contour) {
    if (contour == NULL) {
        return;
    }
    for (size_t i = 0; i < contour->pitch_id_count; i++) {
        pitch_identity_destroy(contour->pitch_ids[i]);
    }
    free(contour->pitch_ids);
    free(contour->directions);
    free(contour);
}

size_t contour_pitch_id_count(const Contour *contour) {
    return contour->pitch_id_count;
}

const PitchIdentity *contour_pitch_id_at(const Contour *contour, size_t index) {
    return contour->pitch_ids[index];
}

size_t contour_direction_count(const Contour *contour) {
    return contour->direction_count;
}

ContourDirection contour_direction_at(const Contour *contour, size_t index) {
    return contour->directions[index];
}