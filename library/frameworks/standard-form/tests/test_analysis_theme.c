#include <math.h>

#include "analysis_theme.h"
#include "form_test_util.h"
#include "hybrid_rules.h"
#include "phrase_beginning.h"
#include "semantics/phrase_ending.h"
#include "semantics/phrase_level_form.h"

/* Themes from segmented phrases: the framework's beginning, ending and pairing rules over idea similarity and the cadences. */

#define PAC CADENCE_TYPE_PERFECT_AUTHENTIC
#define HC CADENCE_TYPE_HALF

static const int A[4] = {60, 62, 64, 60};
static const int A_UP[4] = {65, 67, 69, 65}; /* A a fourth higher */
static const int B[4] = {72, 67, 71, 64};
static const int B_UP[4] = {77, 72, 76, 69}; /* B a fourth higher */
static const int C[4] = {69, 71, 72, 67};
static const int C_UP[4] = {74, 76, 77, 72}; /* C a fourth higher */

typedef struct {
    FormFixture f;
    StandardFormPhrases phrases;
    StandardFormSegmentation segmentation;
    StandardFormThemes themes;
} Run;

static void run(Run *r) {
    form_finish(&r->f);
    assert(standard_form_segment_phrases(&r->f.input, NULL, form_kernel_detect, NULL, &r->phrases) ==
           STANDARD_FORM_ANALYSIS_OK);
    assert(standard_form_segment_ideas(&r->f.input, &r->phrases, NULL, &r->segmentation) == STANDARD_FORM_ANALYSIS_OK);
    assert(standard_form_identify_themes(&r->f.input, &r->phrases, &r->segmentation, NULL, &r->themes) ==
           STANDARD_FORM_ANALYSIS_OK);
}

static void finish(Run *r) {
    standard_form_themes_destroy(&r->themes);
    standard_form_segmentation_destroy(&r->segmentation);
    standard_form_phrases_destroy(&r->phrases);
    form_destroy(&r->f);
}

/* Measures first.. from a list of patterns. */
static void lay(Run *r, int first, const int *patterns[], int count) {
    for (int i = 0; i < count; i++) {
        form_measure(&r->f, first + i, patterns[i]);
    }
}

static void test_a_sentence(void) {
    Run r;
    memset(&r, 0, sizeof(r));
    form_init(&r.f, 4, 0);
    const int *m[8] = {A, A, A_UP, A_UP, B, C, C, B}; /* presentation: A A' (a repeated basic idea); continuation: B C, C B */
    lay(&r, 1, m, 8);
    form_cadence(&r.f, "pac8", 8, 0, 4, PAC, 3, false, 0.1);
    run(&r);
    assert(r.themes.count == 1);
    const StandardFormThemeClaim *c = &r.themes.items[0].claim;
    assert(c->form == (int)PHRASE_LEVEL_FORM_SENTENCE && c->beginning == (int)PHRASE_BEGINNING_PRESENTATION &&
           c->ending == (int)PHRASE_ENDING_CONTINUATION);
    assert(c->phrase_count == 1 && c->parts_are_phrases == 0 && c->first_measure == 1 && c->last_measure == 8 &&
           c->length_measures == 8);
    assert(c->first_part_length == 4 && c->second_part_length == 4 && c->second_part_first_measure == 5);
    assert(c->cadences_usual == 1 && c->lengths_usual == 1 && c->archetypal == 0); /* no fragmentation by the nominal cut */
    assert(c->similarity_match == 1 && c->part_of_compound == 0);
    assert(c->commonality == (int)STANDARD_FORM_COMMONALITY_VERY_COMMON);
    /* the claim pattern: a relationship between the two subphrases, the interpretation on the relationship */
    const StandardFormTheme *theme = &r.themes.items[0];
    assert(strcmp(relationship_predicate_name(theme->relationship), "sentence") == 0);
    assert(strcmp(entity_id_value(relationship_source_id(theme->relationship)),
                  entity_id_value(entity_id(structure_entity(r.segmentation.subphrases[0].shell)))) == 0);
    assert(strcmp(entity_id_value(relationship_target_id(theme->relationship)),
                  entity_id_value(entity_id(structure_entity(r.segmentation.subphrases[1].shell)))) == 0);
    assert(strcmp(entity_id_value(interpretation_target_id(theme->interpretation)),
                  entity_id_value(relationship_id(theme->relationship))) == 0);
    assert(interpretation_framework_reference(theme->interpretation) != NULL);
    assert(structure_event_id_count(theme->shell) == 32);
    assert(fabs(theme->uncertainty - 0.1) < 1e-12); /* the cadence's; the similarity is a certain match */
    StandardFormThemeClaim read;
    assert(standard_form_theme_claim_read(theme->interpretation, &read) && read.form == (int)PHRASE_LEVEL_FORM_SENTENCE);
    finish(&r);
}

