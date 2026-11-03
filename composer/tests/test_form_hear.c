#include <assert.h>
#include <stdio.h>
#include <stdlib.h>

#include <standard-form/phrase_beginning.h>
#include <standard-form/phrase_ending.h>
#include <standard-form/phrase_level_form.h>
#include <semantics/cadence_type.h>

#include <ursatz_composer/form_hear.h>
#include <ursatz_composer/lanes.h>
#include <ursatz_composer/piece_plan.h>
#include <ursatz_composer/plan_json.h>
#include <ursatz_composer/plan_state.h>

/*
 * Hearing the phrase form of a written plan. Two hand-written Realized themes are heard as what they
 * were written as: an 8-bar sentence in C major ending in a PAC, and an 8-bar period in A minor, a
 * half cadence then a PAC. A stretch with no cadence hears no theme, and a Constrained melody is not
 * heard at all. A third theme modulates (R-069): the sentence in C major whose continuation is in G major from bar 5
 * (tests/fixtures/sentence_c_to_g.json, its second half the first one's moved a fifth up, I V6/5 I V6/5 then IV ii V I
 * in G, IV being the pivot) is heard as a sentence ending in a PAC, the PAC in G: its soprano is G, the tonic of the key
 * under the last chord, which in C would be the fifth and make the cadence imperfect.
 */

static PiecePlan *load(const char *name) {
    char path[512];
    snprintf(path, sizeof(path), "%s/%s", COMPOSER_FIXTURE_DIR, name);
    PiecePlan *plan = NULL;
    PlanJsonError error;
    assert(plan_read_json_file(path, &plan, &error) == PLAN_OK);
    return plan;
}

static PlanSpan bars(long start, long length) {
    PlanSpan span;
    assert(rational_make(start, 1, &span.start) == RATIONAL_OK);
    assert(rational_make(length, 1, &span.length) == RATIONAL_OK);
    return span;
}

static void test_sentence(void) {
    PiecePlan *plan = load("sentence_c_major.json");
    FormHeard heard;
    assert(form_hear(plan, bars(0, 8), &heard) == FORM_HEAR_OK);
    assert(heard.phrase_count == 1);
    assert(heard.count == 1);
    const FormHeardTheme *theme = &heard.themes[0];
    assert(theme->form == PHRASE_LEVEL_FORM_SENTENCE);
    assert(theme->beginning == PHRASE_BEGINNING_PRESENTATION);
    assert(theme->ending == PHRASE_ENDING_CONTINUATION);
    assert(theme->first_bar == 0 && theme->bars == 8);
    assert(theme->phrase_count == 1);
    /* The presentation has no cadence of its own; the continuation ends in the PAC. */
    assert(theme->first_cadence == -1);
    assert(theme->second_cadence == CADENCE_TYPE_PERFECT_AUTHENTIC);
    form_heard_destroy(&heard);
    plan_destroy(plan);
}

static void test_period(void) {
    PiecePlan *plan = load("period_a_minor.json");
    FormHeard heard;
    /* The consequent opens on i6 after the V, which Ursatz reads as an evaded cadence unless it is told a phrase
       starts there (R-090, R-091): the start of bar 5 is given, so the V is the half cadence of the antecedent. */
    Rational consequent;
    assert(rational_make(4, 1, &consequent) == RATIONAL_OK);
    assert(form_hear_with_sections(plan, bars(0, 8), &consequent, 1, &heard) == FORM_HEAR_OK);
    assert(heard.phrase_count == 2);
    assert(heard.count == 1);
    const FormHeardTheme *theme = &heard.themes[0];
    assert(theme->form == PHRASE_LEVEL_FORM_PERIOD);
    assert(theme->beginning == PHRASE_BEGINNING_ANTECEDENT);
    assert(theme->ending == PHRASE_ENDING_CONSEQUENT);
    assert(theme->first_bar == 0 && theme->bars == 8);
    assert(theme->phrase_count == 2);
    assert(theme->first_cadence == CADENCE_TYPE_HALF);
    assert(theme->second_cadence == CADENCE_TYPE_PERFECT_AUTHENTIC);
    form_heard_destroy(&heard);
    plan_destroy(plan);
}

