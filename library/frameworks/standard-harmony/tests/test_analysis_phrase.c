#include "analysis_phrase.h"
#include "analysis_test_util.h"

/* Phrase model labels per segment, over caller-supplied phrase spans. */

static HarmonyPhraseModel run(Fixture *f, HarmonySegment **segments, const HarmonyPhraseSpan *spans, size_t spans_n) {
    assert(harmony_analysis_prepare(f->inputs, f->count, segments) == HARMONY_ANALYSIS_OK);
    HarmonyPhraseModel model;
    assert(harmony_analysis_phrase_model_label(*segments, f->count, spans, spans_n, &model) == HARMONY_PHRASE_MODEL_OK);
    return model;
}

static void test_model_phrase_i_iv_v_i(void) {
    Fixture f;
    fixture_init(&f);
    fixture_add(&f, 1, TRIAD_MAJOR, 3, 0, 1, 3);
    fixture_add(&f, 4, TRIAD_MAJOR, 3, 0, 4, 6);
    fixture_add(&f, 5, SEVENTH_DOMINANT, 4, 0, 5, 7);
    fixture_add(&f, 1, TRIAD_MAJOR, 3, 0, 1, 1);
    HarmonyPhraseSpan span = {0, 3, CADENCE_TYPE_PERFECT_AUTHENTIC};
    HarmonySegment *segments = NULL;
    HarmonyPhraseModel model = run(&f, &segments, &span, 1);
    assert(model.phrase_labeled[0]);
    const PhraseModelLabel expected[4] = {PHRASE_MODEL_LABEL_TONIC_BEGINNING, PHRASE_MODEL_LABEL_PREDOMINANT,
                                          PHRASE_MODEL_LABEL_DOMINANT, PHRASE_MODEL_LABEL_TONIC_ENDING};
    for (int i = 0; i < 4; i++) {
        assert(model.segments[i].has_label && model.segments[i].label == expected[i]);
        HarmonyPhraseLabelClaim claim;
        assert(harmony_phrase_label_claim_read(model.segments[i].interpretation, &claim));
        assert(claim.label == (int)expected[i] && claim.phrase_index == 0);
    }
    harmony_phrase_model_destroy(&model);
    harmony_analysis_destroy(segments);
    fixture_destroy(&f);
}

static void test_half_cadence_phrase_has_no_te(void) {
    Fixture f;
    fixture_init(&f);
    fixture_add(&f, 1, TRIAD_MAJOR, 3, 0, 1, 3);
    fixture_add(&f, 2, TRIAD_MINOR, 3, 1, 4, 6);
    fixture_add(&f, 5, TRIAD_MAJOR, 3, 0, 5, 7);
    HarmonyPhraseSpan span = {0, 2, CADENCE_TYPE_HALF};
    HarmonySegment *segments = NULL;
    HarmonyPhraseModel model = run(&f, &segments, &span, 1);
    assert(model.phrase_labeled[0]);
    assert(model.segments[0].label == PHRASE_MODEL_LABEL_TONIC_BEGINNING);
    assert(model.segments[1].label == PHRASE_MODEL_LABEL_PREDOMINANT);
    assert(model.segments[2].label == PHRASE_MODEL_LABEL_DOMINANT);
    for (int i = 0; i < 3; i++) {
        assert(model.segments[i].label != PHRASE_MODEL_LABEL_TONIC_ENDING);
    }
    harmony_phrase_model_destroy(&model);
    harmony_analysis_destroy(segments);
    fixture_destroy(&f);
}

static void test_repeated_chords_and_applied_chords(void) {
    Fixture f;
    fixture_init(&f);
    fixture_add(&f, 1, TRIAD_MAJOR, 3, 0, 1, 3); /* I, two segments */
    fixture_add(&f, 1, TRIAD_MAJOR, 3, 0, 1, 5);
    fixture_add(&f, 2, TRIAD_MINOR, 3, 0, 2, 4); /* ii */
    fixture_add(&f, 5, TRIAD_MAJOR, 3, 0, 2, 4)->applied_to = 5; /* V/V: not a phrase model chord */
    fixture_add(&f, 5, TRIAD_MAJOR, 3, 0, 5, 7); /* V */
    fixture_add(&f, 5, TRIAD_MAJOR, 3, 0, 5, 2); /* V again */
    fixture_add(&f, 1, TRIAD_MAJOR, 3, 0, 1, 1);
    HarmonyPhraseSpan span = {0, 6, CADENCE_TYPE_IMPERFECT_AUTHENTIC};
    HarmonySegment *segments = NULL;
    HarmonyPhraseModel model = run(&f, &segments, &span, 1);
    assert(model.phrase_labeled[0]);
    assert(model.segments[0].label == PHRASE_MODEL_LABEL_TONIC_BEGINNING);
    assert(model.segments[1].label == PHRASE_MODEL_LABEL_TONIC_BEGINNING); /* the same chord, both segments */
    assert(model.segments[2].label == PHRASE_MODEL_LABEL_PREDOMINANT);
    assert(!model.segments[3].has_label);                                  /* the applied chord */
    assert(model.segments[4].label == PHRASE_MODEL_LABEL_DOMINANT);
    assert(model.segments[5].label == PHRASE_MODEL_LABEL_DOMINANT);
    assert(model.segments[6].label == PHRASE_MODEL_LABEL_TONIC_ENDING);
    harmony_phrase_model_destroy(&model);
    harmony_analysis_destroy(segments);
    fixture_destroy(&f);
}

