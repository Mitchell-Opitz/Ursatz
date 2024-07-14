#include <string.h>

#include "analysis_test_util.h"
#include "meter_analysis_hypermeter.h"

static StandardMeterHypermeter find(Piece *piece) {
    StandardMeterHypermeter found;
    memset(&found, 0, sizeof(found));
    assert(standard_meter_find_hypermeter(&piece->input, NULL, &found) == STANDARD_METER_ANALYSIS_OK);
    return found;
}

/* A phrase over measures first..last (1-based, 4/4 so a measure is one whole note) with its arrival in `arrival`. */
static void phrase(Piece *piece, int first, int last, int arrival, StandardMeterExpansionKind expansion, int measures,
                   bool elided) {
    piece_phrase(piece, first - 1, 1, last, 1, arrival > 0, arrival - 1, 1, arrival, 1, expansion, measures, elided);
}

static void test_four_bar_hypermeasures_in_an_eight_bar_period(void) {
    Piece piece;
    piece_init(&piece, 4, 2);
    phrase(&piece, 1, 4, 4, STANDARD_METER_EXPANSION_NONE, 0, false);
    phrase(&piece, 5, 8, 8, STANDARD_METER_EXPANSION_NONE, 0, false);
    StandardMeterHypermeter found = find(&piece);
    assert(found.phrase_count == 2 && found.hypermeasure_count == 2 && found.reinterpretation_count == 0);
    assert(found.phrases[0].length == 4 && found.phrases[1].length == 4 && found.phrases[0].score == 1.0);
    assert(found.hypermeasures[0].claim.first_measure == 1 && found.hypermeasures[0].claim.measures == 4);
    assert(found.hypermeasures[1].claim.first_measure == 5 && found.hypermeasures[1].claim.last_measure == 8);
    assert(found.hypermeasures[0].claim.kind == (int)STANDARD_METER_HYPERMEASURE_REGULAR);
    assert(found.hypermeasures[0].claim.contains_cadence == 1 && found.phrases[1].arrival_measure == 8);
    assert(found.hypermeasures[0].relationship != NULL && found.hypermeasures[0].interpretation != NULL);
    assert(found.hypermeasures[0].uncertainty == 0.0);
    standard_meter_hypermeter_destroy(&found);
    piece_destroy(&piece);
}

static void test_a_two_bar_choice(void) {
    Piece piece;
    piece_init(&piece, 4, 2);
    phrase(&piece, 1, 2, 2, STANDARD_METER_EXPANSION_NONE, 0, false);
    phrase(&piece, 3, 8, 8, STANDARD_METER_EXPANSION_NONE, 0, false);
    StandardMeterHypermeter found = find(&piece);
    assert(found.phrase_count == 2);
    assert(found.phrases[0].length == 2 && found.phrases[0].score == 1.0);
    assert(found.phrases[1].length == 2 && found.phrases[1].hypermeasure_count == 3); /* six bars, three of two */
    assert(found.hypermeasures[1].claim.first_measure == 3 && found.hypermeasures[2].claim.first_measure == 5 &&
           found.hypermeasures[3].claim.first_measure == 7);
    assert(found.hypermeasures[3].claim.contains_cadence == 1 && found.hypermeasures[2].claim.contains_cadence == 0);
    standard_meter_hypermeter_destroy(&found);
    piece_destroy(&piece);

    /* a three-bar phrase fits neither length well: two, with a short hypermeasure and a lower confidence */
    piece_init(&piece, 4, 2);
    phrase(&piece, 1, 3, 3, STANDARD_METER_EXPANSION_NONE, 0, false);
    found = find(&piece);
    assert(found.phrases[0].length == 2 && found.hypermeasure_count == 2);
    assert(found.hypermeasures[1].claim.kind == (int)STANDARD_METER_HYPERMEASURE_SHORT &&
           found.hypermeasures[1].claim.measures == 1 && found.hypermeasures[1].claim.extra_hyperbeats == -1);
    assert(found.hypermeasures[0].uncertainty > 0.0 && found.hypermeasures[0].claim.score < 1.0);
    standard_meter_hypermeter_destroy(&found);
    piece_destroy(&piece);
}

