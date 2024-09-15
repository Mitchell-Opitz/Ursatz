#include "galant_analysis_test_util.h"

#define S BEAT_STRENGTH_STRONG
#define M BEAT_STRENGTH_MEDIUM
#define W BEAT_STRENGTH_WEAK

static void test_degree_arithmetic(void) {
    /* in C major, a passage in A minor has the tonic 6 0; its lowered third is the C of the home key */
    assert(galant_degree_equal(galant_degree_in_home(deg(3, -1), deg(6, 0)), deg(1, 0)));
    assert(galant_degree_equal(galant_degree_in_home(deg(7, 0), deg(2, 0)), deg(1, 1))); /* C-sharp: the leading tone of D */
    assert(galant_degree_equal(galant_degree_in_home(deg(3, -1), deg(2, 0)), deg(4, 0))); /* F: the lowered third of D */
    assert(galant_degree_equal(galant_degree_in_frame(deg(1, 0), deg(5, 0), deg(1, 0)), deg(5, 0)));
    assert(galant_degree_equal(galant_degree_minus(deg(1, 0), deg(2, 0)), deg(7, -1))); /* C against D: the lowered seventh */
    assert(galant_degree_equal(galant_degree_minus(deg(1, 0), deg(5, 0)), deg(4, 0)));
    assert(galant_degree_equal(galant_degree_plus(deg(5, 0), deg(5, 0)), deg(2, 0)));
}

static void test_a_romanesca(void) {
    Seq seq;
    seq_init(&seq);
    const BeatStrength strengths[4] = {S, W, M, W};
    const int basses[4] = {1, 7, 6, 3};
    const int sopranos[4] = {1, 5, 1, 1};
    seq_stages(&seq, 0, strengths, basses, sopranos, 4);
    GalantSchemaMatches matches = find_matches(&seq, NULL);
    const GalantSchemaMatch *found = find_schema(&matches, "romanesca");
    assert(found != NULL && found->claim.stage_count == 4 && found->claim.first_segment == 0 && found->claim.last_segment == 3);
    assert(near_to(found->claim.confidence, 1.0) && near_to(found->uncertainty, 0.0) && found->claim.frame_degree == 1);
    assert(strcmp(found->claim.stages[0].segment_id, "s0") == 0 && found->claim.stages[1].bass_degree == 7 &&
           found->claim.stages[2].melody_degree == 1);
    assert(found->relationship != NULL && found->interpretation != NULL);
    GalantSchemaClaim claim;
    assert(galant_schema_claim_read(found->interpretation, &claim) && strcmp(claim.name, "romanesca") == 0);
    galant_schema_matches_destroy(&matches);
    seq_destroy(&seq);
}

static void test_a_prinner(void) {
    Seq seq;
    seq_init(&seq);
    const BeatStrength strengths[4] = {S, W, M, W};
    const int basses[4] = {4, 3, 2, 1};
    const int sopranos[4] = {6, 5, 4, 3};
    seq_stages(&seq, 0, strengths, basses, sopranos, 4);
    GalantSchemaMatches matches = find_matches(&seq, NULL);
    const GalantSchemaMatch *found = find_schema(&matches, "prinner");
    assert(found != NULL && near_to(found->claim.confidence, 1.0) && found->claim.stage_count == 4);
    galant_schema_matches_destroy(&matches);
    seq_destroy(&seq);
}

static void test_a_do_re_mi(void) {
    Seq seq;
    seq_init(&seq);
    const BeatStrength strengths[3] = {S, W, M};
    const int basses[3] = {1, 7, 1};
    const int sopranos[3] = {1, 2, 3};
    seq_stages(&seq, 0, strengths, basses, sopranos, 3);
    seq_add(&seq, 6, 2, W, deg(5, 0), deg(2, 0), HOME, 0.0); /* a segment after it */
    GalantSchemaMatches matches = find_matches(&seq, NULL);
    const GalantSchemaMatch *found = find_schema(&matches, "do-re-mi");
    assert(found != NULL && found->claim.stage_count == 3 && found->claim.last_segment == 2 && find_schema(&matches, "do-re-mi-four-stage") == NULL);
    galant_schema_matches_destroy(&matches);
    seq_destroy(&seq);
}

