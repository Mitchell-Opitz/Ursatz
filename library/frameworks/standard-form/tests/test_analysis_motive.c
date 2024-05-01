#include <math.h>

#include "analysis_motive.h"
#include "form_test_util.h"

/* Motives of the soprano: windows of notes grouped by interval pattern and rhythm, the kernel's window matching as the first test,
   maximality, the kinds exact, real and tonal, and the claims and links of the output. */

#define PAC CADENCE_TYPE_PERFECT_AUTHENTIC

typedef struct {
    FormFixture f;
    int degrees[256];
    size_t degree_count;
    bool use_degrees;
} Piece;

static void piece_init(Piece *p, bool use_degrees) {
    memset(p, 0, sizeof(*p));
    form_init(&p->f, 4, 0);
    p->use_degrees = use_degrees;
}

/* A short figure: the notes one quarter each at `start` quarters, the last half a quarter (a rest after it). `degrees` may be NULL. */
static void figure(Piece *p, double start, const int *pitches, const int *degrees, size_t count) {
    for (size_t i = 0; i < count; i++) {
        form_note(&p->f, start + (double)i, i + 1 == count ? 0.5 : 1.0, pitches[i]);
        p->degrees[p->degree_count++] = degrees != NULL ? degrees[i] : 0;
    }
}

static StandardFormMotives find(Piece *p, const StandardFormConfig *config) {
    if (p->use_degrees) {
        form_degrees(&p->f, p->degrees);
    }
    form_finish(&p->f);
    StandardFormPhrases phrases = {NULL, 0};
    StandardFormSegmentation segmentation = {NULL, 0, NULL, 0};
    StandardFormMotives motives;
    assert(standard_form_find_motives(&p->f.input, &phrases, &segmentation, NULL, config, form_kernel_match_windows, NULL,
                                      &motives) == STANDARD_FORM_ANALYSIS_OK);
    return motives;
}

static void piece_destroy(Piece *p) {
    form_destroy(&p->f);
}

static void test_an_exact_repetition_is_a_motive(void) {
    Piece p;
    piece_init(&p, false);
    const int a[4] = {60, 62, 64, 60};
    figure(&p, 0.0, a, NULL, 4);
    figure(&p, 5.0, a, NULL, 4);
    StandardFormMotives m = find(&p, NULL);
    assert(m.family_count == 1); /* the three-note windows inside it are suppressed: maximality */
    const StandardFormMotiveFamily *f = &m.families[0];
    assert(f->claim.note_count == 4 && f->occurrence_count == 2 && f->claim.generic == 0 && f->claim.line == 0);
    assert(f->occurrences[0].claim.kind == (int)STANDARD_FORM_MOTIVE_EXACT && f->occurrences[0].claim.note_first == 0);
    assert(f->occurrences[1].claim.kind == (int)STANDARD_FORM_MOTIVE_EXACT && f->occurrences[1].claim.note_first == 4);
    assert(f->occurrences[1].claim.score == 1.0 && f->occurrences[1].uncertainty == 0.0 && f->occurrences[1].claim.mismatches == 0);
    assert(f->occurrences[0].claim.first_measure == 1 && fabs(f->occurrences[0].claim.first_beat - 1.0) < 1e-12);
    assert(f->occurrences[1].claim.first_measure == 2 && fabs(f->occurrences[1].claim.first_beat - 2.0) < 1e-12);
    standard_form_motives_destroy(&m);
    piece_destroy(&p);
}

static void test_a_real_transposition_keeps_the_semitone_intervals(void) {
    Piece p;
    piece_init(&p, false);
    const int a[4] = {60, 62, 64, 60};
    const int b[4] = {65, 67, 69, 65};
    figure(&p, 0.0, a, NULL, 4);
    figure(&p, 5.0, b, NULL, 4);
    StandardFormMotives m = find(&p, NULL);
    assert(m.family_count == 1 && m.families[0].occurrence_count == 2);
    assert(m.families[0].occurrences[1].claim.kind == (int)STANDARD_FORM_MOTIVE_REAL);
    standard_form_motives_destroy(&m);
    piece_destroy(&p);
}

