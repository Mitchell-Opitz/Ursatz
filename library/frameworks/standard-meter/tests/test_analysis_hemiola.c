#include <string.h>

#include "analysis_test_util.h"
#include "meter_analysis_hemiola.h"

static StandardMeterHemiolas find(Piece *piece, const StandardMeterConfig *config) {
    StandardMeterHemiolas found = {NULL, 0};
    assert(standard_meter_find_hemiolas(&piece->input, config, &found) == STANDARD_METER_ANALYSIS_OK);
    return found;
}

/* Three half notes across two measures of 3/4: the cadential hemiola, with the harmony changing with them. */
static void three_half_notes(Piece *piece, bool with_chords) {
    piece_note(piece, 0, 0, 1, 1, 2);
    piece_note(piece, 0, 1, 2, 1, 1);
    piece_note(piece, 0, 1, 1, 3, 2);
    if (with_chords) {
        piece_chord_change(piece, 0, 1);
        piece_chord_change(piece, 1, 2);
        piece_chord_change(piece, 1, 1);
    }
}

static void test_a_cadential_hemiola_in_three_four(void) {
    Piece piece;
    piece_init(&piece, 3, 2);
    three_half_notes(&piece, true);
    StandardMeterHemiolas found = find(&piece, NULL);
    assert(found.count == 1);
    const StandardMeterHemiolaClaim *claim = &found.items[0].claim;
    assert(claim->first_measure == 1 && claim->last_measure == 2);
    assert(claim->duration_evidence == 1 && claim->chord_evidence == 1 && claim->accent_evidence == 1);
    assert(claim->agreeing == 3 && claim->available == 3 && found.items[0].uncertainty == 0.0);
    assert(found.items[0].shell != NULL && found.items[0].interpretation != NULL);
    standard_meter_hemiolas_destroy(&found);
    piece_destroy(&piece);

    /* with no chord changes given, the attacks and the long notes still agree */
    piece_init(&piece, 3, 2);
    three_half_notes(&piece, false);
    found = find(&piece, NULL);
    assert(found.count == 1 && found.items[0].claim.chord_evidence == -1 && found.items[0].claim.available == 2 &&
           found.items[0].claim.agreeing == 2);
    standard_meter_hemiolas_destroy(&found);
    piece_destroy(&piece);

    /* needing all three, two are not enough */
    piece_init(&piece, 3, 2);
    three_half_notes(&piece, false);
    StandardMeterConfig strict = standard_meter_config_default();
    strict.hemiola_min_evidence = 3;
    found = find(&piece, &strict);
    assert(found.count == 0);
    standard_meter_hemiolas_destroy(&found);
    piece_destroy(&piece);
}

static void test_a_near_miss_with_one_evidence_type(void) {
    Piece piece;
    piece_init(&piece, 3, 2);
    /* quarter notes throughout, so no duple grouping in the notes; only the chords change on the duple pulses */
    for (long i = 0; i < 6; i++) {
        piece_note(&piece, 0, i, 4, i + 1, 4);
    }
    piece_chord_change(&piece, 0, 1);
    piece_chord_change(&piece, 1, 2);
    piece_chord_change(&piece, 1, 1);
    StandardMeterHemiolas found = find(&piece, NULL);
    assert(found.count == 0);
    standard_meter_hemiolas_destroy(&found);
    piece_destroy(&piece);

    /* ordinary triple meter: a note and two quarters in each measure, the chords changing on the downbeats */
    piece_init(&piece, 3, 2);
    piece_note(&piece, 0, 0, 1, 1, 2);
    piece_note(&piece, 0, 1, 2, 3, 4);
    piece_note(&piece, 0, 3, 4, 5, 4);
    piece_note(&piece, 0, 5, 4, 3, 2);
    piece_chord_change(&piece, 0, 1);
    piece_chord_change(&piece, 3, 4);
    found = find(&piece, NULL);
    assert(found.count == 0);
    standard_meter_hemiolas_destroy(&found);
    piece_destroy(&piece);
}

static void test_six_eight_and_other_meters(void) {
    Piece piece;
    piece_init(&piece, 6, 3);
    piece_note(&piece, 0, 0, 1, 1, 4); /* three quarter notes in one measure of 6/8 */
    piece_note(&piece, 0, 1, 4, 1, 2);
    piece_note(&piece, 0, 1, 2, 3, 4);
    StandardMeterHemiolas found = find(&piece, NULL);
    assert(found.count == 1 && found.items[0].claim.first_measure == 1 && found.items[0].claim.last_measure == 1);
    standard_meter_hemiolas_destroy(&found);
    piece_destroy(&piece);

    piece_init(&piece, 12, 3); /* 12/8 and 4/4 have no hemiola here */
    piece_note(&piece, 0, 0, 1, 1, 4);
    piece_note(&piece, 0, 1, 4, 1, 2);
    piece_note(&piece, 0, 1, 2, 3, 4);
    found = find(&piece, NULL);
    assert(found.count == 0);
    standard_meter_hemiolas_destroy(&found);
    piece_destroy(&piece);

    piece_init(&piece, 4, 2);
    three_half_notes(&piece, true);
    found = find(&piece, NULL);
    assert(found.count == 0);
    standard_meter_hemiolas_destroy(&found);
    piece_destroy(&piece);
}

static void test_windows_do_not_overlap(void) {
    Piece piece;
    piece_init(&piece, 3, 2);
    for (long i = 0; i < 6; i++) { /* half notes through four measures: two windows, not three */
        piece_note(&piece, 0, i, 2, i + 1, 2);
    }
    StandardMeterHemiolas found = find(&piece, NULL);
    assert(found.count == 2 && found.items[0].claim.first_measure == 1 && found.items[1].claim.first_measure == 3);
    standard_meter_hemiolas_destroy(&found);
    piece_destroy(&piece);
}

static void test_bad_calls(void) {
    StandardMeterHemiolas found = {NULL, 0};
    assert(standard_meter_find_hemiolas(NULL, NULL, &found) == STANDARD_METER_ANALYSIS_ERR_NULL_ARGUMENT);
    Piece piece;
    piece_init(&piece, 3, 2);
    assert(standard_meter_find_hemiolas(&piece.input, NULL, NULL) == STANDARD_METER_ANALYSIS_ERR_NULL_ARGUMENT);
    found = find(&piece, NULL);
    assert(found.count == 0);
    standard_meter_hemiolas_destroy(&found);
    piece_destroy(&piece);
}

int main(void) {
    test_a_cadential_hemiola_in_three_four();
    test_a_near_miss_with_one_evidence_type();
    test_six_eight_and_other_meters();
    test_windows_do_not_overlap();
    test_bad_calls();
    printf("test_analysis_hemiola (standard-meter): ok\n");
    return 0;
}
