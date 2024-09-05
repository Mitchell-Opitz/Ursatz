#include "scale_analysis_test_util.h"

static void test_ascending_five_six_with_applied_chords(void) {
    Run run;
    run_init(&run);
    /* a fifth then a sixth over each bass note C D E: I vi6 | ii (vii/V)6 | iii I6 */
    const int basses[6] = {1, 1, 2, 2, 3, 3};
    const int sopranos[6] = {5, 6, 6, 7, 7, 1};
    const int roots[6] = {1, 6, 2, 7, 3, 1};
    for (size_t i = 0; i < 6; i++) {
        run_add(&run, basses[i], sopranos[i], roots[i], i == 3 ? 5 : 0, 0.0); /* the fourth chord is applied to the dominant */
    }
    ScaleSequenceMatches matches = detect(&run, NULL);
    assert(matches.count == 1);
    const ScaleSequenceClaim *claim = &matches.items[0].claim;
    assert(claim->kind == SEQUENCE_KIND_FIVE_SIX && claim->pattern == SCALE_PATTERN_FIVE_SIX_ASCENDING && claim->rule_backed == 1);
    assert(claim->units == 3 && claim->unit_length == 2 && claim->stage_count == 6 && claim->transposition_steps == 1);
    assert(claim->first_segment == 0 && claim->last_segment == 5 && strcmp(claim->kind_name, "five-six") == 0);
    assert(near_to(claim->unit_share, 1.0) && near_to(claim->soprano_bonus, 0.1) && near_to(claim->confidence, 1.0));
    assert(strcmp(claim->stages[1].segment_id, "s1") == 0 && claim->stages[1].soprano_degree == 6 && claim->stages[3].root_degree == 7);
    assert(matches.items[0].relationship != NULL && matches.items[0].interpretation != NULL);
    ScaleSequenceClaim read;
    assert(scale_sequence_claim_read(matches.items[0].interpretation, &read) && read.units == 3);
    scale_sequence_matches_destroy(&matches);
    run_destroy(&run);
}

static void test_descending_five_six_in_thirds(void) {
    Run run;
    run_init(&run);
    /* the bass runs down C B A G F E, the soprano down in thirds G G E E C C: a fifth, then a sixth, on each new bass note */
    const int basses[6] = {1, 7, 6, 5, 4, 3};
    const int sopranos[6] = {5, 5, 3, 3, 1, 1};
    run_chords(&run, basses, sopranos, NULL, 6);
    ScaleSequenceMatches matches = detect(&run, NULL);
    assert(matches.count == 1);
    const ScaleSequenceClaim *claim = &matches.items[0].claim;
    assert(claim->kind == SEQUENCE_KIND_FIVE_SIX && claim->pattern == SCALE_PATTERN_FIVE_SIX_DESCENDING_ALTERNATING &&
           claim->direction == SCALE_DIRECTION_DESCENDING && claim->units == 3 && claim->transposition_steps == -2);
    scale_sequence_matches_destroy(&matches);
    run_destroy(&run);

    /* note by note: a fifth then a sixth over each bass note */
    run_init(&run);
    const int slow_basses[6] = {3, 3, 2, 2, 1, 1};
    const int slow_sopranos[6] = {7, 1, 6, 7, 5, 6};
    run_chords(&run, slow_basses, slow_sopranos, NULL, 6);
    matches = detect(&run, NULL);
    assert(matches.count == 1 && matches.items[0].claim.pattern == SCALE_PATTERN_FIVE_SIX_DESCENDING &&
           matches.items[0].claim.transposition_steps == -1);
    scale_sequence_matches_destroy(&matches);
    run_destroy(&run);
}

