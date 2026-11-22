#include <math.h>

#include "analysis/form/analysis_motive_match.h"
#include "analysis/tests/form/form_test_util.h"

/* The framework's own matching, on shapes built from real notes: a varied motive, the fewest intervals of a fragment, and the
   precedence between kinds. */

typedef struct {
    FormFixture f;
    size_t count;
    StandardFormMotiveLine line;
} Notes;

static void notes_init(Notes *n) {
    memset(n, 0, sizeof(*n));
    form_init(&n->f, 4, 0);
}

/* A figure at the given quarter starts: each note lasts until the next one (so the notes are joined), the last a short one. */
static void figure(Notes *n, const double *starts, const int *pitches, size_t count) {
    for (size_t i = 0; i < count; i++) {
        double duration = i + 1 < count ? starts[i + 1] - starts[i] : 0.25;
        form_note(&n->f, starts[i], duration, pitches[i]);
    }
    n->count += count;
}

static void notes_done(Notes *n) {
    form_finish(&n->f);
    assert(standard_form_motive_line_build(&n->line, &n->f.input, 0, NULL));
}

static StandardFormMotiveShape shape_of(Notes *n, size_t first, size_t count) {
    StandardFormMotiveShape shape;
    assert(standard_form_motive_shape(&n->line, first, count, false, &shape));
    return shape;
}

static void notes_destroy(Notes *n) {
    standard_form_motive_line_destroy(&n->line);
    form_destroy(&n->f);
}

static void test_a_varied_motive_changes_an_interval_or_the_rhythm(void) {
    Notes n;
    notes_init(&n);
    const int a[5] = {60, 62, 64, 65, 67};   /* +2 +2 +1 +2 */
    const int one_changed[5] = {60, 62, 64, 65, 69};
    const int two_changed[5] = {60, 62, 64, 66, 69};
    const double s0[5] = {0, 1, 2, 3, 4};
    const double s1[5] = {10, 11, 12, 13, 14};
    const double s2[5] = {20, 21, 22, 23, 24};
    const double s3[5] = {30, 31, 32, 33, 35}; /* the last gap is 2 */
    const double s4[5] = {40, 41, 42, 43, 44};
    const int same_pitches[5] = {60, 62, 64, 65, 67};
    figure(&n, s0, a, 5);
    figure(&n, s1, one_changed, 5);
    figure(&n, s2, two_changed, 5);
    figure(&n, s3, one_changed, 5);
    figure(&n, s4, same_pitches, 5);
    notes_done(&n);
    StandardFormMotiveShape proto = shape_of(&n, 0, 5);
    StandardFormMotiveMatch match;
    StandardFormMotiveShape cand = shape_of(&n, 5, 5);
    assert(standard_form_motive_match_varied(&proto, &cand, 1, &match) && match.kind == (int)MOTIVE_OCCURRENCE_VARIED &&
           match.mismatches == 1 && match.intervals == 4 && fabs(match.score - 0.75) < 1e-12 && match.rhythm_factor == 1.0);
    cand = shape_of(&n, 10, 5);
    assert(!standard_form_motive_match_varied(&proto, &cand, 1, &match)); /* two intervals differ */
    assert(standard_form_motive_match_varied(&proto, &cand, 2, &match) && fabs(match.score - 0.5) < 1e-12);
    cand = shape_of(&n, 15, 5); /* one interval and the rhythm differ: the rhythm costs the factor 0.8 */
    assert(standard_form_motive_match_varied(&proto, &cand, 1, &match) && fabs(match.score - 0.75 * 0.8) < 1e-12 &&
           match.rhythm_factor == 0.8);
    cand = shape_of(&n, 20, 5); /* nothing differs: an exact match, not a variation */
    assert(!standard_form_motive_match_varied(&proto, &cand, 1, &match));
    notes_destroy(&n);

    notes_init(&n); /* a motive of four notes is not varied */
    const int b[4] = {60, 62, 64, 65};
    const int c[4] = {60, 62, 64, 67};
    const double t0[4] = {0, 1, 2, 3};
    const double t1[4] = {10, 11, 12, 13};
    figure(&n, t0, b, 4);
    figure(&n, t1, c, 4);
    notes_done(&n);
    StandardFormMotiveShape p4 = shape_of(&n, 0, 4);
    StandardFormMotiveShape c4 = shape_of(&n, 4, 4);
    assert(!standard_form_motive_match_varied(&p4, &c4, 1, &match));
    notes_destroy(&n);
}