static void test_a_period(void) {
    Run r;
    memset(&r, 0, sizeof(r));
    form_init(&r.f, 4, 0);
    /* antecedent: A A, B B ending on a half cadence; consequent: A' A', C C ending on a perfect authentic cadence */
    const int *m[8] = {A, A, B, B, A_UP, A_UP, C, C};
    lay(&r, 1, m, 8);
    form_cadence(&r.f, "hc4", 4, 0, 4, HC, 1, false, 0.1);
    form_cadence(&r.f, "pac8", 8, 0, 4, PAC, 3, false, 0.2);
    run(&r);
    assert(r.phrases.count == 2 && r.themes.count == 1);
    const StandardFormThemeClaim *c = &r.themes.items[0].claim;
    assert(c->form == (int)PHRASE_LEVEL_FORM_PERIOD && c->beginning == (int)PHRASE_BEGINNING_ANTECEDENT &&
           c->ending == (int)PHRASE_ENDING_CONSEQUENT);
    assert(c->phrase_count == 2 && c->parts_are_phrases == 1 && c->length_measures == 8);
    assert(c->first_part_length == 4 && c->second_part_length == 4);
    assert(c->first_cadence_type == (int)HC && c->second_cadence_type == (int)PAC && c->first_strength_rank == 1 &&
           c->second_strength_rank == 3);
    assert(c->cadences_usual == 1 && c->lengths_usual == 1 && c->similarity_match == 1);
    assert(strcmp(relationship_predicate_name(r.themes.items[0].relationship), "period") == 0);
    assert(strcmp(entity_id_value(relationship_source_id(r.themes.items[0].relationship)),
                  entity_id_value(phrase_occurrence_id(r.phrases.items[0].occurrence))) == 0);
    assert(fabs(r.themes.items[0].uncertainty - 0.2) < 1e-12); /* the greater of the cadences' */
    finish(&r);
}

static void test_a_hybrid_of_an_antecedent_and_a_continuation(void) {
    Run r;
    memset(&r, 0, sizeof(r));
    form_init(&r.f, 4, 0);
    /* antecedent as before; the second phrase repeats its own first idea: no basic-then-contrasting, so a continuation */
    const int *m[8] = {A, A, B, B, B, B, B_UP, B_UP};
    lay(&r, 1, m, 8);
    form_cadence(&r.f, "hc4", 4, 0, 4, HC, 1, false, 0.1);
    form_cadence(&r.f, "pac8", 8, 0, 4, PAC, 3, false, 0.1);
    run(&r);
    assert(r.themes.count == 1);
    const StandardFormThemeClaim *c = &r.themes.items[0].claim;
    assert(c->form == (int)PHRASE_LEVEL_FORM_HYBRID && c->beginning == (int)PHRASE_BEGINNING_ANTECEDENT &&
           c->ending == (int)PHRASE_ENDING_CONTINUATION);
    assert(c->commonality == (int)STANDARD_FORM_COMMONALITY_SLIGHTLY_LESS_COMMON);
    assert(strcmp(relationship_predicate_name(r.themes.items[0].relationship), "hybrid") == 0);
    finish(&r);
}

static void test_a_one_phrase_hybrid_of_a_compound_basic_idea_and_a_continuation(void) {
    Run r;
    memset(&r, 0, sizeof(r));
    form_init(&r.f, 4, 0);
    const int *m[8] = {A, A, B, B, C, C, C, C}; /* a basic idea then a contrasting one, no cadence between, then a continuation */
    lay(&r, 1, m, 8);
    form_cadence(&r.f, "pac8", 8, 0, 4, PAC, 3, false, 0.1);
    run(&r);
    assert(r.themes.count == 1);
    const StandardFormThemeClaim *c = &r.themes.items[0].claim;
    assert(c->form == (int)PHRASE_LEVEL_FORM_HYBRID && c->beginning == (int)PHRASE_BEGINNING_COMPOUND_BASIC_IDEA &&
           c->ending == (int)PHRASE_ENDING_CONTINUATION && c->parts_are_phrases == 0);
    finish(&r);
}

