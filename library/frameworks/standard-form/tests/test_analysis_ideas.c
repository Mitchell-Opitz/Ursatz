#include <math.h>

#include "analysis_ideas.h"
#include "form_test_util.h"
#include "semantics/idea_label.h"

/* Ideas and subphrases of each phrase by standard-form's length conventions, labelled by similarity to the basic idea. */

#define PAC CADENCE_TYPE_PERFECT_AUTHENTIC

static const int BASIC[4] = {60, 62, 64, 60};
static const int BASIC_UP[4] = {65, 67, 69, 65};   /* the basic idea a fourth higher: a repetition */
static const int OTHER[4] = {72, 67, 71, 64};      /* a contrast */

/* A phrase of `measures` measures (4/4, one note a beat) ending with a PAC at its last measure. */
static void phrase_of(FormFixture *f, int measures, const int *pattern_for_measure[]) {
    for (int m = 1; m <= measures; m++) {
        form_measure(f, m, pattern_for_measure[m - 1]);
    }
    form_cadence(f, "pac", measures, 0, 4, PAC, 3, false, 0.1);
    form_finish(f);
}

static StandardFormSegmentation analyze(const FormFixture *f, StandardFormPhrases *phrases) {
    assert(standard_form_segment_phrases(&f->input, NULL, form_kernel_detect, NULL, phrases) == STANDARD_FORM_ANALYSIS_OK);
    StandardFormSegmentation segmentation;
    assert(standard_form_segment_ideas(&f->input, phrases, NULL, &segmentation) == STANDARD_FORM_ANALYSIS_OK);
    return segmentation;
}

static void test_an_eight_measure_phrase_has_four_ideas_and_two_subphrases(void) {
    FormFixture f;
    form_init(&f, 4, 0);
    /* idea 1: measures 1-2 = BASIC then a measure of its second half; build ideas of two measures: A (m1-2), A' (m3-4), B, B */
    const int *measures[8] = {BASIC, BASIC, BASIC_UP, BASIC_UP, OTHER, BASIC, OTHER, BASIC};
    phrase_of(&f, 8, measures);
    StandardFormPhrases phrases;
    StandardFormSegmentation s = analyze(&f, &phrases);
    assert(phrases.count == 1 && s.idea_count == 4 && s.subphrase_count == 2);
    for (size_t i = 0; i < 4; i++) {
        assert(s.ideas[i].claim.length_measures == 2 && s.ideas[i].claim.first_measure == 1 + 2 * (int)i);
        assert(s.ideas[i].shell != NULL && structure_event_id_count(s.ideas[i].shell) == 8);
    }
    assert(s.ideas[0].claim.label == (int)IDEA_LABEL_BASIC_IDEA && s.ideas[0].claim.match == -1);
    assert(s.ideas[1].claim.label == (int)IDEA_LABEL_BASIC_IDEA && s.ideas[1].claim.match == 1 &&
           s.ideas[1].claim.similarity == 1.0); /* a transposed repetition */
    assert(s.ideas[2].claim.label == (int)IDEA_LABEL_CONTRASTING_IDEA && s.ideas[2].claim.match == 0);
    assert(s.subphrases[0].claim.first_idea == 0 && s.subphrases[0].claim.idea_count == 2 &&
           s.subphrases[0].claim.length_measures == 4 && s.subphrases[0].claim.first_measure == 1 &&
           s.subphrases[0].claim.last_measure == 4);
    assert(s.subphrases[1].claim.first_idea == 2 && s.subphrases[1].claim.idea_count == 2 &&
           s.subphrases[1].claim.first_measure == 5 && s.subphrases[1].claim.last_measure == 8);
    size_t first;
    size_t count;
    assert(standard_form_phrase_ideas(&s, 0, &first, &count) && first == 0 && count == 4);
    assert(!standard_form_phrase_ideas(&s, 3, &first, &count));
    standard_form_segmentation_destroy(&s);
    standard_form_phrases_destroy(&phrases);
    form_destroy(&f);
}

static void test_the_interpretation_follows_the_claim_pattern(void) {
    FormFixture f;
    form_init(&f, 4, 0);
    const int *measures[4] = {BASIC, BASIC, OTHER, OTHER};
    phrase_of(&f, 4, measures);
    StandardFormPhrases phrases;
    StandardFormSegmentation s = analyze(&f, &phrases);
    assert(s.idea_count == 2 && s.subphrase_count == 0 && s.subphrases == NULL);
    const StandardFormIdea *idea = &s.ideas[1];
    assert(strcmp(entity_id_value(interpretation_target_id(idea->interpretation)),
                  entity_id_value(entity_id(structure_entity(idea->shell)))) == 0);
    assert(interpretation_framework_reference(idea->interpretation) != NULL);
    StandardFormIdeaClaim read;
    assert(standard_form_idea_claim_read(idea->interpretation, &read) && read.index == 1 && read.phrase == 0);
    /* u = 1 - (1 - 0.1)(1 - 0.5 * (1 - margin)); the contrast has margin 1, so u is the phrase's */
    assert(fabs(idea->uncertainty - 0.1) < 1e-12);
    assert(uncertainty_scalar_confidence_numerator(interpretation_uncertainty(idea->interpretation)) == 900000UL);
    standard_form_segmentation_destroy(&s);
    standard_form_phrases_destroy(&phrases);
    form_destroy(&f);
}

