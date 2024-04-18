#include <math.h>

#include "analysis_expansion.h"
#include "form_test_util.h"
#include "external_expansion_rules.h"
#include "internal_expansion_rules.h"
#include "semantics/expansion_placement.h"
#include "semantics/external_expansion_kind.h"
#include "semantics/internal_expansion_technique.h"
#include "semantics/phrase_length_change.h"
#include "semantics/phrase_level_form.h"

/* Expansion and contraction from the theme's own balance (an ending as long as its beginning), the internal technique evidence,
   and the external expansion of a phrase's extension. */

#define PAC CADENCE_TYPE_PERFECT_AUTHENTIC
#define HC CADENCE_TYPE_HALF

static const int A[4] = {60, 62, 64, 60};
static const int A_UP[4] = {65, 67, 69, 65};
static const int B[4] = {72, 67, 71, 64};
static const int C[4] = {69, 71, 72, 67};

typedef struct {
    FormFixture f;
    StandardFormPhrases phrases;
    StandardFormSegmentation segmentation;
    StandardFormThemes themes;
    StandardFormExpansions expansions;
} Run;

static void run(Run *r) {
    form_finish(&r->f);
    assert(standard_form_segment_phrases(&r->f.input, NULL, form_kernel_detect, NULL, &r->phrases) ==
           STANDARD_FORM_ANALYSIS_OK);
    assert(standard_form_segment_ideas(&r->f.input, &r->phrases, NULL, &r->segmentation) == STANDARD_FORM_ANALYSIS_OK);
    assert(standard_form_identify_themes(&r->f.input, &r->phrases, &r->segmentation, NULL, &r->themes) ==
           STANDARD_FORM_ANALYSIS_OK);
    assert(standard_form_detect_expansions(&r->f.input, &r->phrases, &r->segmentation, &r->themes, NULL, &r->expansions) ==
           STANDARD_FORM_ANALYSIS_OK);
}

static void finish(Run *r) {
    standard_form_expansions_destroy(&r->expansions);
    standard_form_themes_destroy(&r->themes);
    standard_form_segmentation_destroy(&r->segmentation);
    standard_form_phrases_destroy(&r->phrases);
    form_destroy(&r->f);
}

static void lay(Run *r, int first, const int *patterns[], int count) {
    for (int i = 0; i < count; i++) {
        form_measure(&r->f, first + i, patterns[i]);
    }
}

static void test_a_ten_measure_sentence_is_expanded_by_two(void) {
    Run r;
    memset(&r, 0, sizeof(r));
    form_init(&r.f, 4, 0);
    /* a presentation of four measures and a continuation of six: against the nominal eight */
    const int *m[10] = {A, A, A_UP, A_UP, B, C, C, B, A, B};
    lay(&r, 1, m, 10);
    form_cadence(&r.f, "pac10", 10, 0, 4, PAC, 3, false, 0.1);
    run(&r);
    assert(r.themes.count == 1 && r.expansions.count == 1);
    const StandardFormExpansionClaim *c = &r.expansions.items[0].claim;
    assert(c->kind == (int)STANDARD_FORM_EXPANSION_INTERNAL && c->change == (int)PHRASE_LENGTH_CHANGE_EXPANSION &&
           c->placement == (int)EXPANSION_PLACEMENT_INTERNAL);
    assert(c->expected_length == 4 && c->actual_length == 6 && c->measures_changed == 2);
    assert(c->first_measure == 5 && c->last_measure == 10 && c->theme == 0 && c->phrase == 0);
    assert(c->techniques == 0 && c->external_kind == -1); /* no evidence of a technique: unspecified */
    const StandardFormExpansion *e = &r.expansions.items[0];
    assert(strcmp(entity_id_value(interpretation_target_id(e->interpretation)),
                  entity_id_value(entity_id(structure_entity(e->shell)))) == 0);
    assert(interpretation_framework_reference(e->interpretation) != NULL);
    assert(structure_event_id_count(e->shell) == 24);
    assert(fabs(e->uncertainty - r.themes.items[0].uncertainty) < 1e-12);
    StandardFormExpansionClaim read;
    assert(standard_form_expansion_claim_read(e->interpretation, &read) && read.measures_changed == 2);
    finish(&r);
}

