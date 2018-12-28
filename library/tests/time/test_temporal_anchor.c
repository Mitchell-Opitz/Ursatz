#include "time/temporal_anchor.h"

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

/* Property: TemporalAnchor equality follows MusicalTime's scale-invariant
   equality (MusicalTime(a,b) == MusicalTime(k*a, k*b) for any positive k). */
static void test_equality_scale_invariant(void) {
    MusicalTime *base_position = NULL;
    musical_time_create(-3, 4, &base_position);
    TemporalAnchor *base = NULL;
    temporal_anchor_create(base_position, &base);

    for (unsigned long k = 1; k <= 20; k++) {
        MusicalTime *scaled_position = NULL;
        musical_time_create(-3L * (long)k, 4UL * k, &scaled_position);
        TemporalAnchor *scaled = NULL;
        temporal_anchor_create(scaled_position, &scaled);

        assert(temporal_anchor_equals(base, scaled));
        temporal_anchor_destroy(scaled);
    }

    temporal_anchor_destroy(base);
}

static void test_inequality_different_position(void) {
    MusicalTime *position_a = NULL;
    musical_time_create(1, 3, &position_a);
    TemporalAnchor *a = NULL;
    temporal_anchor_create(position_a, &a);

    MusicalTime *position_b = NULL;
    musical_time_create(1, 4, &position_b);
    TemporalAnchor *b = NULL;
    temporal_anchor_create(position_b, &b);

    assert(!temporal_anchor_equals(a, b));

    temporal_anchor_destroy(a);
    temporal_anchor_destroy(b);
}

int main(void) {
    test_create_rejects_null_position();
    test_create_accepts_valid();
    test_create_accepts_negative_position();
    test_position_accessor_matches_wrapped_time();
    test_equality_scale_invariant();
    test_inequality_different_position();
    return 0;
}