static void test_expansions_change_a_hypermeasure(void) {
    Piece piece;
    piece_init(&piece, 4, 2);
    /* an internal expansion of one measure: four bars and an added hyperbeat */
    phrase(&piece, 1, 5, 5, STANDARD_METER_EXPANSION_INTERNAL, 1, false);
    StandardMeterHypermeter found = find(&piece);
    assert(found.phrases[0].length == 4 && found.hypermeasure_count == 1);
    assert(found.hypermeasures[0].claim.measures == 5 && found.hypermeasures[0].claim.extra_hyperbeats == 1 &&
           found.hypermeasures[0].claim.kind == (int)STANDARD_METER_HYPERMEASURE_EXPANDED);
    standard_meter_hypermeter_destroy(&found);
    piece_destroy(&piece);

    /* an external expansion: the cadence in bar 4 and two more bars after it */
    piece_init(&piece, 4, 2);
    phrase(&piece, 1, 6, 4, STANDARD_METER_EXPANSION_EXTERNAL, 2, false);
    found = find(&piece);
    assert(found.hypermeasure_count == 2 && found.hypermeasures[0].claim.measures == 4 &&
           found.hypermeasures[1].claim.kind == (int)STANDARD_METER_HYPERMEASURE_EXTENSION &&
           found.hypermeasures[1].claim.first_measure == 5 && found.hypermeasures[1].claim.measures == 2);
    standard_meter_hypermeter_destroy(&found);
    piece_destroy(&piece);

    /* a contraction: three bars read as four with the last hyperbeat gone */
    piece_init(&piece, 4, 2);
    phrase(&piece, 1, 3, 3, STANDARD_METER_EXPANSION_CONTRACTION, 1, false);
    found = find(&piece);
    assert(found.phrases[0].length == 4 && found.hypermeasure_count == 1 && found.hypermeasures[0].claim.measures == 3 &&
           found.hypermeasures[0].claim.kind == (int)STANDARD_METER_HYPERMEASURE_CONTRACTED &&
           found.hypermeasures[0].claim.extra_hyperbeats == -1);
    standard_meter_hypermeter_destroy(&found);
    piece_destroy(&piece);
}

static void test_an_elision_is_reinterpreted(void) {
    Piece piece;
    piece_init(&piece, 4, 2);
    /* the first phrase's arrival is bar 5, the second phrase's first bar: "4 = 1" */
    piece_phrase(&piece, 0, 1, 4, 1, true, 4, 1, 5, 1, STANDARD_METER_EXPANSION_NONE, 0, true);
    piece_phrase(&piece, 4, 1, 8, 1, true, 7, 1, 8, 1, STANDARD_METER_EXPANSION_NONE, 0, false);
    StandardMeterHypermeter found = find(&piece);
    assert(found.phrase_count == 2 && found.reinterpretation_count == 1);
    const StandardMeterReinterpretationClaim *claim = &found.reinterpretations[0].claim;
    assert(claim->phrase == 0 && claim->next_phrase == 1 && claim->measure == 5);
    assert(claim->count_in_ending == 4 && claim->count_in_next == 1);
    assert(found.reinterpretations[0].relationship != NULL && found.reinterpretations[0].interpretation != NULL);
    assert(found.phrases[0].arrival_measure == -1 && found.phrases[1].first_measure == 5);
    standard_meter_hypermeter_destroy(&found);
    piece_destroy(&piece);

    /* an elided flag on the last phrase has nothing to elide into */
    piece_init(&piece, 4, 2);
    piece_phrase(&piece, 0, 1, 4, 1, false, 0, 1, 1, 1, STANDARD_METER_EXPANSION_NONE, 0, true);
    found = find(&piece);
    assert(found.phrase_count == 1 && found.reinterpretation_count == 0);
    standard_meter_hypermeter_destroy(&found);
    piece_destroy(&piece);
}

static void test_bad_calls(void) {
    StandardMeterHypermeter found;
    memset(&found, 0, sizeof(found));
    assert(standard_meter_find_hypermeter(NULL, NULL, &found) == STANDARD_METER_ANALYSIS_ERR_NULL_ARGUMENT);
    Piece piece;
    piece_init(&piece, 4, 2);
    assert(standard_meter_find_hypermeter(&piece.input, NULL, NULL) == STANDARD_METER_ANALYSIS_ERR_NULL_ARGUMENT);
    found = find(&piece);
    assert(found.phrase_count == 0 && found.hypermeasures == NULL);
    standard_meter_hypermeter_destroy(&found);
    /* an expansion needs a measure count and a count needs an expansion */
    assert(standard_meter_input_add_phrase(&piece.input, "bad", piece_span(&piece, 0, 1, 1, 1), NULL, STANDARD_METER_EXPANSION_NONE,
                                           2, false) == STANDARD_METER_ANALYSIS_ERR_INVALID);
    piece_destroy(&piece);
}

int main(void) {
    test_four_bar_hypermeasures_in_an_eight_bar_period();
    test_a_two_bar_choice();
    test_expansions_change_a_hypermeasure();
    test_an_elision_is_reinterpreted();
    test_bad_calls();
    printf("test_analysis_hypermeter (standard-meter): ok\n");
    return 0;
}
