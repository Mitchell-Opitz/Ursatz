#ifndef MUSIC_SEMANTICS_CONTOUR_H
#define MUSIC_SEMANTICS_CONTOUR_H

#include <stddef.h>

#include "pitch/pitch_identity.h"

/*
 * Contour (Value): a relative pitch change (up/down/same) over a
 * PitchIdentity sequence.
 *
 * Fields:
 *   - pitch_ids: owned, ordered array of PitchIdentity, count >= 2 (a
 *     single pitch has no relative change to observe).
 *   - directions: owned array of ContourDirection, count ==
 *     pitch_id_count - 1. directions[i] is the observed change from
 *     pitch_ids[i] to pitch_ids[i + 1].
 *
 * directions are supplied by the caller, not computed here -- the same
 * principle semantics/interval.h already establishes: PitchIdentity is
 * an opaque token with no numeric semantics (pitch/pitch_identity.h),
 * so this type has no way to derive "up" vs "down" vs "same" from the
 * tokens themselves. Whatever produced the sequence (a caller reading
 * real pitch data under some tuning/notation) supplies the directions
 * it already knows.
 *
 * Contour is a Value, not an Entity -- it carries no EntityID of its
 * own (same as Sonority/Scale). Contour has no FrameworkReference: an
 * observed pitch-direction sequence does not require an interpretive
 * claim to exist. Registry non-normative, descriptive examples only
 * (docs/modules/semantics.md).
 *
 * Contour has no mutator: a revision is a new Contour construction,
 * not an in-place field change (Law C2).
 */

typedef struct Contour Contour;

typedef enum { CONTOUR_DIRECTION_UP, CONTOUR_DIRECTION_DOWN, CONTOUR_DIRECTION_SAME } ContourDirection;

typedef enum {
    CONTOUR_OK = 0,
    CONTOUR_ERR_NULL_PITCH_IDS,
    CONTOUR_ERR_NULL_PITCH_ID,
    CONTOUR_ERR_TOO_FEW_PITCH_IDS,
    CONTOUR_ERR_NULL_DIRECTIONS,
    CONTOUR_ERR_DIRECTION_COUNT_MISMATCH,
    CONTOUR_ERR_ALLOC
} ContourResult;

/*
 * On success takes ownership of each PitchIdentity pitch_ids points to
 * (destroyed by contour_destroy) and copies the pitch_ids and directions
 * arrays: both arrays stay the caller's to free. On failure nothing is taken.
 * pitch_ids must be non-NULL with pitch_id_count >= 2; no entry may be
 * NULL. directions must be non-NULL with direction_count ==
 * pitch_id_count - 1.
 */
ContourResult contour_create(PitchIdentity **pitch_ids, size_t pitch_id_count, ContourDirection *directions,
                              size_t direction_count, Contour **out);
void contour_destroy(Contour *contour);

size_t contour_pitch_id_count(const Contour *contour);
const PitchIdentity *contour_pitch_id_at(const Contour *contour, size_t index);

size_t contour_direction_count(const Contour *contour);
ContourDirection contour_direction_at(const Contour *contour, size_t index);

#endif