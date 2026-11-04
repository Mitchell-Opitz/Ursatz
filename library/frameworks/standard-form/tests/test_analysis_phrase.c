#include "analysis_phrase.h"
#include "core/rational.h"
#include "time/meter_arithmetic.h"
#include "time/meter_map.h"
#include "time/score_time.h"
#include "external_expansion_rules.h"
#include "form_test_util.h"
#include "interpretation/uncertainty.h"

/* Phrases from cadences: the kernel's Phrase Boundary Detection (through the bridge) fed with real cadences, post-cadential
   absorption, snapping, and the secondary signal. */

#define PAC CADENCE_TYPE_PERFECT_AUTHENTIC
#define HC CADENCE_TYPE_HALF

static const int RISING[4] = {60, 62, 64, 65};

static StandardFormPhrases segment(const FormFixture *f, const StandardFormConfig *config) {
    StandardFormPhrases phrases;
    assert(standard_form_segment_phrases(&f->input, config, chain_form_boundary_detect, NULL, &phrases) == STANDARD_FORM_ANALYSIS_OK);
    return phrases;
}

static void test_two_cadences_make_two_phrases(void) {
    FormFixture f;
    form_init(&f, 4, 0);
    form_measures(&f, 1, 8, RISING);
    form_cadence(&f, "c4", 4, 0, 4, HC, 1, false, 0.1);
    form_cadence(&f, "c8", 8, 0, 4, PAC, 3, false, 0.2);
    form_finish(&f);
    StandardFormPhrases phrases = segment(&f, NULL);
    assert(phrases.count == 2);
    const StandardFormPhraseClaim *a = &phrases.items[0].claim;
    const StandardFormPhraseClaim *b = &phrases.items[1].claim;
    assert(a->first_measure == 1 && a->end_measure == 4 && a->last_measure == 4 && a->cadence_measure == 4);
    assert(a->length_measures == 4 && a->extension_measures == 0 && a->extension_kind == -1);
    assert(a->signal == (int)STANDARD_FORM_SIGNAL_CADENCE && a->cadence_type == (int)HC && a->strength_rank == 1);
    assert(b->first_measure == 5 && b->end_measure == 8 && b->length_measures == 4 && b->cadence_type == (int)PAC);
    assert(a->note_first == 0 && a->note_end == 16 && b->note_first == 16 && b->note_end == 32);
    standard_form_phrases_destroy(&phrases);
    form_destroy(&f);
}

static void test_the_interpretation_follows_the_claim_pattern(void) {
    FormFixture f;
    form_init(&f, 4, 0);
    form_measures(&f, 1, 4, RISING);
    form_cadence(&f, "c4", 4, 0, 4, PAC, 3, false, 0.2);
    form_finish(&f);
    StandardFormPhrases phrases = segment(&f, NULL);
    assert(phrases.count == 1);
    const StandardFormPhrase *phrase = &phrases.items[0];
    assert(phrase_occurrence_region_event_id_count(phrase->occurrence) == 16);
    assert(strcmp(entity_id_value(phrase_occurrence_phrase_id(phrase->occurrence)), "standard-form-phrase-0") == 0);
    assert(strcmp(entity_id_value(interpretation_target_id(phrase->interpretation)),
                  entity_id_value(phrase_occurrence_id(phrase->occurrence))) == 0);
    assert(interpretation_framework_reference(phrase->interpretation) != NULL);
    assert(uncertainty_scalar_confidence_numerator(interpretation_uncertainty(phrase->interpretation)) == 800000UL);
    StandardFormPhraseClaim read;
    assert(standard_form_phrase_claim_read(phrase->interpretation, &read) && read.length_measures == 4);
    Rational start;
    Rational end;
    assert(score_time_span(phrase->span, &start, &end) && start.num == 0 && rational_cmp(end, rational_ratio(4, 1)) == 0);
    standard_form_phrases_destroy(&phrases);
    form_destroy(&f);
}

