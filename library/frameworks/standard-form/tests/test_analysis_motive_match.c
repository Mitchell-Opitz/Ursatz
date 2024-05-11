#include <math.h>

#include "analysis_motive_match.h"
#include "form_test_util.h"

/* The transformation matchers, each on shapes built from real notes: the pattern match, inversion, retrograde, augmentation and
   diminution, a varied motive, and the precedence between kinds. */

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

static const double STEADY[4] = {0, 1, 2, 3};

/* a second figure placed after a rest */
static double shifted(const double *starts, size_t i, double by) {
    return starts[i] + by;
}

static void test_a_pattern_is_exact_real_or_tonal(void) {
    Notes n;
    notes_init(&n);
    const int a[4] = {60, 62, 64, 60};
    const int same[4] = {60, 62, 64, 60};
    const int up[4] = {65, 67, 69, 65};
    const double later[4] = {shifted(STEADY, 0, 10), shifted(STEADY, 1, 10), shifted(STEADY, 2, 10), shifted(STEADY, 3, 10)};
    const double later2[4] = {shifted(STEADY, 0, 20), shifted(STEADY, 1, 20), shifted(STEADY, 2, 20), shifted(STEADY, 3, 20)};
    figure(&n, STEADY, a, 4);
    figure(&n, later, same, 4);
    figure(&n, later2, up, 4);
    notes_done(&n);
    StandardFormMotiveShape proto = shape_of(&n, 0, 4);
    StandardFormMotiveMatch match;
    assert(standard_form_motive_match_pattern(&proto, &(StandardFormMotiveShape){0}, &match) == false);
    StandardFormMotiveShape exact = shape_of(&n, 4, 4);
    assert(standard_form_motive_match_pattern(&proto, &exact, &match) && match.kind == (int)STANDARD_FORM_MOTIVE_EXACT &&
           match.score == 1.0);
    StandardFormMotiveShape real = shape_of(&n, 8, 4);
    assert(standard_form_motive_match_pattern(&proto, &real, &match) && match.kind == (int)STANDARD_FORM_MOTIVE_REAL);
    notes_destroy(&n);
}

static void test_inversion_negates_every_interval(void) {
    Notes n;
    notes_init(&n);
    const int a[4] = {60, 62, 64, 60};     /* +2 +2 -4 */
    const int inverted[4] = {72, 70, 68, 72}; /* -2 -2 +4 */
    const int not_inverted[4] = {72, 70, 69, 72};
    const double s1[4] = {10, 11, 12, 13};
    const double s2[4] = {20, 21, 22, 23};
    figure(&n, STEADY, a, 4);
    figure(&n, s1, inverted, 4);
    figure(&n, s2, not_inverted, 4);
    notes_done(&n);
    StandardFormMotiveShape proto = shape_of(&n, 0, 4);
    StandardFormMotiveMatch match;
    StandardFormMotiveShape cand = shape_of(&n, 4, 4);
    assert(standard_form_motive_match_inversion(&proto, &cand, &match) && match.kind == (int)STANDARD_FORM_MOTIVE_INVERTED &&
           match.score == 1.0 && match.mismatches == 0);
    assert(!standard_form_motive_match_pattern(&proto, &cand, &match));
    cand = shape_of(&n, 8, 4);
    assert(!standard_form_motive_match_inversion(&proto, &cand, &match));
    notes_destroy(&n);
}