static void test_a_meyer_and_its_usual_bass(void) {
    Seq seq;
    seq_init(&seq);
    const BeatStrength strengths[4] = {W, M, W, S};
    const int basses[4] = {1, 2, 7, 1};
    const int sopranos[4] = {1, 7, 4, 3};
    seq_stages(&seq, 2, strengths, basses, sopranos, 4);
    GalantSchemaMatches matches = find_matches(&seq, NULL);
    const GalantSchemaMatch *found = find_schema(&matches, "meyer");
    assert(found != NULL && near_to(found->claim.confidence, 1.0));
    galant_schema_matches_destroy(&matches);
    seq_destroy(&seq);

    /* the Meyer's bass is less fixed than its melody: another bass in the third stage still matches, at a lower credit (0.75) */
    seq_init(&seq);
    const int other_basses[4] = {1, 2, 5, 1};
    seq_stages(&seq, 2, strengths, other_basses, sopranos, 4);
    matches = find_matches(&seq, NULL);
    found = find_schema(&matches, "meyer");
    assert(found != NULL && near_to(found->claim.pair_share, (1.0 + 1.0 + 0.75 + 1.0) / 4.0) &&
           near_to(found->claim.confidence, 0.9375));
    galant_schema_matches_destroy(&matches);
    seq_destroy(&seq);

    /* a defining bass is not so: the same change in a Prinner is no Prinner */
    seq_init(&seq);
    const BeatStrength prinner_strengths[4] = {S, W, M, W};
    const int prinner_basses[4] = {4, 3, 5, 1};
    const int prinner_sopranos[4] = {6, 5, 4, 3};
    seq_stages(&seq, 0, prinner_strengths, prinner_basses, prinner_sopranos, 4);
    matches = find_matches(&seq, NULL);
    assert(find_schema(&matches, "prinner") == NULL);
    galant_schema_matches_destroy(&matches);
    seq_destroy(&seq);
}

static void test_a_fonte_through_two_keys(void) {
    Seq seq;
    seq_init(&seq);
    const GalantPitchDegree d_major = deg(2, 0); /* the key of ii in C */
    /* the first two stages in D (C-sharp then D under G then F), the last two in C: read in C they are the Fonte's 1# 2 7 1 under 5 4 4 3 */
    seq_add(&seq, 2, 2, W, deg(7, 0), deg(4, 0), d_major, 0.0);
    seq_add(&seq, 4, 2, M, deg(1, 0), deg(3, -1), d_major, 0.0);
    seq_add(&seq, 6, 2, W, deg(7, 0), deg(4, 0), HOME, 0.0);
    seq_add(&seq, 8, 2, S, deg(1, 0), deg(3, 0), HOME, 0.0);
    GalantSchemaMatches matches = find_matches(&seq, NULL);
    const GalantSchemaMatch *found = find_schema(&matches, "fonte");
    assert(found != NULL && found->claim.frame_degree == 1 && found->claim.frame_alteration == 0 && near_to(found->claim.confidence, 1.0));
    assert(found->claim.stages[0].bass_degree == 1 && found->claim.stages[0].bass_alteration == 1);
    galant_schema_matches_destroy(&matches);
    seq_destroy(&seq);
}