static void test_a_post_cadential_plagal_is_absorbed(void) {
    FormFixture f;
    form_init(&f, 4, 0);
    form_measures(&f, 1, 9, RISING);
    form_cadence(&f, "pac4", 4, 0, 4, PAC, 3, false, 0.1);
    form_cadence(&f, "plagal5", 5, 0, 4, CADENCE_TYPE_PLAGAL, 0, true, 0.1);
    form_cadence(&f, "pac9", 9, 0, 4, PAC, 3, false, 0.1);
    form_finish(&f);
    StandardFormPhrases phrases = segment(&f, NULL);
    assert(phrases.count == 2); /* the plagal starts no phrase */
    const StandardFormPhraseClaim *a = &phrases.items[0].claim;
    assert(a->first_measure == 1 && a->end_measure == 4 && a->last_measure == 5 && a->length_measures == 4);
    assert(a->extension_measures == 1 && a->extension_kind == (int)EXTERNAL_EXPANSION_KIND_POST_CADENTIAL_EXTENSION);
    const StandardFormPhraseClaim *b = &phrases.items[1].claim;
    assert(b->first_measure == 6 && b->end_measure == 9 && b->length_measures == 4);
    standard_form_phrases_destroy(&phrases);
    form_destroy(&f);
}

static void test_only_a_goal_cadence_ends_a_phrase(void) {
    FormFixture f;
    form_init(&f, 4, 0);
    form_measures(&f, 1, 8, RISING);
    form_cadence(&f, "deceptive4", 4, 0, 4, CADENCE_TYPE_DECEPTIVE, 0, false, 0.1);
    form_cadence(&f, "pac8", 8, 0, 4, PAC, 3, false, 0.1);
    form_finish(&f);
    StandardFormPhrases phrases = segment(&f, NULL);
    assert(phrases.count == 1 && phrases.items[0].claim.end_measure == 8 && phrases.items[0].claim.length_measures == 8);
    standard_form_phrases_destroy(&phrases);
    assert(standard_form_cadence_ends_phrase(CADENCE_TYPE_AUTHENTIC_UNSPECIFIED) && !standard_form_cadence_ends_phrase(CADENCE_TYPE_PLAGAL));
    form_destroy(&f);
}

static void test_without_a_cadence_the_secondary_signal_applies_after_eight_measures(void) {
    FormFixture f;
    form_init(&f, 4, 0);
    form_measures(&f, 1, 12, RISING);
    form_finish(&f);
    StandardFormPhrases phrases = segment(&f, NULL);
    assert(phrases.count == 1 && phrases.items[0].claim.signal == (int)STANDARD_FORM_SIGNAL_NONE);
    standard_form_phrases_destroy(&phrases);
    form_destroy(&f);

    /* a rest at the start of measure 4 after measure 3: a discontinuity, but only 3 points in, so no boundary */
    form_init(&f, 4, 0);
    const int rest_first[4] = {0, 62, 64, 65};
    form_measures(&f, 1, 3, RISING);
    form_measure(&f, 4, rest_first);
    form_measures(&f, 5, 12, RISING);
    form_finish(&f);
    phrases = segment(&f, NULL);
    assert(phrases.count == 1);
    standard_form_phrases_destroy(&phrases);
    form_destroy(&f);

    /* the same break after measure 9 (9 points in): the kernel counts it, so a phrase ends at measure 9 */
    form_init(&f, 4, 0);
    form_measures(&f, 1, 9, RISING);
    form_measure(&f, 10, rest_first);
    form_measures(&f, 11, 12, RISING);
    form_finish(&f);
    phrases = segment(&f, NULL);
    assert(phrases.count == 2);
    assert(phrases.items[0].claim.signal == (int)STANDARD_FORM_SIGNAL_DISCONTINUITY && phrases.items[0].claim.end_measure == 9);
    assert(phrases.items[0].claim.cadence_index == -1 && phrases.items[0].claim.cadence_type == -1);
    assert(phrases.items[0].uncertainty > 0.24 && phrases.items[0].uncertainty < 0.26);
    assert(phrases.items[1].claim.first_measure == 10 && phrases.items[1].claim.last_measure == 12 &&
           phrases.items[1].claim.signal == (int)STANDARD_FORM_SIGNAL_NONE);
    assert(phrases.items[1].uncertainty == standard_form_config_default().no_signal_uncertainty);
    standard_form_phrases_destroy(&phrases);
    form_destroy(&f);
}