static void test_retrograde_reverses_the_notes_and_the_onset_gaps(void) {
    Notes n;
    notes_init(&n);
    const double proto_starts[4] = {0, 1, 2, 4};   /* gaps 1 1 2 */
    const int a[4] = {60, 62, 65, 64};             /* +2 +3 -1 */
    const double back_starts[4] = {10, 12, 13, 14}; /* gaps 2 1 1 */
    const int back[4] = {64, 65, 62, 60};          /* +1 -3 -2: the notes reversed */
    const int wrong_rhythm[4] = {64, 65, 62, 60};
    const double wrong_starts[4] = {20, 21, 22, 24}; /* gaps 1 1 2: not reversed */
    figure(&n, proto_starts, a, 4);
    figure(&n, back_starts, back, 4);
    figure(&n, wrong_starts, wrong_rhythm, 4);
    notes_done(&n);
    StandardFormMotiveShape proto = shape_of(&n, 0, 4);
    StandardFormMotiveMatch match;
    StandardFormMotiveShape cand = shape_of(&n, 4, 4);
    assert(standard_form_motive_match_retrograde(&proto, &cand, &match) && match.kind == (int)STANDARD_FORM_MOTIVE_RETROGRADE &&
           match.score == 1.0);
    cand = shape_of(&n, 8, 4);
    assert(!standard_form_motive_match_retrograde(&proto, &cand, &match));
    notes_destroy(&n);
}

