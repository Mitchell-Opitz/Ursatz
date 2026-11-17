#include "model/time/temporal_anchor.h"

#include <stdlib.h>

struct TemporalAnchor {
    MusicalTime position;
};

TemporalAnchorResult temporal_anchor_create(MusicalTime *position, TemporalAnchor **out) {
    if (position == NULL) {
        return TEMPORAL_ANCHOR_ERR_NULL_POSITION;
    }

    TemporalAnchor *anchor = (TemporalAnchor *)malloc(sizeof(TemporalAnchor));
    if (anchor == NULL) {
        return TEMPORAL_ANCHOR_ERR_ALLOC;
    }

    anchor->position = *position;
    musical_time_destroy(position);

    *out = anchor;
    return TEMPORAL_ANCHOR_OK;
}

void temporal_anchor_destroy(TemporalAnchor *anchor) {
    if (anchor == NULL) {
        return;
    }
    free(anchor);
}

const MusicalTime *temporal_anchor_position(const TemporalAnchor *anchor) {
    return &anchor->position;
}