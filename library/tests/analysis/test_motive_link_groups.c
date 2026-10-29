#include <assert.h>
#include <stdlib.h>
#include <string.h>

#include "analysis/motive_link.h"

/*
 * motive_link_candidates (the nearest few in other pieces, gated, as unordered pairs) and motive_link_groups
 * (the components of the linked pairs).
 */

static Rational q(unsigned long long n, unsigned long long d) {
    return rational_ratio(n, d);
}

/* C4:q E4:e G4:e F4:q E4:q in C major, 4/4; shift moves the last pitch. */
static MotiveSketch motive(int last_pitch) {
    static const int pitches[5] = {60, 64, 67, 65, 64};
    static const unsigned long long onset_n[5] = {0, 1, 3, 1, 3};
    static const unsigned long long onset_d[5] = {1, 4, 8, 2, 4};
    static const unsigned long long dur_d[5] = {4, 8, 8, 4, 4};
    MotiveSketch sketch;
    memset(&sketch, 0, sizeof(sketch));
    for (size_t i = 0; i < 5; i++) {
        sketch.notes[i].pitch = pitches[i];
        sketch.notes[i].onset = q(onset_n[i], onset_d[i]);
        sketch.notes[i].duration = q(1, dur_d[i]);
    }
    sketch.notes[4].pitch = last_pitch;
    sketch.note_count = 5;
    sketch.has_scale = true;
    sketch.tonic_pc = 0;
    sketch.bar = q(1, 1);
    sketch.beat = q(1, 4);
    sketch.pickup = q(0, 1);
    sketch.length = q(1, 1);
    return sketch;
}

static void assert_pair(const MotiveLinkPair *pairs, size_t index, size_t low, size_t high) {
    assert(pairs[index].low == low && pairs[index].high == high);
}

static void test_candidates(void) {
    /* 0 and 2 are the same motive, 1 differs in its last note. All in different pieces. */
    MotiveSketch sketches[3] = {motive(64), motive(72), motive(64)};
    size_t groups[3] = {0, 1, 2};
    assert(motive_link_nearness(&sketches[0], &sketches[2]) > motive_link_nearness(&sketches[0], &sketches[1]));
    MotiveLinkPair *pairs = NULL;
    size_t count = 0;

    /* Nearest first: 0 takes 2; 1 ties 0 and 2 and takes the lower index (0); 2 takes 0. */
    assert(motive_link_candidates(sketches, groups, 3, 1, 0.0, &pairs, &count) == MOTIVE_LINK_OK);
    assert(count == 2);
    assert_pair(pairs, 0, 0, 1);
    assert_pair(pairs, 1, 0, 2);
    free(pairs);

    /* More per sketch: every pair across pieces, once. */
    assert(motive_link_candidates(sketches, groups, 3, 4, 0.0, &pairs, &count) == MOTIVE_LINK_OK);
    assert(count == 3);
    assert_pair(pairs, 0, 0, 1);
    assert_pair(pairs, 1, 0, 2);
    assert_pair(pairs, 2, 1, 2);
    free(pairs);

    /* The gate: only the identical motives pass nearness 1. */
    assert(motive_link_candidates(sketches, groups, 3, 4, 1.0, &pairs, &count) == MOTIVE_LINK_OK);
    assert(count == 1);
    assert_pair(pairs, 0, 0, 2);
    free(pairs);

    /* The same piece is never a candidate: 0 and 1 share piece 0, so only 2 pairs with each. */
    size_t same[3] = {0, 0, 1};
    assert(motive_link_candidates(sketches, same, 3, 4, 0.0, &pairs, &count) == MOTIVE_LINK_OK);
    assert(count == 2);
    assert_pair(pairs, 0, 0, 2);
    assert_pair(pairs, 1, 1, 2);
    free(pairs);

    /* All in one piece, or nothing: no pair, NULL. */
    size_t one[3] = {5, 5, 5};
    pairs = (MotiveLinkPair *)&count;
    assert(motive_link_candidates(sketches, one, 3, 4, 0.0, &pairs, &count) == MOTIVE_LINK_OK);
    assert(pairs == NULL && count == 0);
    pairs = (MotiveLinkPair *)&count;
    count = 9;
    assert(motive_link_candidates(NULL, NULL, 0, 1, 0.0, &pairs, &count) == MOTIVE_LINK_OK);
    assert(pairs == NULL && count == 0);
}