static void test_a_phrase_that_does_not_end_as_its_cadence_says_is_unlabeled(void) {
    Fixture f;
    fixture_init(&f);
    fixture_add(&f, 1, TRIAD_MAJOR, 3, 0, 1, 3);
    fixture_add(&f, 4, TRIAD_MAJOR, 3, 0, 4, 6);
    fixture_add(&f, 1, TRIAD_MAJOR, 3, 0, 1, 1); /* ends on I, but the span says half cadence */
    fixture_add(&f, 1, TRIAD_MAJOR, 3, 0, 1, 1); /* outside every span */
    HarmonyPhraseSpan span = {0, 2, CADENCE_TYPE_HALF};
    HarmonySegment *segments = NULL;
    HarmonyPhraseModel model = run(&f, &segments, &span, 1);
    assert(!model.phrase_labeled[0]);
    for (int i = 0; i < 3; i++) {
        assert(!model.segments[i].has_label && model.segments[i].phrase_index == 0);
    }
    assert(model.segments[3].phrase_index == -1);
    harmony_phrase_model_destroy(&model);

    HarmonyPhraseSpan bad[2] = {{0, 2, CADENCE_TYPE_HALF}, {2, 3, CADENCE_TYPE_HALF}}; /* overlap */
    assert(harmony_analysis_phrase_model_label(segments, f.count, bad, 2, &model) == HARMONY_PHRASE_MODEL_ERR_BAD_SPAN);
    HarmonyPhraseSpan beyond = {0, 9, CADENCE_TYPE_HALF};
    assert(harmony_analysis_phrase_model_label(segments, f.count, &beyond, 1, &model) ==
           HARMONY_PHRASE_MODEL_ERR_BAD_SPAN);
    harmony_analysis_destroy(segments);
    fixture_destroy(&f);
}

static void test_a_neighbor_six_four_does_not_break_the_dominant(void) {
    Fixture f;
    fixture_init(&f);
    fixture_add(&f, 2, TRIAD_MINOR, 3, 1, 4, 6);  /* ii6, beat 1 */
    fixture_add(&f, 5, TRIAD_MAJOR, 3, 0, 5, 7);  /* V, beat 2 */
    fixture_add(&f, 5, TRIAD_MAJOR, 3, 0, 5, 7);  /* V, beat 3 */
    fixture_add(&f, 1, TRIAD_MAJOR, 3, 2, 5, 1);  /* I6/4 over sol, beat 4: a neighbor, on a weak beat */
    fixture_add(&f, 5, TRIAD_MAJOR, 3, 0, 5, 7);  /* V, measure 2 beat 1 */
    fixture_add(&f, 1, TRIAD_MAJOR, 3, 2, 5, 1);  /* I6/4, beat 2 */
    fixture_add(&f, 5, TRIAD_MAJOR, 3, 0, 5, 7);  /* V, beat 3 */
    HarmonyPhraseSpan span = {0, 6, CADENCE_TYPE_HALF};
    HarmonySegment *segments = NULL;
    HarmonyPhraseModel model = run(&f, &segments, &span, 1);
    assert(model.phrase_labeled[0]);
    assert(model.segments[0].label == PHRASE_MODEL_LABEL_PREDOMINANT);
    for (int i = 1; i <= 6; i++) {
        if (i == 3 || i == 5) {
            assert(!model.segments[i].has_label); /* the neighbor 6/4 is part of the V */
        } else {
            assert(model.segments[i].label == PHRASE_MODEL_LABEL_DOMINANT);
        }
    }
    harmony_phrase_model_destroy(&model);
    harmony_analysis_destroy(segments);
    fixture_destroy(&f);
}

int main(void) {
    test_model_phrase_i_iv_v_i();
    test_half_cadence_phrase_has_no_te();
    test_repeated_chords_and_applied_chords();
    test_a_phrase_that_does_not_end_as_its_cadence_says_is_unlabeled();
    test_a_neighbor_six_four_does_not_break_the_dominant();
    printf("standard-harmony analysis phrase model: all tests passed\n");
    return 0;
}