static void test_a_leap_into_a_measure_is_a_discontinuity(void) {
    FormFixture f;
    form_init(&f, 4, 0);
    const int low[4] = {60, 62, 64, 65};
    const int high[4] = {77, 79, 81, 82}; /* a leap of 12 semitones from the previous measure's last note */
    form_measures(&f, 1, 9, low);
    form_measures(&f, 10, 12, high);
    form_finish(&f);
    StandardFormPhrases phrases = segment(&f, NULL);
    assert(phrases.count == 2 && phrases.items[0].claim.end_measure == 9 &&
           phrases.items[0].claim.signal == (int)STANDARD_FORM_SIGNAL_DISCONTINUITY);
    standard_form_phrases_destroy(&phrases);
    StandardFormConfig config = standard_form_config_default();
    config.max_length_without_cadence = 12; /* the limit is the caller's */
    phrases = segment(&f, &config);
    assert(phrases.count == 1);
    standard_form_phrases_destroy(&phrases);
    form_destroy(&f);
}

static void test_a_break_shortly_before_a_cadence_is_inside_the_phrase(void) {
    FormFixture f;
    form_init(&f, 4, 0);
    const int low[4] = {60, 62, 64, 65};
    const int high[4] = {77, 79, 81, 82};
    form_measures(&f, 1, 9, low);
    form_measures(&f, 10, 11, high); /* a leap into measure 10, and a cadence at measure 11 */
    form_cadence(&f, "pac11", 11, 0, 4, PAC, 3, false, 0.1);
    form_finish(&f);
    StandardFormPhrases phrases = segment(&f, NULL);
    assert(phrases.count == 1 && phrases.items[0].claim.end_measure == 11 && phrases.items[0].claim.length_measures == 11);
    assert(phrases.items[0].claim.signal == (int)STANDARD_FORM_SIGNAL_CADENCE);
    standard_form_phrases_destroy(&phrases);
    StandardFormConfig config = standard_form_config_default();
    config.cadence_lookahead_measures = 0; /* report every break: the kernel ends a phrase at measure 9 */
    phrases = segment(&f, &config);
    assert(phrases.count == 2 && phrases.items[0].claim.end_measure == 9 &&
           phrases.items[0].claim.signal == (int)STANDARD_FORM_SIGNAL_DISCONTINUITY);
    standard_form_phrases_destroy(&phrases);
    form_destroy(&f);
}

static void test_a_tail_after_the_last_cadence(void) {
    FormFixture f;
    form_init(&f, 4, 0);
    form_measures(&f, 1, 10, RISING);
    form_cadence(&f, "pac8", 8, 0, 4, PAC, 3, false, 0.1);
    form_finish(&f);
    StandardFormPhrases phrases = segment(&f, NULL);
    assert(phrases.count == 1);
    const StandardFormPhraseClaim *c = &phrases.items[0].claim;
    assert(c->end_measure == 8 && c->last_measure == 10 && c->extension_measures == 2 &&
           c->extension_kind == (int)EXTERNAL_EXPANSION_KIND_POST_CADENTIAL_EXTENSION);
    standard_form_phrases_destroy(&phrases);
    form_destroy(&f);

    form_init(&f, 4, 0);
    form_measures(&f, 1, 12, RISING);
    form_cadence(&f, "pac8", 8, 0, 4, PAC, 3, false, 0.1);
    form_finish(&f);
    phrases = segment(&f, NULL);
    assert(phrases.count == 1 && phrases.items[0].claim.extension_measures == 4 &&
           phrases.items[0].claim.extension_kind == (int)EXTERNAL_EXPANSION_KIND_CODETTA);
    standard_form_phrases_destroy(&phrases);
    form_destroy(&f);

    form_init(&f, 4, 0);
    form_measures(&f, 1, 18, RISING);
    form_cadence(&f, "pac4", 4, 0, 4, PAC, 3, false, 0.1);
    form_finish(&f);
    phrases = segment(&f, NULL); /* a tail of 14 measures is longer than the limit: a phrase of its own */
    assert(phrases.count == 2 && phrases.items[1].claim.first_measure == 5 && phrases.items[1].claim.last_measure == 18 &&
           phrases.items[1].claim.signal == (int)STANDARD_FORM_SIGNAL_NONE);
    standard_form_phrases_destroy(&phrases);
    form_destroy(&f);
}

