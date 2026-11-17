#include "model/time/temporal_anchor.h"

#include <assert.h>
#include <stddef.h>

static void test_create_rejects_null_position(void) {
    TemporalAnchor *anchor = NULL;
    TemporalAnchorResult r = temporal_anchor_create(NULL, &anchor);
    assert(r == TEMPORAL_ANCHOR_ERR_NULL_POSITION);
    assert(anchor == NULL);
}

static void test_create_accepts_valid(void) {
    MusicalTime *position = NULL;
    musical_time_create(3, 4, &position);

    TemporalAnchor *anchor = NULL;
    TemporalAnchorResult r = temporal_anchor_create(position, &anchor);
    assert(r == TEMPORAL_ANCHOR_OK);
    assert(anchor != NULL);

    temporal_anchor_destroy(anchor);
}

/* No lower bound: negative positions (e.g. pickup measures) are legal,
   matching MusicalTime. */
static void test_create_accepts_negative_position(void) {
    MusicalTime *position = NULL;
    musical_time_create(-1, 4, &position);

    TemporalAnchor *anchor = NULL;
    TemporalAnchorResult r = temporal_anchor_create(position, &anchor);
    assert(r == TEMPORAL_ANCHOR_OK);

    const MusicalTime *anchor_position = temporal_anchor_position(anchor);
    assert(musical_time_numerator(anchor_position) == -1);
    assert(musical_time_denominator(anchor_position) == 4);

    temporal_anchor_destroy(anchor);
}

static void test_position_accessor_matches_wrapped_time(void) {
    MusicalTime *position = NULL;
    musical_time_create(1, 4, &position);

    TemporalAnchor *anchor = NULL;
    temporal_anchor_create(position, &anchor);

    const MusicalTime *anchor_position = temporal_anchor_position(anchor);
    assert(musical_time_numerator(anchor_position) == 1);
    assert(musical_time_denominator(anchor_position) == 4);

    temporal_anchor_destroy(anchor);
}

int main(void) {
    test_create_rejects_null_position();
    test_create_accepts_valid();
    test_create_accepts_negative_position();
    test_position_accessor_matches_wrapped_time();
    return 0;
}