static void test_a_tonal_transposition_matches_by_generic_intervals_only(void) {
    const int a[4] = {60, 62, 64, 65}; /* C D E F: whole, whole, half */
    const int b[4] = {62, 64, 65, 67}; /* D E F G: whole, half, whole: the same steps of the scale */
    const int da[4] = {1, 2, 3, 4};
    const int db[4] = {2, 3, 4, 5};
    Piece p;
    piece_init(&p, false); /* by semitones the four-note figures differ */
    figure(&p, 0.0, a, NULL, 4);
    figure(&p, 5.0, b, NULL, 4);
    StandardFormMotives m = find(&p, NULL);
    for (size_t i = 0; i < m.family_count; i++) {
        assert(m.families[i].claim.note_count == 3); /* only the three notes D E F they share */
    }
    standard_form_motives_destroy(&m);
    piece_destroy(&p);

    piece_init(&p, true);
    figure(&p, 0.0, a, da, 4);
    figure(&p, 5.0, b, db, 4);
    m = find(&p, NULL);
    assert(m.family_count == 1 && m.families[0].claim.note_count == 4 && m.families[0].claim.generic == 1);
    assert(m.families[0].occurrences[1].claim.kind == (int)STANDARD_FORM_MOTIVE_TONAL &&
           m.families[0].occurrences[1].claim.dimension_generic == 1);
    standard_form_motives_destroy(&m);
    piece_destroy(&p);
}

static void test_a_single_occurrence_is_no_motive(void) {
    Piece p;
    piece_init(&p, false);
    const int a[4] = {60, 62, 64, 60};
    figure(&p, 0.0, a, NULL, 4);
    StandardFormMotives m = find(&p, NULL);
    assert(m.family_count == 0 && m.families == NULL);
    standard_form_motives_destroy(&m);
    piece_destroy(&p);
}

static void test_the_rhythm_is_part_of_the_pattern(void) {
    Piece p;
    piece_init(&p, false);
    const int a[4] = {60, 62, 64, 60};
    figure(&p, 0.0, a, NULL, 4);
    /* the same pitches, but the second note lasts two quarters */
    form_note(&p.f, 5.0, 1.0, 60);
    p.degrees[p.degree_count++] = 0;
    form_note(&p.f, 6.0, 2.0, 62);
    p.degrees[p.degree_count++] = 0;
    form_note(&p.f, 8.0, 1.0, 64);
    p.degrees[p.degree_count++] = 0;
    form_note(&p.f, 9.0, 0.5, 60);
    p.degrees[p.degree_count++] = 0;
    StandardFormMotives m = find(&p, NULL);
    for (size_t i = 0; i < m.family_count; i++) {
        assert(m.families[i].claim.note_count < 4); /* no four-note motive; only the shared opening gap-free parts could match */
    }
    standard_form_motives_destroy(&m);
    piece_destroy(&p);
}

static void test_a_window_starts_on_a_beat(void) {
    Piece p;
    piece_init(&p, false);
    const int a[4] = {60, 62, 64, 60};
    figure(&p, 0.5, a, NULL, 4); /* an off-beat start */
    figure(&p, 5.5, a, NULL, 4);
    StandardFormMotives m = find(&p, NULL);
    assert(m.family_count == 0); /* no note falls on a beat, so no window is a candidate */
    standard_form_motives_destroy(&m);
    piece_destroy(&p);
}

static void test_the_config_limits_the_length(void) {
    Piece p;
    piece_init(&p, false);
    const int a[6] = {60, 62, 64, 65, 67, 69};
    figure(&p, 0.0, a, NULL, 6);
    figure(&p, 7.0, a, NULL, 6);
    StandardFormConfig config = standard_form_config_default();
    config.motive_max_notes = 4;
    StandardFormMotives m = find(&p, &config);
    for (size_t i = 0; i < m.family_count; i++) {
        assert(m.families[i].claim.note_count <= 4);
    }
    standard_form_motives_destroy(&m);
    piece_destroy(&p);
}