static void test_a_six_measure_phrase_has_a_one_idea_ending(void) {
    FormFixture f;
    form_init(&f, 4, 0);
    const int *measures[6] = {BASIC, BASIC, BASIC, BASIC, OTHER, OTHER};
    phrase_of(&f, 6, measures);
    StandardFormPhrases phrases;
    StandardFormSegmentation s = analyze(&f, &phrases);
    assert(s.idea_count == 3 && s.subphrase_count == 2);
    assert(s.subphrases[0].claim.length_measures == 4 && s.subphrases[1].claim.length_measures == 2 &&
           s.subphrases[1].claim.idea_count == 1);
    standard_form_segmentation_destroy(&s);
    standard_form_phrases_destroy(&phrases);
    form_destroy(&f);
}

static void test_an_odd_length_gives_the_last_idea_the_remainder(void) {
    FormFixture f;
    form_init(&f, 4, 0);
    const int *measures[5] = {BASIC, BASIC, BASIC, OTHER, OTHER};
    phrase_of(&f, 5, measures);
    StandardFormPhrases phrases;
    StandardFormSegmentation s = analyze(&f, &phrases);
    assert(s.idea_count == 2 && s.ideas[0].claim.length_measures == 2 && s.ideas[1].claim.length_measures == 3);
    assert(s.ideas[1].claim.first_measure == 3 && s.ideas[1].claim.last_measure == 5);
    /* the odd length adds the penalty 0.2 to the phrase's 0.1: 1 - 0.9 * 0.8 */
    assert(fabs(s.ideas[0].uncertainty - (1.0 - 0.9 * 0.8)) < 1e-12);
    standard_form_segmentation_destroy(&s);
    standard_form_phrases_destroy(&phrases);
    form_destroy(&f);
}

static void test_the_pickup_belongs_to_the_first_idea_and_is_not_counted(void) {
    FormFixture f;
    form_init(&f, 4, 1);
    form_note(&f, 0.0, 1.0, 59);
    const int *measures[4] = {BASIC, BASIC, OTHER, OTHER};
    for (int m = 1; m <= 4; m++) {
        form_measure(&f, m, measures[m - 1]);
    }
    form_cadence(&f, "pac", 4, 0, 4, PAC, 3, false, 0.1);
    form_finish(&f);
    StandardFormPhrases phrases;
    StandardFormSegmentation s = analyze(&f, &phrases);
    assert(s.idea_count == 2);
    assert(s.ideas[0].claim.first_measure == 0 && s.ideas[0].claim.last_measure == 2 && s.ideas[0].claim.length_measures == 2);
    assert(s.ideas[0].claim.note_first == 0 && s.ideas[0].claim.note_end == 9); /* the pickup note and eight more */
    assert(s.ideas[1].claim.first_measure == 3 && s.ideas[1].claim.last_measure == 4);
    standard_form_segmentation_destroy(&s);
    standard_form_phrases_destroy(&phrases);
    form_destroy(&f);
}

static void test_an_extension_belongs_to_no_idea(void) {
    FormFixture f;
    form_init(&f, 4, 0);
    const int *measures[4] = {BASIC, BASIC, OTHER, OTHER};
    for (int m = 1; m <= 4; m++) {
        form_measure(&f, m, measures[m - 1]);
    }
    form_measure(&f, 5, BASIC); /* a measure after the cadence: an extension */
    form_cadence(&f, "pac", 4, 0, 4, PAC, 3, false, 0.1);
    form_finish(&f);
    StandardFormPhrases phrases;
    StandardFormSegmentation s = analyze(&f, &phrases);
    assert(phrases.items[0].claim.extension_measures == 1 && s.idea_count == 2);
    assert(s.ideas[1].claim.last_measure == 4 && s.ideas[1].claim.note_end == 16);
    standard_form_segmentation_destroy(&s);
    standard_form_phrases_destroy(&phrases);
    form_destroy(&f);
}

static void test_bad_calls_are_refused(void) {
    FormFixture f;
    form_init(&f, 4, 0);
    form_finish(&f);
    StandardFormPhrases phrases = {NULL, 0};
    StandardFormSegmentation s;
    assert(standard_form_segment_ideas(NULL, &phrases, NULL, &s) == STANDARD_FORM_ANALYSIS_ERR_NULL_ARGUMENT);
    StandardFormConfig bad = standard_form_config_default();
    bad.similarity_min = 2.0;
    assert(standard_form_segment_ideas(&f.input, &phrases, &bad, &s) == STANDARD_FORM_ANALYSIS_ERR_BAD_CONFIG);
    assert(standard_form_segment_ideas(&f.input, &phrases, NULL, &s) == STANDARD_FORM_ANALYSIS_OK && s.idea_count == 0);
    form_destroy(&f);
}

int main(void) {
    test_an_eight_measure_phrase_has_four_ideas_and_two_subphrases();
    test_the_interpretation_follows_the_claim_pattern();
    test_a_six_measure_phrase_has_a_one_idea_ending();
    test_an_odd_length_gives_the_last_idea_the_remainder();
    test_the_pickup_belongs_to_the_first_idea_and_is_not_counted();
    test_an_extension_belongs_to_no_idea();
    test_bad_calls_are_refused();
    printf("test_standard_form_analysis_ideas: ok\n");
    return 0;
}