static void test_augmentation_doubles_and_diminution_halves_the_onset_gaps(void) {
    Notes n;
    notes_init(&n);
    const int a[4] = {60, 62, 64, 60};
    const int up[4] = {67, 69, 71, 67};
    const double doubled[4] = {10, 12, 14, 16};
    const double halved[4] = {20, 20.5, 21, 21.5};
    const double tripled[4] = {30, 33, 36, 39};
    figure(&n, STEADY, a, 4);
    figure(&n, doubled, up, 4);
    figure(&n, halved, a, 4);
    figure(&n, tripled, a, 4);
    notes_done(&n);
    StandardFormMotiveShape proto = shape_of(&n, 0, 4);
    StandardFormMotiveMatch match;
    StandardFormMotiveShape cand = shape_of(&n, 4, 4);
    assert(standard_form_motive_match_augmentation(&proto, &cand, &match) && match.kind == (int)STANDARD_FORM_MOTIVE_AUGMENTED &&
           match.score == 1.0);
    assert(!standard_form_motive_match_diminution(&proto, &cand, &match));
    cand = shape_of(&n, 8, 4);
    assert(standard_form_motive_match_diminution(&proto, &cand, &match) && match.kind == (int)STANDARD_FORM_MOTIVE_DIMINISHED);
    assert(!standard_form_motive_match_augmentation(&proto, &cand, &match));
    cand = shape_of(&n, 12, 4); /* by three: no such factor is matched */
    assert(!standard_form_motive_match_augmentation(&proto, &cand, &match) && !standard_form_motive_match_diminution(&proto, &cand, &match));
    assert(standard_form_motive_match_scaled(&proto, &cand, 3, 1, &match) && match.kind == (int)STANDARD_FORM_MOTIVE_AUGMENTED);
    assert(!standard_form_motive_match_scaled(&proto, &cand, 1, 1, &match) && !standard_form_motive_match_scaled(&proto, &cand, 0, 1, &match));
    notes_destroy(&n);
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
    assert(standard_form_motive_match_varied(&proto, &cand, 1, &match) && match.kind == (int)STANDARD_FORM_MOTIVE_VARIED &&
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
    Notes n;
    notes_init(&n);
    /* 60 62 60 is its own retrograde (+2 -2): a transposition of it is both a real transposition and a retrograde */
    const int a[3] = {60, 62, 60};
    const int up[3] = {65, 67, 65};
    const double s0[3] = {0, 1, 2};
    const double s1[3] = {10, 11, 12};
    figure(&n, s0, a, 3);
    figure(&n, s1, up, 3);
    notes_done(&n);
    StandardFormMotiveShape proto = shape_of(&n, 0, 3);
    StandardFormMotiveShape cand = shape_of(&n, 3, 3);
    StandardFormMotiveMatch match;
    assert(standard_form_motive_match_retrograde(&proto, &cand, &match));
    assert(standard_form_motive_match_pattern(&proto, &cand, &match));
    assert(standard_form_motive_classify(&proto, &cand, 1, &match) && match.kind == (int)STANDARD_FORM_MOTIVE_REAL);
    notes_destroy(&n);

    notes_init(&n);
    /* a rising line of equal steps: its inversion and its retrograde coincide; the inversion is listed first */
    const int rise[3] = {60, 62, 64};
    const int fall[3] = {72, 70, 68};
    figure(&n, s0, rise, 3);
    figure(&n, s1, fall, 3);
    notes_done(&n);
    proto = shape_of(&n, 0, 3);
    cand = shape_of(&n, 3, 3);
    assert(standard_form_motive_match_inversion(&proto, &cand, &match));
    assert(standard_form_motive_match_retrograde(&proto, &cand, &match));
    assert(standard_form_motive_classify(&proto, &cand, 1, &match) && match.kind == (int)STANDARD_FORM_MOTIVE_INVERTED);
    notes_destroy(&n);

    assert(standard_form_motive_kind_rank(STANDARD_FORM_MOTIVE_EXACT) < standard_form_motive_kind_rank(STANDARD_FORM_MOTIVE_REAL) ||
           standard_form_motive_kind_rank(STANDARD_FORM_MOTIVE_EXACT) == 0);
    assert(standard_form_motive_kind_rank(STANDARD_FORM_MOTIVE_TONAL) == standard_form_motive_kind_rank(STANDARD_FORM_MOTIVE_REAL));
    assert(standard_form_motive_kind_rank(STANDARD_FORM_MOTIVE_REAL) < standard_form_motive_kind_rank(STANDARD_FORM_MOTIVE_INVERTED));
    assert(standard_form_motive_kind_rank(STANDARD_FORM_MOTIVE_INVERTED) == standard_form_motive_kind_rank(STANDARD_FORM_MOTIVE_RETROGRADE));
    assert(standard_form_motive_kind_rank(STANDARD_FORM_MOTIVE_RETROGRADE) < standard_form_motive_kind_rank(STANDARD_FORM_MOTIVE_AUGMENTED));
    assert(standard_form_motive_kind_rank(STANDARD_FORM_MOTIVE_DIMINISHED) < standard_form_motive_kind_rank(STANDARD_FORM_MOTIVE_FRAGMENTED));
    assert(standard_form_motive_kind_rank(STANDARD_FORM_MOTIVE_FRAGMENTED) < standard_form_motive_kind_rank(STANDARD_FORM_MOTIVE_VARIED));
    assert(standard_form_motive_kind_rank((StandardFormMotiveKind)99) == -1 && standard_form_motive_kind_name((StandardFormMotiveKind)99) == NULL);
    assert(strcmp(standard_form_motive_kind_name(STANDARD_FORM_MOTIVE_TONAL), "tonal-transposed") == 0);
}

static void test_shapes_of_different_dimensions_or_lengths_do_not_compare(void) {
    Notes n;
    notes_init(&n);
    const int a[4] = {60, 62, 64, 60};
    const double s1[4] = {10, 11, 12, 13};
    figure(&n, STEADY, a, 4);
    figure(&n, s1, a, 4);
    notes_done(&n);
    StandardFormMotiveShape proto = shape_of(&n, 0, 4);
    StandardFormMotiveShape shorter = shape_of(&n, 4, 3);
    StandardFormMotiveShape generic = proto;
    generic.generic = true;
    StandardFormMotiveMatch match;
    assert(!standard_form_motive_match_pattern(&proto, &shorter, &match));
    assert(!standard_form_motive_match_inversion(&proto, &shorter, &match));
    assert(!standard_form_motive_match_pattern(&proto, &generic, &match));
    assert(!standard_form_motive_classify(&proto, &shorter, 1, &match));
    assert(!standard_form_motive_match_pattern(&proto, NULL, &match) && !standard_form_motive_match_pattern(&proto, &proto, NULL));
    notes_destroy(&n);
}

int main(void) {
    test_a_pattern_is_exact_real_or_tonal();
    test_inversion_negates_every_interval();
    test_retrograde_reverses_the_notes_and_the_onset_gaps();
    test_augmentation_doubles_and_diminution_halves_the_onset_gaps();
    test_a_varied_motive_changes_an_interval_or_the_rhythm();
    test_precedence_picks_the_strongest_kind();
    test_shapes_of_different_dimensions_or_lengths_do_not_compare();
    printf("test_standard_form_analysis_motive_match: ok\n");
    return 0;
}