/* Four measures of quarter notes: A, B, A, B (so the repeating unit is two measures, with no shorter period). */
static void four_measures(Piece *p) {
    const int a[4] = {60, 62, 64, 60};
    const int b[4] = {72, 67, 71, 64};
    for (int m = 1; m <= 4; m++) {
        for (int beat = 0; beat < 4; beat++) {
            form_note(&p->f, form_measure_start(&p->f, m) + beat, 1.0, (m % 2 == 1 ? a : b)[beat]);
            p->degrees[p->degree_count++] = 0;
        }
    }
}

static void test_the_output_is_shells_interpretations_and_links(void) {
    Piece p;
    piece_init(&p, false);
    four_measures(&p);
    form_cadence(&p.f, "pac4", 4, 0, 4, PAC, 3, false, 0.1);
    form_finish(&p.f);
    StandardFormPhrases phrases;
    StandardFormSegmentation segmentation;
    assert(standard_form_segment_phrases(&p.f.input, NULL, form_kernel_detect, NULL, &phrases) == STANDARD_FORM_ANALYSIS_OK);
    assert(standard_form_segment_ideas(&p.f.input, &phrases, NULL, &segmentation) == STANDARD_FORM_ANALYSIS_OK);
    StandardFormMotives m;
    assert(standard_form_find_motives(&p.f.input, &phrases, &segmentation, NULL, NULL, form_kernel_match_windows, NULL, &m) ==
           STANDARD_FORM_ANALYSIS_OK);
    assert(m.family_count == 1);
    const StandardFormMotiveFamily *f = &m.families[0];
    assert(f->claim.note_count == 8 && f->occurrence_count == 2); /* measures 1-2 and 3-4 */
    assert(motif_event_id_count(f->motif) == 8);
    assert(strcmp(entity_id_value(interpretation_target_id(f->interpretation)), entity_id_value(motif_id(f->motif))) == 0);
    assert(interpretation_framework_reference(f->interpretation) != NULL);
    StandardFormMotiveClaim family_claim;
    assert(standard_form_motive_claim_read(f->interpretation, &family_claim) && family_claim.occurrence_count == 2);
    for (size_t j = 0; j < 2; j++) {
        const StandardFormMotiveOccurrence *o = &f->occurrences[j];
        assert(motif_occurrence_region_event_id_count(o->occurrence) == 8);
        assert(strcmp(entity_id_value(motif_occurrence_motif_id(o->occurrence)), entity_id_value(motif_id(f->motif))) == 0);
        assert(strcmp(entity_id_value(interpretation_target_id(o->interpretation)),
                      entity_id_value(motif_occurrence_id(o->occurrence))) == 0);
        StandardFormMotiveOccurrenceClaim read;
        assert(standard_form_motive_occurrence_claim_read(o->interpretation, &read) && read.occurrence == (int)j);
        assert(strcmp(relationship_predicate_name(o->family_relationship), "occurrence-of") == 0);
        assert(strcmp(entity_id_value(relationship_source_id(o->family_relationship)), entity_id_value(motif_id(f->motif))) == 0);
        assert(strcmp(entity_id_value(relationship_target_id(o->family_relationship)),
                      entity_id_value(motif_occurrence_id(o->occurrence))) == 0);
        /* each occurrence is inside one idea (two measures each) */
        assert(o->contained_relationship != NULL);
        assert(strcmp(relationship_predicate_name(o->contained_relationship), "contained-in") == 0);
        assert(strcmp(entity_id_value(relationship_source_id(o->contained_relationship)),
                      entity_id_value(motif_occurrence_id(o->occurrence))) == 0);
        assert(strcmp(entity_id_value(relationship_target_id(o->contained_relationship)),
                      entity_id_value(entity_id(structure_entity(segmentation.ideas[j].shell)))) == 0);
    }
    standard_form_motives_destroy(&m);
    standard_form_segmentation_destroy(&segmentation);
    standard_form_phrases_destroy(&phrases);
    piece_destroy(&p);
}