static void test_a_monte_with_two_note_stages(void) {
    Seq seq;
    seq_init(&seq);
    const GalantPitchDegree f_major = deg(4, 0);
    const GalantPitchDegree g_major = deg(5, 0);
    /* in C: E under C then B-flat, F under A, F-sharp under D then C, G under B; the keys F then G */
    seq_add(&seq, 2, 1, W, deg(7, 0), deg(5, 0), f_major, 0.0);
    seq_add(&seq, 3, 1, W, deg(7, 0), deg(4, 0), f_major, 0.0);
    seq_add(&seq, 4, 2, M, deg(1, 0), deg(3, 0), f_major, 0.0);
    seq_add(&seq, 6, 1, W, deg(7, 0), deg(5, 0), g_major, 0.0);
    seq_add(&seq, 7, 1, W, deg(7, 0), deg(4, 0), g_major, 0.0);
    seq_add(&seq, 8, 2, S, deg(1, 0), deg(3, 0), g_major, 0.0);
    GalantSchemaMatches matches = find_matches(&seq, NULL);
    const GalantSchemaMatch *found = find_schema(&matches, "monte");
    assert(found != NULL && found->claim.stage_count == 4 && near_to(found->claim.confidence, 1.0));
    assert(found->claim.stages[0].has_second && strcmp(found->claim.stages[0].second_segment_id, "s1") == 0 &&
           found->claim.stages[0].second_degree == 7 && found->claim.stages[0].second_alteration == -1);
    assert(found->claim.stages[2].has_second && strcmp(found->claim.stages[3].segment_id, "s5") == 0 && found->claim.last_segment == 5);
    galant_schema_matches_destroy(&matches);
    seq_destroy(&seq);

    /* without the second melody note of a two-note stage there is no Monte */
    seq_init(&seq);
    seq_add(&seq, 2, 2, W, deg(7, 0), deg(5, 0), f_major, 0.0);
    seq_add(&seq, 4, 2, M, deg(1, 0), deg(3, 0), f_major, 0.0);
    seq_add(&seq, 6, 2, W, deg(7, 0), deg(5, 0), g_major, 0.0);
    seq_add(&seq, 8, 2, S, deg(1, 0), deg(3, 0), g_major, 0.0);
    matches = find_matches(&seq, NULL);
    assert(find_schema(&matches, "monte") == NULL);
    galant_schema_matches_destroy(&matches);
    seq_destroy(&seq);
}

static void test_embellishing_segments_within_the_gap(void) {
    Seq seq;
    seq_init(&seq);
    /* a Prinner whose second stage comes after a passing segment: the gap is one beat, within the two allowed */
    seq_add(&seq, 0, 2, S, deg(4, 0), deg(6, 0), HOME, 0.0);
    seq_add(&seq, 2, 2, W, deg(4, 0), deg(7, 0), HOME, 0.0);
    seq_add(&seq, 4, 2, W, deg(3, 0), deg(5, 0), HOME, 0.0);
    seq_add(&seq, 6, 2, M, deg(2, 0), deg(4, 0), HOME, 0.0);
    seq_add(&seq, 8, 2, W, deg(1, 0), deg(3, 0), HOME, 0.0);
    GalantSchemaMatches matches = find_matches(&seq, NULL);
    const GalantSchemaMatch *found = find_schema(&matches, "prinner");
    assert(found != NULL && found->claim.first_segment == 0 && found->claim.last_segment == 4 &&
           strcmp(found->claim.stages[1].segment_id, "s2") == 0 && near_to(found->claim.metric_fit, 1.0) &&
           near_to(found->claim.confidence, 1.0));
    galant_schema_matches_destroy(&matches);
    seq_destroy(&seq);
}

static void test_a_gap_that_is_too_long(void) {
    Seq seq;
    seq_init(&seq);
    seq_add(&seq, 0, 2, S, deg(4, 0), deg(6, 0), HOME, 0.0);
    for (int k = 0; k < 3; k++) { /* three beats of embellishment */
        seq_add(&seq, 2 + 2 * k, 2, W, deg(4, 0), deg(7, 0), HOME, 0.0);
    }
    seq_add(&seq, 8, 2, W, deg(3, 0), deg(5, 0), HOME, 0.0);
    seq_add(&seq, 10, 2, M, deg(2, 0), deg(4, 0), HOME, 0.0);
    seq_add(&seq, 12, 2, W, deg(1, 0), deg(3, 0), HOME, 0.0);
    GalantSchemaMatches matches = find_matches(&seq, NULL);
    assert(find_schema(&matches, "prinner") == NULL);
    galant_schema_matches_destroy(&matches);
    /* the same with a longer allowance */
    GalantMatchConfig longer = galant_match_config_default();
    longer.max_gap_beats = 3.0;
    matches = find_matches(&seq, &longer);
    assert(find_schema(&matches, "prinner") != NULL);
    galant_schema_matches_destroy(&matches);
    seq_destroy(&seq);
}