static void test_candidates_invalid(void) {
    MotiveSketch sketches[1] = {motive(64)};
    size_t groups[1] = {0};
    MotiveLinkPair *pairs = NULL;
    size_t count = 0;
    assert(motive_link_candidates(sketches, groups, 1, 0, 0.5, &pairs, &count) == MOTIVE_LINK_INVALID);
    assert(motive_link_candidates(sketches, groups, 1, 1, 1.5, &pairs, &count) == MOTIVE_LINK_INVALID);
    assert(motive_link_candidates(sketches, groups, 1, 1, -0.1, &pairs, &count) == MOTIVE_LINK_INVALID);
    assert(motive_link_candidates(NULL, groups, 1, 1, 0.5, &pairs, &count) == MOTIVE_LINK_INVALID);
    assert(motive_link_candidates(sketches, NULL, 1, 1, 0.5, &pairs, &count) == MOTIVE_LINK_INVALID);
    assert(motive_link_candidates(sketches, groups, 1, 1, 0.5, NULL, &count) == MOTIVE_LINK_INVALID);
    assert(motive_link_candidates(sketches, groups, 1, 1, 0.5, &pairs, NULL) == MOTIVE_LINK_INVALID);
}

static void test_groups(void) {
    /* Six sketches: {0, 3, 5} and {1, 2} are linked; the pair (2, 4) is not linked, so 4 is a singleton. */
    const MotiveLinkPair pairs[4] = {{0, 3}, {3, 5}, {1, 2}, {2, 4}};
    const bool linked[4] = {true, true, true, false};
    size_t *group_of = NULL;
    size_t group_count = 0;
    assert(motive_link_groups(6, pairs, linked, 4, &group_of, &group_count) == MOTIVE_LINK_OK);
    assert(group_count == 2);
    assert(group_of[0] == 0 && group_of[1] == 1 && group_of[2] == 1);
    assert(group_of[3] == 0 && group_of[4] == (size_t)-1 && group_of[5] == 0);
    free(group_of);

    /* Joined through a later pair: 0 - 2 and 1 - 2 make one group, numbered by its lowest member. */
    const MotiveLinkPair chain[2] = {{1, 2}, {0, 2}};
    const bool both[2] = {true, true};
    assert(motive_link_groups(4, chain, both, 2, &group_of, &group_count) == MOTIVE_LINK_OK);
    assert(group_count == 1);
    assert(group_of[0] == 0 && group_of[1] == 0 && group_of[2] == 0 && group_of[3] == (size_t)-1);
    free(group_of);

    /* Nothing linked: no group. */
    const bool none[2] = {false, false};
    assert(motive_link_groups(4, chain, none, 2, &group_of, &group_count) == MOTIVE_LINK_OK);
    assert(group_count == 0);
    for (size_t i = 0; i < 4; i++) {
        assert(group_of[i] == (size_t)-1);
    }
    free(group_of);

    group_of = (size_t *)&group_count;
    group_count = 5;
    assert(motive_link_groups(0, NULL, NULL, 0, &group_of, &group_count) == MOTIVE_LINK_OK);
    assert(group_of == NULL && group_count == 0);
}

static void test_groups_invalid(void) {
    const MotiveLinkPair reversed[1] = {{2, 1}};
    const MotiveLinkPair outside[1] = {{0, 4}};
    const bool linked[1] = {true};
    size_t *group_of = NULL;
    size_t group_count = 0;
    assert(motive_link_groups(4, reversed, linked, 1, &group_of, &group_count) == MOTIVE_LINK_INVALID);
    assert(motive_link_groups(4, outside, linked, 1, &group_of, &group_count) == MOTIVE_LINK_INVALID);
    assert(motive_link_groups(4, NULL, linked, 1, &group_of, &group_count) == MOTIVE_LINK_INVALID);
    assert(motive_link_groups(4, outside, NULL, 1, &group_of, &group_count) == MOTIVE_LINK_INVALID);
    assert(motive_link_groups(4, outside, linked, 1, NULL, &group_count) == MOTIVE_LINK_INVALID);
    assert(motive_link_groups(4, outside, linked, 1, &group_of, NULL) == MOTIVE_LINK_INVALID);
}

int main(void) {
    test_candidates();
    test_candidates_invalid();
    test_groups();
    test_groups_invalid();
    return 0;
}
