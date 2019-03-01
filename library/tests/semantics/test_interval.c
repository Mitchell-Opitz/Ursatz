#include <assert.h>
#include <stdio.h>

#include "identity/entity_id.h"
#include "pitch/pitch_identity.h"
#include "semantics/interval.h"

/*
 * Property-based test: for every (has_directed, has_undirected,
 * has_chromatic) combination and a spread of directed values, Interval
 * MUST satisfy:
 *
 *   1. A dimension not supplied at construction is never reported as
 *      known (interval_has_* is false), regardless of what default
 *      the underlying storage happens to hold.
 *   2. When both directed and undirected are supplied, they remain
 *      consistent (undirected == |directed|) after construction --
 *      the accessor never returns a value that violates that relation.
 *   3. interval_create rejects a directed/undirected pair that is
 *      inconsistent up front (undirected != |directed|).
 *   4. pitch_a/pitch_b are returned unchanged (identity preserved,
 *      not copied through some intermediate representation).
 */

static PitchIdentity *make_pitch(int token) {
    PitchIdentity *pitch;
    assert(pitch_identity_create(&token, sizeof(token), &pitch) == PITCH_IDENTITY_OK);
    return pitch;
}

static void check_case(const PitchIdentity *a, const PitchIdentity *b, bool has_directed, long directed,
                        bool has_undirected, unsigned long undirected, bool has_chromatic,
                        unsigned long chromatic) {
    Interval *interval;
    IntervalResult result = interval_create(a, b, has_directed, directed, has_undirected, undirected,
                                             has_chromatic, chromatic, &interval);
    assert(result == INTERVAL_OK);

    assert(interval_pitch_a(interval) == a);
    assert(interval_pitch_b(interval) == b);

    assert(interval_has_directed_semitones(interval) == has_directed);
    if (has_directed) {
        assert(interval_directed_semitones(interval) == directed);
    }

    assert(interval_has_undirected_semitones(interval) == has_undirected);
    if (has_undirected) {
        assert(interval_undirected_semitones(interval) == undirected);
    }

    assert(interval_has_chromatic_distance(interval) == has_chromatic);
    if (has_chromatic) {
        assert(interval_chromatic_distance(interval) == chromatic);
    }

    if (has_directed && has_undirected) {
        unsigned long magnitude = directed < 0 ? (unsigned long)(-directed) : (unsigned long)directed;
        assert(interval_undirected_semitones(interval) == magnitude);
    }

    interval_destroy(interval);
}

int main(void) {
    PitchIdentity *a = make_pitch(60);
    PitchIdentity *b = make_pitch(64);

    long directed_values[] = {-12, -7, -1, 0, 1, 7, 12};
    size_t directed_count = sizeof(directed_values) / sizeof(directed_values[0]);

    for (size_t i = 0; i < directed_count; i++) {
        long directed = directed_values[i];
        unsigned long undirected = directed < 0 ? (unsigned long)(-directed) : (unsigned long)directed;

        /* All three dimensions known and consistent. */
        check_case(a, b, true, directed, true, undirected, true, undirected);

        /* Only directed known -- undirected/chromatic MUST NOT be reported as known. */
        check_case(a, b, true, directed, false, 0, false, 0);

        /* Only undirected known. */
        check_case(a, b, false, 0, true, undirected, false, 0);

        /* Only chromatic distance known. */
        check_case(a, b, false, 0, false, 0, true, undirected);

        /* Nothing known at all -- a valid Interval, just an uninformative one. */
        check_case(a, b, false, 0, false, 0, false, 0);
    }

    /* Inconsistent directed/undirected pair MUST be rejected. */
    Interval *bad;
    IntervalResult bad_result = interval_create(a, b, true, 5, true, 6, false, 0, &bad);
    assert(bad_result == INTERVAL_ERR_INCONSISTENT_DIRECTED_UNDIRECTED);

    /* NULL pitches MUST be rejected. */
    Interval *null_result;
    assert(interval_create(NULL, b, false, 0, false, 0, false, 0, &null_result) == INTERVAL_ERR_NULL_PITCH);
    assert(interval_create(a, NULL, false, 0, false, 0, false, 0, &null_result) == INTERVAL_ERR_NULL_PITCH);

    pitch_identity_destroy(a);
    pitch_identity_destroy(b);

    printf("test_interval: ok\n");
    return 0;
}