static void test_parallel_chords_and_seven_six(void) {
    Run run;
    run_init(&run);
    const int basses[4] = {1, 2, 3, 4};
    const int sixths[4] = {6, 7, 1, 2};
    run_chords(&run, basses, sixths, NULL, 4);
    ScaleSequenceMatches matches = detect(&run, NULL);
    assert(matches.count == 1 && matches.items[0].claim.kind == SEQUENCE_KIND_PARALLEL_CHORDS && matches.items[0].claim.units == 4 &&
           matches.items[0].claim.direction == SCALE_DIRECTION_ASCENDING);
    scale_sequence_matches_destroy(&matches);
    run_destroy(&run);

    /* a chain of suspensions: a seventh held over each new bass note, then resolved to the sixth */
    run_init(&run);
    const int seven_basses[6] = {1, 1, 7, 7, 6, 6};
    const int seven_sopranos[6] = {7, 6, 6, 5, 5, 4};
    run_chords(&run, seven_basses, seven_sopranos, NULL, 6);
    matches = detect(&run, NULL);
    assert(matches.count == 1 && matches.items[0].claim.kind == SEQUENCE_KIND_SEVEN_SIX &&
           matches.items[0].claim.pattern == SCALE_PATTERN_SEVEN_SIX_DESCENDING && matches.items[0].claim.units == 3);
    scale_sequence_matches_destroy(&matches);
    run_destroy(&run);
}

static void test_a_cycle_of_fifths_is_not_rule_backed(void) {
    Run run;
    run_init(&run);
    /* I IV vii iii vi ii V I: each root a fifth below the one before; two chords make a unit, four units */
    const int roots[8] = {1, 4, 7, 3, 6, 2, 5, 1};
    const int sopranos[8] = {3, 4, 2, 3, 1, 2, 7, 1};
    run_chords(&run, roots, sopranos, roots, 8);
    ScaleSequenceMatches matches = detect(&run, NULL);
    assert(matches.count == 1);
    const ScaleSequenceClaim *claim = &matches.items[0].claim;
    assert(claim->kind == SEQUENCE_KIND_CYCLE_OF_FIFTHS && claim->rule_backed == 0 && claim->pattern == -1 && claim->units == 4 &&
           claim->unit_length == 2 && claim->stage_count == 8 && claim->transposition_steps == -1);
    assert(near_to(claim->unit_share, 1.0) && near_to(claim->confidence, 1.0));
    scale_sequence_matches_destroy(&matches);
    run_destroy(&run);

    /* roots that rise by fifths have no kind */
    run_init(&run);
    const int rising[8] = {1, 5, 2, 6, 3, 7, 4, 1};
    run_chords(&run, rising, sopranos, rising, 8);
    matches = detect(&run, NULL);
    assert(matches.count == 0);
    scale_sequence_matches_destroy(&matches);
    run_destroy(&run);

    /* without numerals there are no roots to read */
    run_init(&run);
    run_chords(&run, roots, sopranos, NULL, 8);
    matches = detect(&run, NULL);
    assert(matches.count == 0);
    scale_sequence_matches_destroy(&matches);
    run_destroy(&run);
}

static void test_two_units_are_not_a_sequence_unless_allowed(void) {
    Run run;
    run_init(&run);
    const int basses[4] = {1, 1, 2, 2};
    const int sopranos[4] = {5, 6, 6, 7};
    run_chords(&run, basses, sopranos, NULL, 4);
    ScaleSequenceMatches matches = detect(&run, NULL);
    assert(matches.count == 0);
    scale_sequence_matches_destroy(&matches);
    ScaleDetectConfig two = scale_detect_config_default();
    two.minimum_units = 2;
    matches = detect(&run, &two);
    assert(matches.count == 1 && matches.items[0].claim.units == 2 && matches.items[0].claim.kind == SEQUENCE_KIND_FIVE_SIX);
    scale_sequence_matches_destroy(&matches);
    run_destroy(&run);
}

