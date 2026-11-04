#include <math.h>

#include "analysis_motive_match.h"
#include "form_test_util.h"

/* The injected relation matcher (StandardFormConfig.relation_matcher): NULL keeps the framework order, a matcher that refuses everything
   removes the pattern, inversion and retrograde matches and leaves augmentation, diminution and varied, a matcher's kind and score are
   used, and a kind it may not name is refused. */

typedef struct {
    FormFixture f;
    StandardFormMotiveLine line;
} Notes;

static void figure(Notes *n, const double *starts, const int *pitches, size_t count) {
    for (size_t i = 0; i < count; i++) {
        double duration = i + 1 < count ? starts[i + 1] - starts[i] : 0.25;
        form_note(&n->f, starts[i], duration, pitches[i]);
    }
}

static StandardFormMotiveShape shape_of(Notes *n, size_t first, size_t count) {
    StandardFormMotiveShape shape;
    assert(standard_form_motive_shape(&n->line, first, count, false, &shape));
    return shape;
}

typedef struct {
    int calls;
    bool answer;
    StandardFormMotiveKind kind;
    double score;
} Matcher;

static bool scripted(const StandardFormMotiveShape *prototype, const StandardFormMotiveShape *candidate, StandardFormMotiveKind *kind,
                     double *score, void *context) {
    (void)prototype;
    (void)candidate;
    Matcher *m = (Matcher *)context;
    m->calls++;
    *kind = m->kind;
    *score = m->score;
    return m->answer;
}

static void test_defaults_and_null(void) {
    StandardFormConfig config = standard_form_config_default();
    assert(config.relation_matcher == NULL && config.relation_context == NULL);

    Notes n;
    memset(&n, 0, sizeof(n));
    form_init(&n.f, 4, 0);
    const int a[3] = {60, 62, 64};
    const int up[3] = {65, 67, 69};
    const double s0[3] = {0, 1, 2};
    const double s1[3] = {10, 11, 12};
    figure(&n, s0, a, 3);
    figure(&n, s1, up, 3);
    form_finish(&n.f);
    assert(standard_form_motive_line_build(&n.line, &n.f.input, 0, NULL));
    StandardFormMotiveShape proto = shape_of(&n, 0, 3);
    StandardFormMotiveShape cand = shape_of(&n, 3, 3);
    StandardFormMotiveMatch plain;
    StandardFormMotiveMatch with_null;
    assert(standard_form_motive_classify(&proto, &cand, 1, &plain));
    assert(standard_form_motive_classify_with(&proto, &cand, 1, NULL, NULL, &with_null));
    assert(plain.kind == with_null.kind && plain.kind == (int)STANDARD_FORM_MOTIVE_REAL && plain.score == with_null.score);
    standard_form_motive_line_destroy(&n.line);
    form_destroy(&n.f);
}

static void test_a_refusing_matcher_removes_the_three_matchers_only(void) {
    Notes n;
    memset(&n, 0, sizeof(n));
    form_init(&n.f, 4, 0);
    const int a[3] = {60, 62, 64};
    const int up[3] = {65, 67, 69};
    const int fall[3] = {72, 70, 68};
    const double s0[3] = {0, 1, 2};
    const double s1[3] = {10, 11, 12};
    const double s2[3] = {20, 21, 22};
    const double slow[3] = {30, 32, 34}; /* the same intervals at twice the gaps: an augmentation */
    figure(&n, s0, a, 3);
    figure(&n, s1, up, 3);
    figure(&n, s2, fall, 3);
    figure(&n, slow, a, 3);
    form_finish(&n.f);
    assert(standard_form_motive_line_build(&n.line, &n.f.input, 0, NULL));
    StandardFormMotiveShape proto = shape_of(&n, 0, 3);
    StandardFormMotiveShape transposed = shape_of(&n, 3, 3);
    StandardFormMotiveShape inverted = shape_of(&n, 6, 3);
    StandardFormMotiveShape augmented = shape_of(&n, 9, 3);
    Matcher refuse = {0, false, STANDARD_FORM_MOTIVE_REAL, 1.0};
    StandardFormMotiveMatch match;

    assert(!standard_form_motive_classify_with(&proto, &transposed, 1, scripted, &refuse, &match));
    assert(!standard_form_motive_classify_with(&proto, &inverted, 1, scripted, &refuse, &match));
    assert(refuse.calls == 2);
    /* augmentation is the injected kernel matcher's: the framework alone no longer finds it */
    assert(!standard_form_motive_classify_with(&proto, &augmented, 1, scripted, &refuse, &match));
    assert(refuse.calls == 3);
    /* the framework's own order still finds the first two */
    assert(standard_form_motive_classify(&proto, &transposed, 1, &match) && match.kind == (int)STANDARD_FORM_MOTIVE_REAL);
    assert(standard_form_motive_classify(&proto, &inverted, 1, &match) && match.kind == (int)STANDARD_FORM_MOTIVE_INVERTED);
    standard_form_motive_line_destroy(&n.line);
    form_destroy(&n.f);
}

static void test_a_matcher_gives_the_kind_and_the_score(void) {
    Notes n;
    memset(&n, 0, sizeof(n));
    form_init(&n.f, 4, 0);
    const int a[3] = {60, 62, 64};
    const int b[3] = {65, 67, 70};
    const double s0[3] = {0, 1, 2};
    const double s1[3] = {10, 11, 12};
    figure(&n, s0, a, 3);
    figure(&n, s1, b, 3); /* not a transposition: the framework finds nothing */
    form_finish(&n.f);
    assert(standard_form_motive_line_build(&n.line, &n.f.input, 0, NULL));
    StandardFormMotiveShape proto = shape_of(&n, 0, 3);
    StandardFormMotiveShape cand = shape_of(&n, 3, 3);
    StandardFormMotiveMatch match;
    assert(!standard_form_motive_classify(&proto, &cand, 1, &match));

    Matcher says = {0, true, STANDARD_FORM_MOTIVE_RETROGRADE, 0.75};
    assert(standard_form_motive_classify_with(&proto, &cand, 1, scripted, &says, &match));
    assert(match.kind == (int)STANDARD_FORM_MOTIVE_RETROGRADE && fabs(match.score - 0.75) < 1e-12);
    assert(match.intervals == 2 && match.mismatches == 0 && match.fragment_part == -1);

    /* a kind the matcher does not own, or a score out of range, is refused and the later matchers run */
    Matcher bad_kind = {0, true, STANDARD_FORM_MOTIVE_AUGMENTED, 1.0};
    assert(!standard_form_motive_classify_with(&proto, &cand, 1, scripted, &bad_kind, &match));
    Matcher bad_score = {0, true, STANDARD_FORM_MOTIVE_REAL, 0.0};
    assert(!standard_form_motive_classify_with(&proto, &cand, 1, scripted, &bad_score, &match));
    standard_form_motive_line_destroy(&n.line);
    form_destroy(&n.f);
}

int main(void) {
    test_defaults_and_null();
    test_a_refusing_matcher_removes_the_three_matchers_only();
    test_a_matcher_gives_the_kind_and_the_score();
    printf("relation matcher: ok\n");
    return 0;
}
