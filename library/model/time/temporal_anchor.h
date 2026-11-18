#ifndef MUSIC_TIME_TEMPORAL_ANCHOR_H
#define MUSIC_TIME_TEMPORAL_ANCHOR_H

#include <stdbool.h>
#include "model/time/musical_time.h"

/*
 * TemporalAnchor: situates a higher-layer object at a MusicalTime position,
 * without that object embedding position semantics itself. Wraps a
 * MusicalTime; carries no other fields.
 *
 * Built for L3 (music-events) Event to compose with L1 Occurrence, so an
 * Occurrence-based type gains a temporal position without music-time
 * internals leaking into music-core or music-events reimplementing
 * MusicalTime's rational representation.
 */

typedef struct TemporalAnchor TemporalAnchor;

typedef enum {
    TEMPORAL_ANCHOR_OK = 0,
    TEMPORAL_ANCHOR_ERR_ALLOC
} TemporalAnchorResult;

/* position is copied. */
TemporalAnchorResult temporal_anchor_create(MusicalTime position, TemporalAnchor **out);
void temporal_anchor_destroy(TemporalAnchor *anchor);

const MusicalTime *temporal_anchor_position(const TemporalAnchor *anchor);

#endif