static void test_a_broken_pattern_stops_the_sequence_at_its_unit(void) {
    Run run;
    run_init(&run);
    /* four whole units of the ascending 5-6, then a bass note G with a soprano that is not a fifth above it */
    const int basses[10] = {1, 1, 2, 2, 3, 3, 4, 4, 5, 5};
    const int sopranos[10] = {5, 6, 6, 7, 7, 1, 1, 2, 4, 5};
    run_chords(&run, basses, sopranos, NULL, 10);
    ScaleSequenceMatches matches = detect(&run, NULL);
    assert(matches.count == 1);
    assert(matches.items[0].claim.units == 4 && matches.items[0].claim.stage_count == 8 && matches.items[0].claim.last_segment == 7);
    scale_sequence_matches_destroy(&matches);
    run_destroy(&run);

    /* an odd stage at the end of a run is dropped: the sequence is whole units */
    run_init(&run);
    const int odd_basses[7] = {1, 1, 2, 2, 3, 3, 4};
    const int odd_sopranos[7] = {5, 6, 6, 7, 7, 1, 1};
    run_chords(&run, odd_basses, odd_sopranos, NULL, 7);
    matches = detect(&run, NULL);
    assert(matches.count == 1 && matches.items[0].claim.units == 3 && matches.items[0].claim.last_segment == 5);
    scale_sequence_matches_destroy(&matches);
    run_destroy(&run);
}

static void test_a_segment_without_a_bass_ends_a_run(void) {
    Run run;
    run_init(&run);
    const int basses[6] = {1, 1, 2, 2, 3, 3};
    const int sopranos[6] = {5, 6, 6, 7, 7, 1};
    for (size_t i = 0; i < 6; i++) {
        if (i == 3) {
            run_add(&run, 0, 7, 0, 0, 0.0); /* no bass here */
        } else {
            run_add(&run, basses[i], sopranos[i], 0, 0, 0.0);
        }
    }
    ScaleSequenceMatches matches = detect(&run, NULL);
    assert(matches.count == 0);
    scale_sequence_matches_destroy(&matches);
    run_destroy(&run);
}

static void test_uncertainty_lowers_the_confidence(void) {
    Run run;
    run_init(&run);
    const int basses[6] = {1, 1, 2, 2, 3, 3};
    const int sopranos[6] = {5, 6, 6, 7, 7, 1};
    for (size_t i = 0; i < 6; i++) {
        run_add(&run, basses[i], sopranos[i], 0, 0, 0.3);
    }
    ScaleSequenceMatches matches = detect(&run, NULL);
    assert(matches.count == 1 && near_to(matches.items[0].claim.mean_uncertainty, 0.3) &&
           near_to(matches.items[0].claim.confidence, 0.7 + 0.1) && near_to(matches.items[0].uncertainty, 1.0 - 0.8));
    scale_sequence_matches_destroy(&matches);
    run_destroy(&run);
}

static void test_bad_calls(void) {
    ScaleSequenceMatches matches = {NULL, 0};
    assert(scale_sequence_detect(NULL, NULL, &matches) == GALANT_ANALYSIS_ERR_NULL_ARGUMENT);
    Run run;
    run_init(&run);
    assert(scale_sequence_detect(&run.list, NULL, NULL) == GALANT_ANALYSIS_ERR_NULL_ARGUMENT);
    assert(scale_sequence_detect(&run.list, NULL, &matches) == GALANT_ANALYSIS_OK && matches.count == 0);
    ScaleDetectConfig bad = scale_detect_config_default();
    bad.minimum_units = 1;
    assert(scale_sequence_detect(&run.list, &bad, &matches) == GALANT_ANALYSIS_ERR_INVALID);
    assert(!scale_sequence_claim_read(NULL, NULL));
    run_destroy(&run);
}

int main(void) {
    test_ascending_five_six_with_applied_chords();
    test_descending_five_six_in_thirds();
    test_parallel_chords_and_seven_six();
    test_a_cycle_of_fifths_is_not_rule_backed();
    test_two_units_are_not_a_sequence_unless_allowed();
    test_a_broken_pattern_stops_the_sequence_at_its_unit();
    test_a_segment_without_a_bass_ends_a_run();
    test_uncertainty_lowers_the_confidence();
    test_bad_calls();
    printf("test_analysis_detect (scale-harmonization): ok\n");
    return 0;
}
