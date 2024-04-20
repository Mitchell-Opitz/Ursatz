#include <math.h>

#include "analysis_similarity.h"
#include "form_test_util.h"

/* Idea similarity: intervals (transposition allowed) and onset rhythm, over the longer idea's intervals. */

static const StandardFormNote *notes_from(const FormFixture *f, size_t first) {
    return &f->input.notes[first];
}

/* One idea of quarter notes with the given pitches starting at `start` quarters. */
static void idea(FormFixture *f, double start, const int *pitches, size_t count) {
    for (size_t i = 0; i < count; i++) {
        form_note(f, start + (double)i, 1.0, pitches[i]);
    }
}

static void test_a_transposed_repetition_matches(void) {
    FormFixture f;
    form_init(&f, 4, 0);
    const int a[4] = {60, 62, 64, 60};
    const int b[4] = {65, 67, 69, 65}; /* a fourth higher: the same intervals */
    idea(&f, 0.0, a, 4);
    idea(&f, 4.0, b, 4);
    form_finish(&f);
    StandardFormSimilarity s;
    assert(standard_form_idea_similarity(notes_from(&f, 0), 4, notes_from(&f, 4), 4, 0.75, &s));
    assert(s.comparable && s.positions == 3 && s.matched == 3 && s.match && fabs(s.similarity - 1.0) < 1e-12);
    assert(fabs(s.margin - 1.0) < 1e-12 && fabs(standard_form_similarity_uncertainty(&s)) < 1e-12);
    form_destroy(&f);
}

static void test_a_contrasting_idea_does_not_match(void) {
    FormFixture f;
    form_init(&f, 4, 0);
    const int a[4] = {60, 62, 64, 60};
    const int b[4] = {60, 67, 59, 72};
    idea(&f, 0.0, a, 4);
    idea(&f, 4.0, b, 4);
    form_finish(&f);
    StandardFormSimilarity s;
    assert(standard_form_idea_similarity(notes_from(&f, 0), 4, notes_from(&f, 4), 4, 0.75, &s));
    assert(s.comparable && s.matched == 0 && !s.match && fabs(s.margin - 1.0) < 1e-12);
    form_destroy(&f);
}

static void test_the_threshold_is_the_configured_share(void) {
    FormFixture f;
    form_init(&f, 4, 0);
    const int a[5] = {60, 62, 64, 65, 67};
    const int b[5] = {60, 62, 64, 65, 72}; /* three of four intervals equal */
    idea(&f, 0.0, a, 5);
    idea(&f, 8.0, b, 5);
    form_finish(&f);
    StandardFormSimilarity s;
    assert(standard_form_idea_similarity(notes_from(&f, 0), 5, notes_from(&f, 5), 5, 0.75, &s));
    assert(s.matched == 3 && s.positions == 4 && s.match && fabs(s.margin) < 1e-12);
    assert(fabs(standard_form_similarity_uncertainty(&s) - 0.5) < 1e-12);
    assert(standard_form_idea_similarity(notes_from(&f, 0), 5, notes_from(&f, 5), 5, 0.9, &s));
    assert(!s.match && fabs(s.margin - (0.9 - 0.75) / 0.9) < 1e-12);
    form_destroy(&f);
}

static void test_a_different_rhythm_does_not_match(void) {
    FormFixture f;
    form_init(&f, 4, 0);
    form_note(&f, 0.0, 1.0, 60);
    form_note(&f, 1.0, 1.0, 62);
    form_note(&f, 2.0, 1.0, 64);
    form_note(&f, 4.0, 1.0, 65);
    form_note(&f, 5.0, 2.0, 67); /* the same intervals; the first one lasts 1 and so does the second... */
    form_note(&f, 7.0, 1.0, 69);
    form_finish(&f);
    StandardFormSimilarity s;
    /* ideas: [60 62 64] and [65 67 69] have equal intervals, but the second note's onset gap is 2 against 1 */
    assert(standard_form_idea_similarity(notes_from(&f, 0), 3, notes_from(&f, 3), 3, 0.75, &s));
    assert(s.positions == 2 && s.matched == 1 && !s.match);
    form_destroy(&f);
}

static void test_a_longer_idea_leaves_positions_unmatched(void) {
    FormFixture f;
    form_init(&f, 4, 0);
    const int a[3] = {60, 62, 64};
    const int b[5] = {60, 62, 64, 65, 67};
    idea(&f, 0.0, a, 3);
    idea(&f, 4.0, b, 5);
    form_finish(&f);
    StandardFormSimilarity s;
    assert(standard_form_idea_similarity(notes_from(&f, 0), 3, notes_from(&f, 3), 5, 0.75, &s));
    assert(s.positions == 4 && s.matched == 2 && !s.match && fabs(s.similarity - 0.5) < 1e-12);
    form_destroy(&f);
}

static void test_ideas_with_no_interval_are_not_comparable(void) {
    FormFixture f;
    form_init(&f, 4, 0);
    const int a[2] = {60, 62};
    idea(&f, 0.0, a, 2);
    form_finish(&f);
    StandardFormSimilarity s;
    assert(standard_form_idea_similarity(notes_from(&f, 0), 1, notes_from(&f, 0), 2, 0.75, &s));
    assert(!s.comparable && !s.match && s.similarity == 0.0 && s.margin == 0.0);
    assert(fabs(standard_form_similarity_uncertainty(&s) - 0.5) < 1e-12);
    assert(!standard_form_idea_similarity(notes_from(&f, 0), 2, notes_from(&f, 0), 2, 0.0, &s));
    assert(!standard_form_idea_similarity(NULL, 2, notes_from(&f, 0), 2, 0.75, &s));
    assert(!standard_form_idea_similarity(notes_from(&f, 0), 2, notes_from(&f, 0), 2, 0.75, NULL));
    form_destroy(&f);
}

int main(void) {
    test_a_transposed_repetition_matches();
    test_a_contrasting_idea_does_not_match();
    test_the_threshold_is_the_configured_share();
    test_a_different_rhythm_does_not_match();
    test_a_longer_idea_leaves_positions_unmatched();
    test_ideas_with_no_interval_are_not_comparable();
    printf("test_standard_form_analysis_similarity: ok\n");
    return 0;
}