static void test_a_stage_on_a_weaker_beat_than_what_lies_between(void) {
    Seq seq;
    seq_init(&seq);
    /* a Romanesca with a strong-beat segment between the first and second stages: the second stage is weaker than it */
    seq_add(&seq, 0, 2, S, deg(1, 0), deg(1, 0), HOME, 0.0);
    seq_add(&seq, 2, 2, S, deg(1, 0), deg(3, 0), HOME, 0.0);
    seq_add(&seq, 4, 2, W, deg(7, 0), deg(5, 0), HOME, 0.0);
    seq_add(&seq, 6, 2, M, deg(6, 0), deg(1, 0), HOME, 0.0);
    seq_add(&seq, 8, 2, W, deg(3, 0), deg(1, 0), HOME, 0.0);
    GalantSchemaMatches matches = find_matches(&seq, NULL);
    const GalantSchemaMatch *found = find_schema(&matches, "romanesca");
    assert(found != NULL && near_to(found->claim.metric_fit, 0.8) && near_to(found->claim.confidence, 0.8));
    galant_schema_matches_destroy(&matches);
    seq_destroy(&seq);
}

static void test_the_bass_only_stage(void) {
    Seq seq;
    seq_init(&seq);
    const BeatStrength strengths[4] = {S, W, M, W};
    const int basses[4] = {1, 7, 6, 3};
    const int one_absent[4] = {1, 0, 1, 1};
    seq_stages(&seq, 0, strengths, basses, one_absent, 4);
    GalantSchemaMatches matches = find_matches(&seq, NULL);
    const GalantSchemaMatch *found = find_schema(&matches, "romanesca");
    assert(found != NULL && found->claim.bass_only_stages == 1 && found->claim.stages[1].bass_only &&
           near_to(found->claim.pair_share, 0.875) && near_to(found->claim.confidence, 0.875));
    galant_schema_matches_destroy(&matches);
    /* no allowance: no match */
    GalantMatchConfig none = galant_match_config_default();
    none.bass_only_allowance = 0;
    matches = find_matches(&seq, &none);
    assert(find_schema(&matches, "romanesca") == NULL);
    galant_schema_matches_destroy(&matches);
    seq_destroy(&seq);

    /* at most once per schema */
    seq_init(&seq);
    const int two_absent[4] = {1, 0, 0, 1};
    seq_stages(&seq, 0, strengths, basses, two_absent, 4);
    matches = find_matches(&seq, NULL);
    assert(find_schema(&matches, "romanesca") == NULL);
    galant_schema_matches_destroy(&matches);
    seq_destroy(&seq);
}

static void test_the_uncertainty_lowers_the_confidence(void) {
    Seq seq;
    seq_init(&seq);
    seq_add(&seq, 0, 2, S, deg(1, 0), deg(1, 0), HOME, 0.2);
    seq_add(&seq, 2, 2, W, deg(7, 0), deg(5, 0), HOME, 0.2);
    seq_add(&seq, 4, 2, M, deg(6, 0), deg(1, 0), HOME, 0.2);
    seq_add(&seq, 6, 2, W, deg(3, 0), deg(1, 0), HOME, 0.2);
    GalantSchemaMatches matches = find_matches(&seq, NULL);
    const GalantSchemaMatch *found = find_schema(&matches, "romanesca");
    assert(found != NULL && near_to(found->claim.mean_uncertainty, 0.2) && near_to(found->claim.confidence, 0.8));
    galant_schema_matches_destroy(&matches);
    seq_destroy(&seq);
}