static void test_precedence_picks_the_strongest_kind(void) {
    assert(standard_form_motive_kind_rank(MOTIVE_OCCURRENCE_EXACT) < standard_form_motive_kind_rank(MOTIVE_OCCURRENCE_REAL) ||
           standard_form_motive_kind_rank(MOTIVE_OCCURRENCE_EXACT) == 0);
    assert(standard_form_motive_kind_rank(MOTIVE_OCCURRENCE_TONAL) == standard_form_motive_kind_rank(MOTIVE_OCCURRENCE_REAL));
    assert(standard_form_motive_kind_rank(MOTIVE_OCCURRENCE_REAL) < standard_form_motive_kind_rank(MOTIVE_OCCURRENCE_INVERTED));
    assert(standard_form_motive_kind_rank(MOTIVE_OCCURRENCE_INVERTED) == standard_form_motive_kind_rank(MOTIVE_OCCURRENCE_RETROGRADE));
    assert(standard_form_motive_kind_rank(MOTIVE_OCCURRENCE_RETROGRADE) < standard_form_motive_kind_rank(MOTIVE_OCCURRENCE_AUGMENTED));
    assert(standard_form_motive_kind_rank(MOTIVE_OCCURRENCE_DIMINISHED) < standard_form_motive_kind_rank(MOTIVE_OCCURRENCE_FRAGMENTED));
    assert(standard_form_motive_kind_rank(MOTIVE_OCCURRENCE_FRAGMENTED) < standard_form_motive_kind_rank(MOTIVE_OCCURRENCE_VARIED));
}

static void test_the_fewest_intervals_of_a_fragment(void) {
    assert(standard_form_motive_fragment_min_intervals(1) == 2 && standard_form_motive_fragment_min_intervals(2) == 2 &&
           standard_form_motive_fragment_min_intervals(4) == 2 && standard_form_motive_fragment_min_intervals(5) == 3 &&
           standard_form_motive_fragment_min_intervals(7) == 4);
}

/* A window is n notes from `first` that exist, hold no rest and have positive onset gaps. */
static void test_the_window_of_notes(void) {
    Notes n;
    notes_init(&n);
    const int pitches[5] = {60, 62, 64, 65, 67};
    const double joined[5] = {0, 1, 2, 3, 4};
    figure(&n, joined, pitches, 5); /* notes 0 to 4, each lasting to the next onset */
    form_note(&n.f, 10, 0.5, 60);   /* notes 5 and 6: a rest between them (the first ends at 10.5, the next starts at 11) */
    form_note(&n.f, 11, 1, 62);
    form_note(&n.f, 12, 1, 64);     /* note 7, joined to note 6 */
    n.count += 3;
    notes_done(&n);
    assert(standard_form_motive_window_ok(&n.line, 0, 5));
    assert(standard_form_motive_window_ok(&n.line, 0, 2));
    assert(standard_form_motive_window_ok(&n.line, 2, 3));
    assert(!standard_form_motive_window_ok(&n.line, 0, 1));  /* fewer than two notes */
    assert(!standard_form_motive_window_ok(&n.line, 3, 6));  /* past the last note */
    assert(!standard_form_motive_window_ok(&n.line, 0, 33)); /* more than the most a window holds */
    assert(!standard_form_motive_window_ok(&n.line, 4, 3));  /* a rest lies inside */
    notes_destroy(&n);
}

int main(void) {
    test_the_window_of_notes();
    test_a_varied_motive_changes_an_interval_or_the_rhythm();
    test_precedence_picks_the_strongest_kind();
    test_the_fewest_intervals_of_a_fragment();
    printf("test_standard_form_analysis_motive_match: ok\n");
    return 0;
}