static void test_a_compound_period_holds_two_sentences(void) {
    Run r;
    memset(&r, 0, sizeof(r));
    form_init(&r.f, 4, 0);
    const int *first[8] = {A, A, A_UP, A_UP, B, C, C, B};
    const int *second[8] = {C, C, C_UP, C_UP, A, B, B, A};
    lay(&r, 1, first, 8);
    lay(&r, 9, second, 8);
    form_cadence(&r.f, "hc8", 8, 0, 4, HC, 1, false, 0.1);
    form_cadence(&r.f, "pac16", 16, 0, 4, PAC, 3, false, 0.1);
    run(&r);
    assert(r.phrases.count == 2 && r.themes.count == 3);
    assert(r.themes.items[0].claim.form == (int)PHRASE_LEVEL_FORM_SENTENCE && r.themes.items[0].claim.part_of_compound == 1);
    assert(r.themes.items[1].claim.form == (int)PHRASE_LEVEL_FORM_SENTENCE && r.themes.items[1].claim.part_of_compound == 1);
    const StandardFormThemeClaim *c = &r.themes.items[2].claim;
    assert(c->form == (int)PHRASE_LEVEL_FORM_COMPOUND_PERIOD && c->phrase_count == 2 && c->length_measures == 16 &&
           c->beginning == -1 && c->ending == -1 && c->parts_are_phrases == 1);
    assert(strcmp(relationship_predicate_name(r.themes.items[2].relationship), "compound-period") == 0);
    finish(&r);
}

static void test_a_written_out_repeat_is_a_repeated_phrase(void) {
    Run r;
    memset(&r, 0, sizeof(r));
    form_init(&r.f, 4, 0);
    const int *m[8] = {A, A, B, B, A, A, B, B};
    lay(&r, 1, m, 8);
    form_cadence(&r.f, "pac4", 4, 0, 4, PAC, 3, false, 0.1);
    form_cadence(&r.f, "pac8", 8, 0, 4, PAC, 3, false, 0.1);
    run(&r);
    assert(r.themes.count == 1);
    assert(r.themes.items[0].claim.form == (int)PHRASE_LEVEL_FORM_REPEATED_PHRASE);
    assert(strcmp(relationship_predicate_name(r.themes.items[0].relationship), "repeated-phrase") == 0);
    finish(&r);
}

static void test_no_theme_is_claimed_without_one(void) {
    Run r;
    memset(&r, 0, sizeof(r));
    form_init(&r.f, 4, 0);
    const int *lone[4] = {A, A, B, B};
    lay(&r, 1, lone, 4);
    form_cadence(&r.f, "pac4", 4, 0, 4, PAC, 3, false, 0.1);
    run(&r);
    assert(r.themes.count == 0 && r.themes.items == NULL);
    finish(&r);

    /* two phrases whose second cadence is no stronger: neither a period nor a hybrid */
    memset(&r, 0, sizeof(r));
    form_init(&r.f, 4, 0);
    const int *m[8] = {A, A, B, B, C, C, B, B};
    lay(&r, 1, m, 8);
    form_cadence(&r.f, "pac4", 4, 0, 4, PAC, 3, false, 0.1);
    form_cadence(&r.f, "pac8", 8, 0, 4, PAC, 3, false, 0.1);
    run(&r);
    assert(r.themes.count == 0);
    finish(&r);

    /* a deceptive cadence is no phrase goal: the phrase runs on and has no theme of its own at four measures */
    memset(&r, 0, sizeof(r));
    form_init(&r.f, 4, 0);
    const int *n[4] = {A, A, B, B};
    lay(&r, 1, n, 4);
    form_cadence(&r.f, "deceptive4", 4, 0, 4, CADENCE_TYPE_DECEPTIVE, 0, false, 0.1);
    run(&r);
    assert(r.themes.count == 0);
    finish(&r);
}

static void test_bad_calls_are_refused(void) {
    Run r;
    memset(&r, 0, sizeof(r));
    form_init(&r.f, 4, 0);
    form_finish(&r.f);
    StandardFormThemes themes;
    StandardFormPhrases phrases = {NULL, 0};
    StandardFormSegmentation segmentation = {NULL, 0, NULL, 0};
    assert(standard_form_identify_themes(NULL, &phrases, &segmentation, NULL, &themes) ==
           STANDARD_FORM_ANALYSIS_ERR_NULL_ARGUMENT);
    StandardFormConfig bad = standard_form_config_default();
    bad.no_signal_uncertainty = 2.0;
    assert(standard_form_identify_themes(&r.f.input, &phrases, &segmentation, &bad, &themes) ==
           STANDARD_FORM_ANALYSIS_ERR_BAD_CONFIG);
    assert(standard_form_identify_themes(&r.f.input, &phrases, &segmentation, NULL, &themes) == STANDARD_FORM_ANALYSIS_OK &&
           themes.count == 0);
    form_destroy(&r.f);
}

int main(void) {
    test_a_sentence();
    test_a_period();
    test_a_hybrid_of_an_antecedent_and_a_continuation();
    test_a_one_phrase_hybrid_of_a_compound_basic_idea_and_a_continuation();
    test_a_compound_period_holds_two_sentences();
    test_a_written_out_repeat_is_a_repeated_phrase();
    test_no_theme_is_claimed_without_one();
    test_bad_calls_are_refused();
    printf("test_standard_form_analysis_theme: ok\n");
    return 0;
}