static void test_modulating_sentence(void) {
    PiecePlan *plan = load("sentence_c_to_g.json");
    LaneSpanId keys[3];
    assert(plan_lane_find(plan, PLAN_LANE_KEY, 0, bars(0, 8), keys, 3) == 2);
    FormHeard heard;
    assert(form_hear(plan, bars(0, 8), &heard) == FORM_HEAR_OK);
    assert(heard.phrase_count == 1);
    assert(heard.count == 1);
    const FormHeardTheme *theme = &heard.themes[0];
    assert(theme->form == PHRASE_LEVEL_FORM_SENTENCE);
    assert(theme->beginning == PHRASE_BEGINNING_PRESENTATION);
    assert(theme->ending == PHRASE_ENDING_CONTINUATION);
    assert(theme->first_bar == 0 && theme->bars == 8);
    assert(theme->first_cadence == -1);
    assert(theme->second_cadence == CADENCE_TYPE_PERFECT_AUTHENTIC);
    /* The one cadence heard arrives on the last chord, bar 8, and is perfect in G. */
    assert(heard.cadence_count == 1);
    assert(heard.cadences[0].type == CADENCE_TYPE_PERFECT_AUTHENTIC);
    assert(heard.cadences[0].arrival.num == 7 && heard.cadences[0].arrival.den == 1);
    form_heard_destroy(&heard);
    /* Heard bar by bar, each half in its own key: the first half has no cadence, the second has the PAC. */
    assert(form_hear(plan, bars(0, 4), &heard) == FORM_HEAR_OK);
    assert(heard.count == 0 && heard.cadence_count == 0);
    form_heard_destroy(&heard);
    assert(form_hear(plan, bars(4, 4), &heard) == FORM_HEAR_OK);
    assert(heard.cadence_count == 1 && heard.cadences[0].type == CADENCE_TYPE_PERFECT_AUTHENTIC);
    form_heard_destroy(&heard);
    plan_destroy(plan);
}

/* The sentence's presentation alone (I V6/5 I V6/5) has no cadence, so no theme. */
static void test_no_cadence(void) {
    PiecePlan *plan = load("sentence_c_major.json");
    FormHeard heard;
    assert(form_hear(plan, bars(0, 4), &heard) == FORM_HEAR_OK);
    assert(heard.count == 0 && heard.themes == NULL);
    form_heard_destroy(&heard);
    plan_destroy(plan);
}

static void test_constrained_melody(void) {
    PiecePlan *plan = load("sentence_c_major.json");
    LaneSpanId melody;
    assert(plan_lane_find(plan, PLAN_LANE_MELODY, 0, bars(0, 8), &melody, 1) == 1);
    assert(plan_lane_span_set_state(plan, melody, PLAN_STATE_CONSTRAINED) == PLAN_OK);
    FormHeard heard;
    assert(form_hear(plan, bars(0, 8), &heard) == FORM_HEAR_ERR_NOT_WRITTEN);
    assert(heard.count == 0 && heard.themes == NULL);
    plan_destroy(plan);
}

static void test_names_and_null(void) {
    FormHeard heard;
    assert(form_hear(NULL, bars(0, 8), &heard) == FORM_HEAR_ERR_NULL);
    assert(form_hear_result_name(FORM_HEAR_ERR_NOT_WRITTEN) != NULL);
    assert(form_hear_form_name(PHRASE_LEVEL_FORM_SENTENCE) == phrase_level_form_name(PHRASE_LEVEL_FORM_SENTENCE));
}

int main(void) {
    test_sentence();
    test_period();
    test_no_cadence();
    test_modulating_sentence();
    test_constrained_melody();
    test_names_and_null();
    return 0;
}