static void test_overlapping_candidates_by_confidence(void) {
    Seq seq;
    seq_init(&seq);
    /* two segments could be the second stage (a repeated chord); the one that is less uncertain wins */
    seq_add(&seq, 0, 2, S, deg(1, 0), deg(1, 0), HOME, 0.0);
    seq_add(&seq, 2, 1, W, deg(7, 0), deg(5, 0), HOME, 0.3);
    seq_add(&seq, 3, 1, W, deg(7, 0), deg(5, 0), HOME, 0.0);
    seq_add(&seq, 4, 2, M, deg(6, 0), deg(1, 0), HOME, 0.0);
    seq_add(&seq, 6, 2, W, deg(3, 0), deg(1, 0), HOME, 0.0);
    GalantSchemaMatches matches = find_matches(&seq, NULL);
    size_t romanescas = 0;
    for (size_t i = 0; i < matches.count; i++) {
        romanescas += strcmp(matches.items[i].claim.name, "romanesca") == 0 ? 1 : 0;
    }
    const GalantSchemaMatch *found = find_schema(&matches, "romanesca");
    assert(romanescas == 1 && found != NULL && strcmp(found->claim.stages[1].segment_id, "s2") == 0 && near_to(found->claim.confidence, 1.0));
    galant_schema_matches_destroy(&matches);
    seq_destroy(&seq);

    /* two schemas one after the other, sharing nothing: both are kept, in order */
    seq_init(&seq);
    const BeatStrength strengths[4] = {S, W, M, W};
    const int romanesca_basses[4] = {1, 7, 6, 3};
    const int romanesca_sopranos[4] = {1, 5, 1, 1};
    seq_stages(&seq, 0, strengths, romanesca_basses, romanesca_sopranos, 4);
    const int prinner_basses[4] = {4, 3, 2, 1};
    const int prinner_sopranos[4] = {6, 5, 4, 3};
    seq_stages(&seq, 8, strengths, prinner_basses, prinner_sopranos, 4);
    matches = find_matches(&seq, NULL);
    const GalantSchemaMatch *first = find_schema(&matches, "romanesca");
    const GalantSchemaMatch *second = find_schema(&matches, "prinner");
    assert(first != NULL && second != NULL && first->claim.first_segment == 0 && second->claim.first_segment == 4);
    galant_schema_matches_destroy(&matches);
    seq_destroy(&seq);
}

static void test_bad_calls(void) {
    GalantSchemaMatches matches = {NULL, 0};
    assert(galant_schema_match(NULL, NULL, &matches) == GALANT_ANALYSIS_ERR_NULL_ARGUMENT);
    Seq seq;
    seq_init(&seq);
    assert(galant_schema_match(&seq.list, NULL, NULL) == GALANT_ANALYSIS_ERR_NULL_ARGUMENT);
    assert(galant_schema_match(&seq.list, NULL, &matches) == GALANT_ANALYSIS_OK && matches.count == 0 && matches.items == NULL);
    GalantMatchConfig bad = galant_match_config_default();
    bad.max_gap_beats = -1.0;
    seq_add(&seq, 0, 2, S, deg(1, 0), deg(1, 0), HOME, 0.0);
    assert(galant_schema_match(&seq.list, &bad, &matches) == GALANT_ANALYSIS_ERR_INVALID);
    /* a segment out of order, a degree out of range, an empty id */
    GalantSegment segment = seq.list.items[0];
    segment.bass = deg(8, 0);
    assert(galant_segments_add(&seq.list, &segment) == GALANT_ANALYSIS_ERR_INVALID);
    segment = seq.list.items[0];
    segment.id[0] = '\0';
    assert(galant_segments_add(&seq.list, &segment) == GALANT_ANALYSIS_ERR_INVALID);
    segment = seq.list.items[0];
    segment.uncertainty = 1.5;
    assert(galant_segments_add(&seq.list, &segment) == GALANT_ANALYSIS_ERR_INVALID);
    seq_add(&seq, 4, 2, W, deg(1, 0), deg(1, 0), HOME, 0.0);
    segment = seq.list.items[0];
    assert(galant_segments_add(&seq.list, &segment) == GALANT_ANALYSIS_ERR_INVALID); /* starts before the one before it */
    GalantSchemaClaim claim;
    assert(!galant_schema_claim_read(NULL, &claim));
    seq_destroy(&seq);
}

int main(void) {
    test_degree_arithmetic();
    test_a_romanesca();
    test_a_prinner();
    test_a_do_re_mi();
    test_a_meyer_and_its_usual_bass();
    test_a_fonte_through_two_keys();
    test_a_monte_with_two_note_stages();
    test_embellishing_segments_within_the_gap();
    test_a_gap_that_is_too_long();
    test_a_stage_on_a_weaker_beat_than_what_lies_between();
    test_the_bass_only_stage();
    test_the_uncertainty_lowers_the_confidence();
    test_overlapping_candidates_by_confidence();
    test_bad_calls();
    printf("test_analysis_match (galant-schemas): ok\n");
    return 0;
}