static void test_a_repeated_ending_idea_is_a_repetition(void) {
    Run r;
    memset(&r, 0, sizeof(r));
    form_init(&r.f, 4, 0);
    const int *m[10] = {A, A, A_UP, A_UP, B, C, C, B, B, C}; /* the last idea repeats the first of the ending */
    lay(&r, 1, m, 10);
    form_cadence(&r.f, "pac10", 10, 0, 4, PAC, 3, false, 0.1);
    run(&r);
    assert(r.expansions.count == 1);
    unsigned bit = standard_form_internal_technique_bit(INTERNAL_EXPANSION_TECHNIQUE_REPETITION);
    assert(((unsigned)r.expansions.items[0].claim.techniques & bit) != 0 &&
           !standard_form_internal_expansion_is_combined((unsigned)r.expansions.items[0].claim.techniques));
    finish(&r);
}

static void test_an_evaded_cadence_before_the_goal_is_one_more_time(void) {
    Run r;
    memset(&r, 0, sizeof(r));
    form_init(&r.f, 4, 0);
    const int *m[10] = {A, A, A_UP, A_UP, B, C, C, B, A, B};
    lay(&r, 1, m, 10);
    form_cadence(&r.f, "subverted7", 7, 0, 4, CADENCE_TYPE_SUBVERTED, 0, false, 0.1);
    form_cadence(&r.f, "pac10", 10, 0, 4, PAC, 3, false, 0.1);
    run(&r);
    assert(r.phrases.count == 1 && r.expansions.count == 1); /* the subverted cadence ends no phrase */
    unsigned bit = standard_form_internal_technique_bit(INTERNAL_EXPANSION_TECHNIQUE_ONE_MORE_TIME);
    assert(((unsigned)r.expansions.items[0].claim.techniques & bit) != 0);
    finish(&r);
}

static void test_a_short_continuation_is_a_contraction(void) {
    Run r;
    memset(&r, 0, sizeof(r));
    form_init(&r.f, 4, 0);
    const int *m[6] = {A, A, A_UP, A_UP, B, C};
    lay(&r, 1, m, 6);
    form_cadence(&r.f, "pac6", 6, 0, 4, PAC, 3, false, 0.1);
    run(&r);
    assert(r.themes.count == 1 && r.themes.items[0].claim.form == (int)PHRASE_LEVEL_FORM_SENTENCE);
    assert(r.expansions.count == 1);
    const StandardFormExpansionClaim *c = &r.expansions.items[0].claim;
    assert(c->kind == (int)STANDARD_FORM_EXPANSION_CONTRACTION && c->change == (int)PHRASE_LENGTH_CHANGE_CONTRACTION &&
           c->placement == (int)EXPANSION_PLACEMENT_INTERNAL);
    assert(c->expected_length == 4 && c->actual_length == 2 && c->measures_changed == 2 && c->techniques == 0);
    finish(&r);
}

static void test_a_balanced_theme_has_no_length_change(void) {
    Run r;
    memset(&r, 0, sizeof(r));
    form_init(&r.f, 4, 0);
    const int *m[8] = {A, A, A_UP, A_UP, B, C, C, B};
    lay(&r, 1, m, 8);
    form_cadence(&r.f, "pac8", 8, 0, 4, PAC, 3, false, 0.1);
    run(&r);
    assert(r.themes.count == 1 && r.expansions.count == 0 && r.expansions.items == NULL);
    finish(&r);
}

static void test_a_period_with_a_longer_consequent_is_expanded(void) {
    Run r;
    memset(&r, 0, sizeof(r));
    form_init(&r.f, 4, 0);
    const int *m[10] = {A, A, B, B, A_UP, A_UP, C, C, B, B}; /* an antecedent of four, a consequent of six */
    lay(&r, 1, m, 10);
    form_cadence(&r.f, "hc4", 4, 0, 4, HC, 1, false, 0.1);
    form_cadence(&r.f, "pac10", 10, 0, 4, PAC, 3, false, 0.1);
    run(&r);
    assert(r.themes.count == 1 && r.themes.items[0].claim.form == (int)PHRASE_LEVEL_FORM_PERIOD);
    assert(r.expansions.count == 1);
    const StandardFormExpansionClaim *c = &r.expansions.items[0].claim;
    assert(c->kind == (int)STANDARD_FORM_EXPANSION_INTERNAL && c->phrase == 1 && c->expected_length == 4 &&
           c->actual_length == 6 && c->first_measure == 5 && c->last_measure == 10);
    finish(&r);
}

