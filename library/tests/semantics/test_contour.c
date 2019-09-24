#include <assert.h>
#include <stdio.h>

#include "pitch/pitch_identity.h"
#include "semantics/contour.h"

/*
 * Golden test: a known PitchIdentity sequence and caller-supplied
 * direction sequence produce a known Contour shape -- every field
 * comes back exactly as given, nothing implicit added or dropped.
 */

static PitchIdentity *make_pitch(const char *token) {
    PitchIdentity *identity;
    assert(pitch_identity_create(token, 2, &identity) == PITCH_IDENTITY_OK);
    return identity;
}

int main(void) {
    PitchIdentity *pitch_ids[3] = {make_pitch("c\0"), make_pitch("e\0"), make_pitch("e\0")};
    ContourDirection directions[2] = {CONTOUR_DIRECTION_UP, CONTOUR_DIRECTION_SAME};

    Contour *contour;
    assert(contour_create(pitch_ids, 3, directions, 2, &contour) == CONTOUR_OK);

    assert(contour_pitch_id_count(contour) == 3);
    assert(contour_pitch_id_at(contour, 0) == pitch_ids[0]);
    assert(contour_pitch_id_at(contour, 1) == pitch_ids[1]);
    assert(contour_pitch_id_at(contour, 2) == pitch_ids[2]);

    assert(contour_direction_count(contour) == 2);
    assert(contour_direction_at(contour, 0) == CONTOUR_DIRECTION_UP);
    assert(contour_direction_at(contour, 1) == CONTOUR_DIRECTION_SAME);

    contour_destroy(contour);

    /* A single pitch has no relative change to observe. */
    PitchIdentity *lone_pitch_ids[1] = {make_pitch("c\0")};
    Contour *too_few;
    ContourResult too_few_result = contour_create(lone_pitch_ids, 1, NULL, 0, &too_few);
    assert(too_few_result == CONTOUR_ERR_TOO_FEW_PITCH_IDS);
    pitch_identity_destroy(lone_pitch_ids[0]);

    /* Mismatched direction count is rejected. */
    PitchIdentity *mismatch_pitch_ids[2] = {make_pitch("c\0"), make_pitch("d\0")};
    ContourDirection mismatch_directions[2] = {CONTOUR_DIRECTION_UP, CONTOUR_DIRECTION_UP};
    Contour *mismatch;
    ContourResult mismatch_result = contour_create(mismatch_pitch_ids, 2, mismatch_directions, 2, &mismatch);
    assert(mismatch_result == CONTOUR_ERR_DIRECTION_COUNT_MISMATCH);
    pitch_identity_destroy(mismatch_pitch_ids[0]);
    pitch_identity_destroy(mismatch_pitch_ids[1]);

    printf("test_contour: ok\n");
    return 0;
}