static void test_the_phrase_ends_at_the_next_beat_after_the_arrival(void) {
    FormFixture f;
    form_init(&f, 4, 0);
    form_measures(&f, 1, 4, RISING);
    /* the arrival chord sounds from the downbeat of measure 4 for one and a half beats: it ends mid-beat 2 and snaps to beat 3 */
    form_cadence(&f, "pac4", 4, 0, 1.5, PAC, 3, false, 0.1);
    form_finish(&f);
    StandardFormPhrases phrases = segment(&f, NULL);
    assert(phrases.count >= 1);
    Rational end;
    assert(score_time_span(phrases.items[0].span, NULL, &end));
    assert(rational_cmp(end, rational_ratio(14, 4)) == 0); /* 12 quarters to measure 4's start, plus 2 */
    assert(phrases.items[0].claim.end_measure == 4);
    standard_form_phrases_destroy(&phrases);
    form_destroy(&f);
}

static void test_a_pickup_measure_is_not_counted(void) {
    FormFixture f;
    form_init(&f, 4, 1);
    form_note(&f, 0.0, 1.0, 59); /* the pickup: one quarter note */
    form_measures(&f, 1, 4, RISING);
    form_cadence(&f, "pac4", 4, 0, 4, PAC, 3, false, 0.1);
    form_finish(&f);
    assert(meter_map_first_counted_measure(f.map) == 1);
    StandardFormPhrases phrases = segment(&f, NULL);
    assert(phrases.count == 1);
    const StandardFormPhraseClaim *c = &phrases.items[0].claim;
    assert(c->first_measure == 0 && c->counted_first == 1 && c->end_measure == 4 && c->length_measures == 4);
    assert(c->note_first == 0 && c->note_end == 17);
    standard_form_phrases_destroy(&phrases);
    form_destroy(&f);
}

static void test_bad_calls_are_refused(void) {
    FormFixture f;
    form_init(&f, 4, 0);
    form_measures(&f, 1, 4, RISING);
    StandardFormPhrases phrases;
    assert(standard_form_segment_phrases(&f.input, NULL, chain_form_boundary_detect, NULL, &phrases) ==
           STANDARD_FORM_ANALYSIS_ERR_BAD_CONFIG); /* not finished */
    form_finish(&f);
    assert(standard_form_segment_phrases(&f.input, NULL, NULL, NULL, &phrases) == STANDARD_FORM_ANALYSIS_ERR_NO_DETECTOR);
    StandardFormConfig bad = standard_form_config_default();
    bad.similarity_min = 0.0;
    assert(standard_form_segment_phrases(&f.input, &bad, chain_form_boundary_detect, NULL, &phrases) ==
           STANDARD_FORM_ANALYSIS_ERR_BAD_CONFIG);
    assert(standard_form_segment_phrases(NULL, NULL, chain_form_boundary_detect, NULL, &phrases) ==
           STANDARD_FORM_ANALYSIS_ERR_NULL_ARGUMENT);
    assert(standard_form_input_add_cadence(&f.input, "x", NULL, PAC, 3, false, 0.1) == STANDARD_FORM_ANALYSIS_ERR_BAD_INPUT);
    assert(standard_form_input_add_cadence(&f.input, "x", f.note_spans[0], PAC, 3, false, 1.5) ==
           STANDARD_FORM_ANALYSIS_ERR_BAD_INPUT);
    form_destroy(&f);

    FormFixture empty;
    form_init(&empty, 4, 0);
    form_finish(&empty);
    assert(standard_form_segment_phrases(&empty.input, NULL, chain_form_boundary_detect, NULL, &phrases) == STANDARD_FORM_ANALYSIS_OK);
    assert(phrases.count == 0 && phrases.items == NULL);
    form_destroy(&empty);
}

int main(void) {
    test_two_cadences_make_two_phrases();
    test_the_interpretation_follows_the_claim_pattern();
    test_a_post_cadential_plagal_is_absorbed();
    test_only_a_goal_cadence_ends_a_phrase();
    test_without_a_cadence_the_secondary_signal_applies_after_eight_measures();
    test_a_leap_into_a_measure_is_a_discontinuity();
    test_a_break_shortly_before_a_cadence_is_inside_the_phrase();
    test_a_tail_after_the_last_cadence();
    test_the_phrase_ends_at_the_next_beat_after_the_arrival();
    test_a_pickup_measure_is_not_counted();
    test_bad_calls_are_refused();
    printf("test_standard_form_analysis_phrase: ok\n");
    return 0;
}