static void test_a_post_cadential_extension_is_an_external_expansion(void) {
    Run r;
    memset(&r, 0, sizeof(r));
    form_init(&r.f, 4, 0);
    const int *m[10] = {A, A, A_UP, A_UP, B, C, C, B, A, A};
    lay(&r, 1, m, 10);
    form_cadence(&r.f, "pac8", 8, 0, 4, PAC, 3, false, 0.1);
    run(&r);
    assert(r.phrases.items[0].claim.extension_measures == 2 && r.expansions.count == 1);
    const StandardFormExpansionClaim *c = &r.expansions.items[0].claim;
    assert(c->kind == (int)STANDARD_FORM_EXPANSION_EXTERNAL && c->placement == (int)EXPANSION_PLACEMENT_EXTERNAL &&
           c->change == (int)PHRASE_LENGTH_CHANGE_EXPANSION);
    assert(c->external_kind == (int)EXTERNAL_EXPANSION_KIND_POST_CADENTIAL_EXTENSION && c->measures_changed == 2 &&
           c->expected_length == 8 && c->actual_length == 10 && c->first_measure == 9 && c->last_measure == 10);
    assert(c->suffix_length_typical == 1 && c->suffix_agrees_with_location == 1 && c->theme == 0 && c->techniques == 0);
    assert(structure_event_id_count(r.expansions.items[0].shell) == 8);
    finish(&r);
}

static void test_a_long_tail_is_a_codetta(void) {
    Run r;
    memset(&r, 0, sizeof(r));
    form_init(&r.f, 4, 0);
    const int *m[12] = {A, A, A_UP, A_UP, B, C, C, B, A, A, B, B};
    lay(&r, 1, m, 12);
    form_cadence(&r.f, "pac8", 8, 0, 4, PAC, 3, false, 0.1);
    run(&r);
    assert(r.expansions.count == 1);
    const StandardFormExpansionClaim *c = &r.expansions.items[0].claim;
    assert(c->external_kind == (int)EXTERNAL_EXPANSION_KIND_CODETTA && c->measures_changed == 4 &&
           c->suffix_length_typical == 1 && c->suffix_agrees_with_location == 1);
    finish(&r);
}

static void test_bad_calls_are_refused(void) {
    Run r;
    memset(&r, 0, sizeof(r));
    form_init(&r.f, 4, 0);
    form_finish(&r.f);
    StandardFormExpansions expansions;
    StandardFormPhrases phrases = {NULL, 0};
    StandardFormSegmentation segmentation = {NULL, 0, NULL, 0};
    StandardFormThemes themes = {NULL, 0};
    assert(standard_form_detect_expansions(NULL, &phrases, &segmentation, &themes, NULL, &expansions) ==
           STANDARD_FORM_ANALYSIS_ERR_NULL_ARGUMENT);
    StandardFormConfig bad = standard_form_config_default();
    bad.discontinuity_leap_semitones = 0;
    assert(standard_form_detect_expansions(&r.f.input, &phrases, &segmentation, &themes, &bad, &expansions) ==
           STANDARD_FORM_ANALYSIS_ERR_BAD_CONFIG);
    assert(standard_form_detect_expansions(&r.f.input, &phrases, &segmentation, &themes, NULL, &expansions) ==
               STANDARD_FORM_ANALYSIS_OK &&
           expansions.count == 0);
    form_destroy(&r.f);
}

int main(void) {
    test_a_ten_measure_sentence_is_expanded_by_two();
    test_a_repeated_ending_idea_is_a_repetition();
    test_an_evaded_cadence_before_the_goal_is_one_more_time();
    test_a_short_continuation_is_a_contraction();
    test_a_balanced_theme_has_no_length_change();
    test_a_period_with_a_longer_consequent_is_expanded();
    test_a_post_cadential_extension_is_an_external_expansion();
    test_a_long_tail_is_a_codetta();
    test_bad_calls_are_refused();
    printf("test_standard_form_analysis_expansion: ok\n");
    return 0;
}