static void test_an_occurrence_outside_every_idea_is_contained_in_the_phrase(void) {
    Piece p;
    piece_init(&p, false);
    four_measures(&p);
    form_cadence(&p.f, "pac4", 4, 0, 4, PAC, 3, false, 0.1);
    form_finish(&p.f);
    StandardFormPhrases phrases;
    StandardFormSegmentation segmentation;
    assert(standard_form_segment_phrases(&p.f.input, NULL, form_kernel_detect, NULL, &phrases) == STANDARD_FORM_ANALYSIS_OK);
    assert(standard_form_segment_ideas(&p.f.input, &phrases, NULL, &segmentation) == STANDARD_FORM_ANALYSIS_OK);
    StandardFormConfig config = standard_form_config_default();
    config.motive_min_notes = 3;
    config.motive_max_notes = 8;
    StandardFormMotives m;
    /* with only the phrase given (no ideas) the occurrences link to the phrase */
    StandardFormSegmentation none = {NULL, 0, NULL, 0};
    assert(standard_form_find_motives(&p.f.input, &phrases, &none, NULL, &config, form_kernel_match_windows, NULL, &m) ==
           STANDARD_FORM_ANALYSIS_OK);
    assert(m.family_count >= 1);
    const StandardFormMotiveOccurrence *o = &m.families[0].occurrences[0];
    assert(o->contained_relationship != NULL &&
           strcmp(entity_id_value(relationship_target_id(o->contained_relationship)),
                  entity_id_value(phrase_occurrence_id(phrases.items[0].occurrence))) == 0);
    standard_form_motives_destroy(&m);
    standard_form_segmentation_destroy(&segmentation);
    standard_form_phrases_destroy(&phrases);
    piece_destroy(&p);
}

static void test_bad_calls_are_refused(void) {
    Piece p;
    piece_init(&p, false);
    form_finish(&p.f);
    StandardFormPhrases phrases = {NULL, 0};
    StandardFormSegmentation segmentation = {NULL, 0, NULL, 0};
    StandardFormMotives m;
    assert(standard_form_find_motives(NULL, &phrases, &segmentation, NULL, NULL, form_kernel_match_windows, NULL, &m) ==
           STANDARD_FORM_ANALYSIS_ERR_NULL_ARGUMENT);
    assert(standard_form_find_motives(&p.f.input, &phrases, &segmentation, NULL, NULL, NULL, NULL, &m) ==
           STANDARD_FORM_ANALYSIS_ERR_NO_DETECTOR);
    StandardFormConfig bad = standard_form_config_default();
    bad.motive_max_notes = 2;
    assert(standard_form_find_motives(&p.f.input, &phrases, &segmentation, NULL, &bad, form_kernel_match_windows, NULL, &m) ==
           STANDARD_FORM_ANALYSIS_ERR_BAD_CONFIG);
    assert(standard_form_find_motives(&p.f.input, &phrases, &segmentation, NULL, NULL, form_kernel_match_windows, NULL, &m) ==
               STANDARD_FORM_ANALYSIS_OK &&
           m.family_count == 0);
    assert(standard_form_input_set_degree(&p.f.input, false, 5, 1, 0) == STANDARD_FORM_ANALYSIS_ERR_BAD_INPUT);
    assert(standard_form_input_set_degree(&p.f.input, false, 0, 8, 0) == STANDARD_FORM_ANALYSIS_ERR_BAD_INPUT);
    piece_destroy(&p);
}

int main(void) {
    test_an_exact_repetition_is_a_motive();
    test_a_real_transposition_keeps_the_semitone_intervals();
    test_a_tonal_transposition_matches_by_generic_intervals_only();
    test_a_single_occurrence_is_no_motive();
    test_the_rhythm_is_part_of_the_pattern();
    test_a_window_starts_on_a_beat();
    test_the_config_limits_the_length();
    test_the_output_is_shells_interpretations_and_links();
    test_an_occurrence_outside_every_idea_is_contained_in_the_phrase();
    test_bad_calls_are_refused();
    printf("test_standard_form_analysis_motive: ok\n");
    return 0;
}
